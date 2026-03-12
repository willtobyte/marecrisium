#include "websocket.hpp"

namespace {
socketconn* connection{nullptr};

[[nodiscard]] int abs_index(lua_State* state, int index) noexcept {
  return (index > 0 || index <= LUA_REGISTRYINDEX)
    ? index
    : lua_gettop(state) + index + 1;
}

void yyjson_to_lua(lua_State* state, yyjson_val* val) {
  const auto type = yyjson_get_type(val);

  switch (type) {
    case YYJSON_TYPE_NULL: {
      lua_pushnil(state);
    } break;

    case YYJSON_TYPE_BOOL: {
      lua_pushboolean(state, yyjson_get_bool(val) ? 1 : 0);
    } break;

    case YYJSON_TYPE_NUM: {
      if (yyjson_is_int(val)) [[likely]]
        lua_pushinteger(state, static_cast<lua_Integer>(yyjson_get_int(val)));
      else if (yyjson_is_uint(val))
        lua_pushinteger(state, static_cast<lua_Integer>(yyjson_get_uint(val)));
      else
        lua_pushnumber(state, yyjson_get_real(val));
    } break;

    case YYJSON_TYPE_STR: {
      lua_pushstring(state, yyjson_get_str(val));
    } break;

    case YYJSON_TYPE_ARR: {
      lua_createtable(state, static_cast<int>(yyjson_arr_size(val)), 0);
      auto idx = 1;
      yyjson_arr_iter iter;
      yyjson_arr_iter_init(val, &iter);
      yyjson_val* elem;
      while ((elem = yyjson_arr_iter_next(&iter))) {
        yyjson_to_lua(state, elem);
        lua_rawseti(state, -2, idx++);
      }
    } break;

    case YYJSON_TYPE_OBJ: {
      lua_createtable(state, 0, static_cast<int>(yyjson_obj_size(val)));
      yyjson_obj_iter iter;
      yyjson_obj_iter_init(val, &iter);
      yyjson_val* key;
      while ((key = yyjson_obj_iter_next(&iter))) {
        lua_pushstring(state, yyjson_get_str(key));
        yyjson_to_lua(state, yyjson_obj_iter_get_val(key));
        lua_rawset(state, -3);
      }
    } break;

    default: [[unlikely]] {
      lua_pushnil(state);
    } break;
  }
}

[[nodiscard]] bool lua_table_is_array(lua_State* state, int idx) {
  const auto abs = abs_index(state, idx);
  auto count = 0;
  lua_pushnil(state);
  while (lua_next(state, abs) != 0) {
    lua_pop(state, 1);
    ++count;
  }

  const auto len = static_cast<int>(lua_objlen(state, abs));
  return len > 0 && len == count;
}

[[nodiscard]] yyjson_mut_val* lua_to_yyjson(lua_State* state, int index, yyjson_mut_doc* document) {
  const auto abs = abs_index(state, index);
  const auto type = lua_type(state, abs);

  switch (type) {
    case LUA_TNIL: {
      return yyjson_mut_null(document);
    }

    case LUA_TBOOLEAN: {
      return yyjson_mut_bool(document, lua_toboolean(state, abs) != 0);
    }

    case LUA_TNUMBER: {
      return yyjson_mut_real(document, lua_tonumber(state, abs));
    }

    case LUA_TSTRING: {
      return yyjson_mut_str(document, lua_tostring(state, abs));
    }

    case LUA_TTABLE: {
      if (lua_table_is_array(state, abs)) [[likely]] {
        auto* arr = yyjson_mut_arr(document);
        const auto len = static_cast<int>(lua_objlen(state, abs));
        for (auto i = 1; i <= len; ++i) {
          lua_rawgeti(state, abs, i);
          yyjson_mut_arr_append(arr, lua_to_yyjson(state, -1, document));
          lua_pop(state, 1);
        }
        return arr;
      }

      auto* obj = yyjson_mut_obj(document);
      lua_pushnil(state);
      while (lua_next(state, abs) != 0) {
        if (lua_type(state, -2) == LUA_TSTRING) {
          auto* key = yyjson_mut_str(document, lua_tostring(state, -2));
          auto* val = lua_to_yyjson(state, -1, document);
          yyjson_mut_obj_add(obj, key, val);
        }
        lua_pop(state, 1);
      }
      return obj;
    }

    default: [[unlikely]] {
      return yyjson_mut_null(document);
    }
  }
}

[[nodiscard]] std::string build_action_json(const char* action, const char* topic) {
  auto* document = yyjson_mut_doc_new(nullptr);
  auto* root = yyjson_mut_obj(document);
  yyjson_mut_doc_set_root(document, root);
  yyjson_mut_obj_add_str(document, root, "action", action);
  yyjson_mut_obj_add_str(document, root, "topic", topic);
  auto* json = yyjson_mut_write(document, 0, nullptr);
  std::string result(json);
  free(json);
  yyjson_mut_doc_free(document);
  return result;
}

int subscription_publish(lua_State* state) {
  auto** pointer = static_cast<subscription**>(luaL_checkudata(state, 1, "Subscription"));
  luaL_checktype(state, 2, LUA_TTABLE);
  (*pointer)->publish(state, 2);
  return 0;
}

int subscription_unsubscribe(lua_State* state) {
  auto** pointer = static_cast<subscription**>(luaL_checkudata(state, 1, "Subscription"));
  (*pointer)->unsubscribe();
  return 0;
}

int subscription_index(lua_State* state) {
  luaL_checkudata(state, 1, "Subscription");
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "publish") {
    lua_pushcfunction(state, subscription_publish);
    return 1;
  }

  if (key == "unsubscribe") {
    lua_pushcfunction(state, subscription_unsubscribe);
    return 1;
  }

  if (key == "topic") {
    auto** pointer = static_cast<subscription**>(lua_touserdata(state, 1));
    lua_pushstring(state, (*pointer)->topic().c_str());
    return 1;
  }

  return lua_pushnil(state), 1;
}

int subscription_gc(lua_State* state) {
  auto** pointer = static_cast<subscription**>(luaL_checkudata(state, 1, "Subscription"));
  if (*pointer) {
    (*pointer)->unsubscribe();
    delete *pointer;
    *pointer = nullptr;
  }
  return 0;
}

int websocket_subscribe(lua_State* state) {
  auto** ws_ptr = static_cast<socketconn**>(luaL_checkudata(state, 1, "WebSocket"));
  const auto* const topic = luaL_checkstring(state, 2);
  luaL_checktype(state, 3, LUA_TFUNCTION);

  lua_pushvalue(state, 3);
  const auto ref = luaL_ref(state, LUA_REGISTRYINDEX);

  auto* instance = new subscription(*ws_ptr, std::string(topic), ref);

  auto** udata = static_cast<subscription**>(lua_newuserdata(state, sizeof(subscription*)));
  *udata = instance;

  if (luaL_newmetatable(state, "Subscription")) {
    lua_pushcfunction(state, subscription_index);
    lua_setfield(state, -2, "__index");

    lua_pushcfunction(state, subscription_gc);
    lua_setfield(state, -2, "__gc");
  }

  lua_setmetatable(state, -2);
  return 1;
}

int websocket_index(lua_State* state) {
  luaL_checkudata(state, 1, "WebSocket");
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "subscribe") {
    lua_pushcfunction(state, websocket_subscribe);
    return 1;
  }

  return lua_pushnil(state), 1;
}

int websocket_gc(lua_State* state) {
  auto** pointer = static_cast<socketconn**>(luaL_checkudata(state, 1, "WebSocket"));
  if (*pointer == connection) {
    delete connection;
    connection = nullptr;
  }
  *pointer = nullptr;
  return 0;
}

int websocket_call(lua_State* state) {
  const auto* const url = luaL_checkstring(state, 1);

  delete connection;
  connection = new socketconn(std::string(url));

  auto** udata = static_cast<socketconn**>(lua_newuserdata(state, sizeof(socketconn*)));
  *udata = connection;

  if (luaL_newmetatable(state, "WebSocket")) {
    lua_pushcfunction(state, websocket_index);
    lua_setfield(state, -2, "__index");

    lua_pushcfunction(state, websocket_gc);
    lua_setfield(state, -2, "__gc");
  }

  lua_setmetatable(state, -2);
  return 1;
}
}

netloc::netloc(std::string_view url) {
  if (url.starts_with("wss://")) [[likely]] {
    ssl = true;
    port = 443;
    url.remove_prefix(6);
  } else if (url.starts_with("ws://")) {
    ssl = false;
    port = 80;
    url.remove_prefix(5);
  } else if (url.starts_with("https://")) {
    ssl = true;
    port = 443;
    url.remove_prefix(8);
  } else if (url.starts_with("http://")) [[unlikely]] {
    ssl = false;
    port = 80;
    url.remove_prefix(7);
  }

  const auto slash = url.find('/');
  const auto host_part = url.substr(0, slash);
  path = (slash != std::string_view::npos) ? std::string(url.substr(slash)) : "/";

  const auto colon = host_part.find(':');
  if (colon != std::string_view::npos) [[unlikely]] {
    host = std::string(host_part.substr(0, colon));
    const auto digits = host_part.substr(colon + 1);
    std::from_chars(digits.data(), digits.data() + digits.size(), port);
  } else {
    host = std::string(host_part);
  }
}

int lws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* /*user*/, void* in, size_t len) {
  auto* context = lws_get_context(wsi);
  auto* ws = static_cast<socketconn*>(lws_context_user(context));

  switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED: {
      ws->_connected.store(true, std::memory_order_release);
      ws->resubscribe();
      lws_callback_on_writable(wsi);
    } break;

    case LWS_CALLBACK_CLIENT_RECEIVE: [[likely]] {
      const auto* const text = static_cast<const char*>(in);
      auto* document = yyjson_read(text, len, 0);
      if (!document) [[unlikely]]
        break;

      auto* root = yyjson_doc_get_root(document);
      auto* topic_value = yyjson_obj_get(root, "topic");
      if (!topic_value) [[unlikely]] {
        yyjson_doc_free(document);
        break;
      }

      const auto* const topic_string = yyjson_get_str(topic_value);
      if (!topic_string) [[unlikely]] {
        yyjson_doc_free(document);
        break;
      }

      ws->_inbound.push(message{
        std::string(topic_string),
        std::string(text, len)
      });

      yyjson_doc_free(document);
    } break;

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
      struct message message;
      while (ws->_outbound.try_pop(message)) {
        const auto& payload = message.payload;
        const auto size = payload.size();
        const auto required = LWS_PRE + size;
        if (ws->_sendbuffer.size() < required) [[unlikely]]
          ws->_sendbuffer.resize(required);
        std::memcpy(ws->_sendbuffer.data() + LWS_PRE, payload.data(), size);
        lws_write(wsi, ws->_sendbuffer.data() + LWS_PRE, size, LWS_WRITE_TEXT);
      }
    } break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    case LWS_CALLBACK_CLIENT_CLOSED: [[unlikely]] {
      ws->_connected.store(false, std::memory_order_release);
      ws->_wsi.store(nullptr, std::memory_order_release);
    } break;

    default:
      break;
  }

  return 0;
}

namespace {

static const struct lws_protocols protocols[] = {
  {"carimbo", lws_callback, 0, 4096, 0, nullptr, 0},
  {nullptr, nullptr, 0, 0, 0, nullptr, 0}
};
}

socketconn::socketconn(std::string url)
  : _url(std::move(url)), _netloc(_url) {
  _sendbuffer.reserve(LWS_PRE + 4096);
  _thread = std::thread([this] { run(); });
}

socketconn::~socketconn() {
  for (auto& [topic, subscribers] : _subscriptions) {
    for (auto* subscriber : subscribers) {
      luaL_unref(L, LUA_REGISTRYINDEX, subscriber->_callback);
      subscriber->_active = false;
      subscriber->_callback = LUA_NOREF;
      subscriber->_owner = nullptr;
    }
  }
  _subscriptions.clear();

  _stop.store(true, std::memory_order_release);

  if (_context) [[likely]]
    lws_cancel_service(_context);

  if (_thread.joinable()) [[likely]]
    _thread.join();
}

void socketconn::connect() {
  struct lws_context_creation_info info{};
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.user = this;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;

  _context = lws_create_context(&info);
}

void socketconn::reconnect() {
  struct lws_client_connect_info ccinfo{};
  ccinfo.context = _context;
  ccinfo.address = _netloc.host.c_str();
  ccinfo.port = _netloc.port;
  ccinfo.path = _netloc.path.c_str();
  ccinfo.host = _netloc.host.c_str();
  ccinfo.origin = _netloc.host.c_str();
  ccinfo.protocol = protocols[0].name;
  ccinfo.ssl_connection = _netloc .ssl ? LCCSCF_USE_SSL : 0;

  _wsi.store(lws_client_connect_via_info(&ccinfo), std::memory_order_release);
}

void socketconn::run() {
  connect();
  reconnect();

  while (!_stop.load(std::memory_order_acquire)) {
    if (!_context) [[unlikely]] {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      connect();
      reconnect();
      continue;
    }

    if (!_wsi.load(std::memory_order_acquire) && !_connected.load(std::memory_order_acquire)) [[unlikely]] {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      reconnect();
    }

    lws_service(_context, 100);

    auto* current_wsi = _wsi.load(std::memory_order_acquire);
    if (_connected.load(std::memory_order_acquire) && current_wsi) [[likely]]
      lws_callback_on_writable(current_wsi);
  }

  if (_context) {
    lws_context_destroy(_context);
    _context = nullptr;
  }
}

void socketconn::send(message message) noexcept {
  _outbound.push(std::move(message));
  if (_context) [[likely]]
    lws_cancel_service(_context);
}

void socketconn::poll() {
  message message;
  while (_inbound.try_pop(message)) {
    auto* document = yyjson_read(message.payload.c_str(), message.payload.size(), 0);
    if (!document) [[unlikely]]
      continue;

    auto* root = yyjson_doc_get_root(document);
    auto* data_value = yyjson_obj_get(root, "data");

    const auto it = _subscriptions.find(message.topic);
    if (it != _subscriptions.end()) [[likely]] {
      for (auto* subscriber : it->second) {
        if (!subscriber->active()) [[unlikely]]
          continue;

        const auto ref = subscriber->callback();
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

        if (data_value)
          yyjson_to_lua(L, data_value);
        else
          lua_pushnil(L);

        if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
          std::string error = lua_tostring(L, -1);
          lua_pop(L, 1);
          yyjson_doc_free(document);
          throw std::runtime_error(std::move(error));
        }
      }
    }

    yyjson_doc_free(document);
  }
}

void socketconn::add_subscription(subscription* subscriber) {
  _subscriptions[subscriber->topic()].emplace_back(subscriber);
  auto payload = build_action_json("subscribe", subscriber->topic().c_str());
  send(message{subscriber->topic(), std::move(payload)});
}

void socketconn::remove_subscription(subscription* subscriber) {
  auto payload = build_action_json("unsubscribe", subscriber->topic().c_str());
  send(message{subscriber->topic(), std::move(payload)});

  auto it = _subscriptions.find(subscriber->topic());
  if (it != _subscriptions.end()) [[likely]] {
    std::erase(it->second, subscriber);
    if (it->second.empty())
      _subscriptions.erase(it);
  }
}

void socketconn::resubscribe() {
  for (const auto& [topic, subscribers] : _subscriptions) {
    for (const auto* subscriber : subscribers) {
      if (!subscriber->active()) [[unlikely]]
        continue;
      auto payload = build_action_json("subscribe", topic.c_str());
      _outbound.push(message{topic, std::move(payload)});
    }
  }
}

subscription::subscription(socketconn* owner, std::string topic, int callback_ref)
  : _owner(owner), _topic(std::move(topic)), _callback(callback_ref) {
  _owner->add_subscription(this);
}

subscription::~subscription() {
  unsubscribe();
}

void subscription::publish(lua_State* state, int idx) {
  if (!_active || !_owner) [[unlikely]]
    return;

  auto* document = yyjson_mut_doc_new(nullptr);
  auto* root = yyjson_mut_obj(document);
  yyjson_mut_doc_set_root(document, root);
  yyjson_mut_obj_add_str(document, root, "action", "publish");
  yyjson_mut_obj_add_str(document, root, "topic", _topic.c_str());
  yyjson_mut_obj_add_val(document, root, "data", lua_to_yyjson(state, idx, document));

  auto* json = yyjson_mut_write(document, 0, nullptr);
  _owner->send(message{_topic, std::string(json)});

  free(json);
  yyjson_mut_doc_free(document);
}

void subscription::unsubscribe() {
  if (!_active) [[unlikely]] return;
  _active = false;

  if (_owner) [[likely]]
    _owner->remove_subscription(this);

  luaL_unref(L, LUA_REGISTRYINDEX, _callback);
  _callback = LUA_NOREF;
}

const std::string& subscription::topic() const noexcept {
  return _topic;
}

int subscription::callback() const noexcept {
  return _callback;
}

bool subscription::active() const noexcept {
  return _active;
}

void websocket::wire() {
  lua_newtable(L);
  lua_pushcfunction(L, websocket_call);
  lua_setfield(L, -2, "new");
  lua_setglobal(L, "WebSocket");
}

void websocket::poll() {
  if (connection) [[likely]]
    connection->poll();
}
