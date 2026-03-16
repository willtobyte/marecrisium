#include "websocket.hpp"

namespace {
std::unique_ptr<socketconn> connection;

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
      auto index = 1;
      yyjson_arr_iter iter;
      yyjson_arr_iter_init(val, &iter);
      yyjson_val* elem;
      while ((elem = yyjson_arr_iter_next(&iter))) {
        yyjson_to_lua(state, elem);
        lua_rawseti(state, -2, index++);
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

[[nodiscard]] bool lua_table_is_array(lua_State* state, int index) {
  const auto abs = abs_index(state, index);
  auto count = 0;
  lua_pushnil(state);
  while (lua_next(state, abs) != 0) {
    lua_pop(state, 1);
    ++count;
  }

  const auto length = static_cast<int>(lua_objlen(state, abs));
  return length > 0 && length == count;
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
        const auto length = static_cast<int>(lua_objlen(state, abs));
        for (auto i = 1; i <= length; ++i) {
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
  auto document = std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)>(yyjson_mut_doc_new(nullptr), yyjson_mut_doc_free);
  auto* root = yyjson_mut_obj(document.get());
  yyjson_mut_doc_set_root(document.get(), root);
  yyjson_mut_obj_add_str(document.get(), root, "action", action);
  yyjson_mut_obj_add_str(document.get(), root, "topic", topic);
  auto json = std::unique_ptr<char, decltype(&free)>(yyjson_mut_write(document.get(), 0, nullptr), free);
  return std::string(json.get());
}

int subscription_publish(lua_State* state) {
  auto* ptr = static_cast<std::unique_ptr<subscription>*>(luaL_checkudata(state, 1, "Subscription"));
  luaL_checktype(state, 2, LUA_TTABLE);
  (*ptr)->publish(state, 2);
  return 0;
}

int subscription_unsubscribe(lua_State* state) {
  auto* ptr = static_cast<std::unique_ptr<subscription>*>(luaL_checkudata(state, 1, "Subscription"));
  (*ptr)->unsubscribe();
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
    auto* ptr = static_cast<std::unique_ptr<subscription>*>(lua_touserdata(state, 1));
    lua_pushstring(state, (*ptr)->topic().c_str());
    return 1;
  }

  return lua_pushnil(state), 1;
}

int subscription_gc(lua_State* state) {
  auto* ptr = static_cast<std::unique_ptr<subscription>*>(luaL_checkudata(state, 1, "Subscription"));
  std::destroy_at(ptr);
  return 0;
}

int websocket_subscribe(lua_State* state) {
  auto** ptr = static_cast<socketconn**>(luaL_checkudata(state, 1, "WebSocket"));
  const auto* const topic = luaL_checkstring(state, 2);
  luaL_checktype(state, 3, LUA_TFUNCTION);

  lua_pushvalue(state, 3);
  const auto reference = luaL_ref(state, LUA_REGISTRYINDEX);

  auto* userdata = static_cast<std::unique_ptr<subscription>*>(lua_newuserdata(state, sizeof(std::unique_ptr<subscription>)));
  std::construct_at(userdata, std::make_unique<subscription>(*ptr, topic, reference));

  luaL_getmetatable(state, "Subscription");
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
  if (*pointer == connection.get()) {
    connection.reset();
  }
  *pointer = nullptr;
  return 0;
}

int websocket_call(lua_State* state) {
  const auto* const url = luaL_checkstring(state, 1);

  connection = std::make_unique<socketconn>(url);

  auto** userdata = static_cast<socketconn**>(lua_newuserdata(state, sizeof(socketconn*)));
  *userdata = connection.get();

  luaL_getmetatable(state, "WebSocket");
  lua_setmetatable(state, -2);
  return 1;
}
}

netloc::netloc(std::string_view url) {
  const auto slash = url.find('/');
  const auto authority = url.substr(0, slash);
  path = (slash != std::string_view::npos) ? std::string(url.substr(slash)) : "/";

  const auto colon = authority.find(':');
  if (colon != std::string_view::npos) [[unlikely]] {
    host = std::string(authority.substr(0, colon));
    const auto digits = authority.substr(colon + 1);
    std::from_chars(digits.data(), digits.data() + digits.size(), port);
    ssl = false;
  } else {
    host = authority;
  }
}

int lws_callback(struct lws* wsi, enum lws_callback_reasons reason, void* /*user*/, void* in, size_t length) {
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
      auto document = std::unique_ptr<yyjson_doc, YYJSON_Deleter>(yyjson_read(text, length, 0), YYJSON_Deleter{});
      if (!document) [[unlikely]]
        break;

      auto* root = yyjson_doc_get_root(document.get());
      auto* topic_value = yyjson_obj_get(root, "topic");
      if (!topic_value) [[unlikely]]
        break;

      const auto* const topic_string = yyjson_get_str(topic_value);
      if (!topic_string) [[unlikely]]
        break;

      ws->_inbound.push(message{
        topic_string,
        std::string(text, length)
      });
    } break;

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
      struct message message;
      if (ws->_outbound.try_pop(message)) {
        const auto& payload = message.payload;
        const auto size = payload.size();
        const auto required = LWS_PRE + size;
        if (ws->_sendbuffer.size() < required) [[unlikely]]
          ws->_sendbuffer.resize(required);
        std::memcpy(ws->_sendbuffer.data() + LWS_PRE, payload.data(), size);
        lws_write(wsi, ws->_sendbuffer.data() + LWS_PRE, size, LWS_WRITE_TEXT);
        lws_callback_on_writable(wsi);
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

const struct lws_protocols protocols[] = {
  {"carimbo", lws_callback, 0, 4096, 0, nullptr, 0},
  {nullptr, nullptr, 0, 0, 0, nullptr, 0}
};
}

socketconn::socketconn(std::string_view url)
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
  ccinfo.ssl_connection = _netloc.ssl ? LCCSCF_USE_SSL : 0;

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
    auto document = std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)>(yyjson_read(message.payload.c_str(), message.payload.size(), 0), yyjson_doc_free);
    if (!document) [[unlikely]]
      continue;

    auto* root = yyjson_doc_get_root(document.get());
    auto* data_value = yyjson_obj_get(root, "data");

    std::vector<int> references;
    {
      std::scoped_lock lock(_mutex);
      const auto it = _subscriptions.find(message.topic);
      if (it != _subscriptions.end()) [[likely]] {
        references.reserve(it->second.size());
        for (const auto* subscriber : it->second) {
          if (!subscriber->active()) [[unlikely]]
            continue;
          references.emplace_back(subscriber->callback());
        }
      }
    }

    for (const auto reference : references) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, reference);

      if (data_value)
        yyjson_to_lua(L, data_value);
      else
        lua_pushnil(L);

      if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
        std::string error(lua_tostring(L, -1));
        lua_pop(L, 1);
        throw std::runtime_error(std::move(error));
      }
    }
  }
}

void socketconn::add_subscription(subscription* subscriber) {
  {
    std::scoped_lock lock(_mutex);
    _subscriptions[subscriber->topic()].emplace_back(subscriber);
  }

  if (_connected.load(std::memory_order_acquire)) {
    auto payload = build_action_json("subscribe", subscriber->topic().c_str());
    send(message{subscriber->topic(), std::move(payload)});
  }
}

void socketconn::remove_subscription(subscription* subscriber) {
  auto payload = build_action_json("unsubscribe", subscriber->topic().c_str());
  send(message{subscriber->topic(), std::move(payload)});

  std::scoped_lock lock(_mutex);
  auto it = _subscriptions.find(subscriber->topic());
  if (it != _subscriptions.end()) [[likely]] {
    std::erase(it->second, subscriber);
    if (it->second.empty())
      _subscriptions.erase(it);
  }
}

void socketconn::resubscribe() {
  std::scoped_lock lock(_mutex);
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

void subscription::publish(lua_State* state, int index) {
  if (!_active || !_owner) [[unlikely]]
    return;

  auto document = std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)>(yyjson_mut_doc_new(nullptr), yyjson_mut_doc_free);
  auto* root = yyjson_mut_obj(document.get());
  yyjson_mut_doc_set_root(document.get(), root);
  yyjson_mut_obj_add_str(document.get(), root, "action", "publish");
  yyjson_mut_obj_add_str(document.get(), root, "topic", _topic.c_str());
  yyjson_mut_obj_add_val(document.get(), root, "data", lua_to_yyjson(state, index, document.get()));

  auto json = std::unique_ptr<char, decltype(&free)>(yyjson_mut_write(document.get(), 0, nullptr), free);
  _owner->send(message{_topic, std::string(json.get())});
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
  luaL_newmetatable(L, "Subscription");
  lua_pushcfunction(L, subscription_index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, subscription_gc);
  lua_setfield(L, -2, "__gc");
  lua_pop(L, 1);

  luaL_newmetatable(L, "WebSocket");
  lua_pushcfunction(L, websocket_index);
  lua_setfield(L, -2, "__index");
  lua_pushcfunction(L, websocket_gc);
  lua_setfield(L, -2, "__gc");
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushcfunction(L, websocket_call);
  lua_setfield(L, -2, "new");
  lua_setglobal(L, "WebSocket");
}

void websocket::poll() {
  if (connection) [[likely]]
    connection->poll();
}
