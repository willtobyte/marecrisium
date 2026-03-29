#if 0

#include "websocket.hpp"

namespace {
std::unique_ptr<channel> connection;

[[nodiscard]] std::vector<uint8_t> envelope(yyjson_mut_doc *doc) {
  size_t len = 0;
  auto *json = yyjson_mut_write(doc, 0, &len);
  yyjson_mut_doc_free(doc);
  if (!json) [[unlikely]]
    return {};

  std::vector<uint8_t> result(json, json + len);
  free(json);
  return result;
}

[[nodiscard]] std::vector<uint8_t> subscribe(uint16_t topic) {
  auto *doc = yyjson_mut_doc_new(nullptr);
  auto *arr = yyjson_mut_arr(doc);
  yyjson_mut_arr_add_uint(doc, arr, std::to_underlying(opcode::subscribe));
  yyjson_mut_arr_add_uint(doc, arr, topic);
  yyjson_mut_doc_set_root(doc, arr);
  return envelope(doc);
}

[[nodiscard]] std::vector<uint8_t> unsubscribe(uint16_t topic) {
  auto *doc = yyjson_mut_doc_new(nullptr);
  auto *arr = yyjson_mut_arr(doc);
  yyjson_mut_arr_add_uint(doc, arr, std::to_underlying(opcode::unsubscribe));
  yyjson_mut_arr_add_uint(doc, arr, topic);
  yyjson_mut_doc_set_root(doc, arr);
  return envelope(doc);
}

[[nodiscard]] std::vector<uint8_t> publish(uint16_t topic, yyjson_mut_val *data, yyjson_mut_doc *doc) {
  auto *arr = yyjson_mut_arr(doc);
  yyjson_mut_arr_add_uint(doc, arr, std::to_underlying(opcode::publish));
  yyjson_mut_arr_add_uint(doc, arr, topic);
  yyjson_mut_arr_append(arr, data);
  yyjson_mut_doc_set_root(doc, arr);
  return envelope(doc);
}

int subscription_publish(lua_State *state) {
  auto *self = *static_cast<subscription **>(luaL_checkudata(state, 1, "Subscription"));
  luaL_checkany(state, 2);
  self->publish(state, 2);
  return 0;
}

int subscription_unsubscribe(lua_State *state) {
  auto *self = *static_cast<subscription **>(luaL_checkudata(state, 1, "Subscription"));
  self->unsubscribe();
  return 0;
}

int subscription_index(lua_State *state) {
  luaL_checkudata(state, 1, "Subscription");
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "publish") {
    lua_pushcfunction(state, subscription_publish);
    return 1;
  }

  if (key == "unsubscribe") {
    lua_pushcfunction(state, subscription_unsubscribe);
    return 1;
  }

  if (key == "topic") {
    auto *self = *static_cast<subscription **>(luaL_checkudata(state, 1, "Subscription"));
    lua_pushinteger(state, static_cast<lua_Integer>(self->topic()));
    return 1;
  }

  return lua_pushnil(state), 1;
}

int subscription_gc(lua_State *state) {
  delete *static_cast<subscription **>(luaL_checkudata(state, 1, "Subscription"));
  return 0;
}

int websocket_on_connect(lua_State *state) {
  auto *instance = *static_cast<channel **>(luaL_checkudata(state, 1, "WebSocket"));
  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  instance->set_on_connect(luaL_ref(state, LUA_REGISTRYINDEX));
  return 0;
}

int websocket_on_disconnect(lua_State *state) {
  auto *instance = *static_cast<channel **>(luaL_checkudata(state, 1, "WebSocket"));
  luaL_checktype(state, 2, LUA_TFUNCTION);
  lua_pushvalue(state, 2);
  instance->set_on_disconnect(luaL_ref(state, LUA_REGISTRYINDEX));
  return 0;
}

int websocket_subscribe(lua_State *state) {
  auto *instance = *static_cast<channel **>(luaL_checkudata(state, 1, "WebSocket"));
  const auto raw = static_cast<int>(luaL_checkinteger(state, 2));
  luaL_argcheck(state, raw >= 0 && raw <= std::numeric_limits<uint16_t>::max(), 2, "topic must be 0..65535");
  const auto topic = static_cast<uint16_t>(raw);
  luaL_checktype(state, 3, LUA_TFUNCTION);
  lua_pushvalue(state, 3);
  auto reference = luaL_ref(state, LUA_REGISTRYINDEX);

  subscription *sub = nullptr;
  try {
    sub = new subscription(instance, topic, reference);
  } catch (...) {
    luaL_unref(state, LUA_REGISTRYINDEX, reference);
    reference = LUA_NOREF;
    throw;
  }

  auto **m = static_cast<subscription **>(lua_newuserdata(state, sizeof(subscription *)));
  *m = sub;
  luaL_getmetatable(state, "Subscription");
  lua_setmetatable(state, -2);
  return 1;
}

int websocket_index(lua_State *state) {
  luaL_checkudata(state, 1, "WebSocket");
  const auto key = std::string_view{luaL_checkstring(state, 2)};

  if (key == "subscribe") {
    lua_pushcfunction(state, websocket_subscribe);
    return 1;
  }

  if (key == "on_connect") {
    lua_pushcfunction(state, websocket_on_connect);
    return 1;
  }

  if (key == "on_disconnect") {
    lua_pushcfunction(state, websocket_on_disconnect);
    return 1;
  }

  return lua_pushnil(state), 1;
}

int websocket_gc(lua_State *state) {
  auto **pointer = static_cast<channel **>(luaL_checkudata(state, 1, "WebSocket"));
  if (*pointer == connection.get())
    connection.reset();
  *pointer = nullptr;
  return 0;
}

int websocket_call(lua_State *state) {
  auto url = std::string{luaL_checkstring(state, 1)};

  connection = std::make_unique<channel>(std::move(url));

  auto **m = static_cast<channel **>(lua_newuserdata(state, sizeof(channel *)));
  *m = connection.get();
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

int lws_callback(struct lws *wsi, enum lws_callback_reasons reason, void * /*user*/, void *in, size_t length) {
  auto *context = lws_get_context(wsi);
  auto *ws = static_cast<channel *>(lws_context_user(context));

  switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      ws->_connected.store(true, std::memory_order_release);
      ws->_pending_connect.store(true, std::memory_order_release);
      ws->resubscribe();
      lws_set_timer_usecs(wsi, 15 * LWS_USEC_PER_SEC);
      lws_callback_on_writable(wsi);
      break;

    case LWS_CALLBACK_TIMER:
      ws->_pending_ping.store(true, std::memory_order_release);
      lws_set_timer_usecs(wsi, 15 * LWS_USEC_PER_SEC);
      lws_callback_on_writable(wsi);
      break;

    case LWS_CALLBACK_CLIENT_RECEIVE: [[likely]] {
      const auto *bytes = static_cast<const char *>(in);
      auto *doc = yyjson_read(bytes, length, 0);
      if (!doc) [[unlikely]]
        break;

      auto *root = yyjson_doc_get_root(doc);
      if (!yyjson_is_arr(root) || yyjson_arr_size(root) < 2) [[unlikely]] {
        yyjson_doc_free(doc);
        break;
      }

      auto *topic_val = yyjson_arr_get(root, 1);
      if (!yyjson_is_int(topic_val)) [[unlikely]] {
        yyjson_doc_free(doc);
        break;
      }

      const auto topic = static_cast<uint16_t>(yyjson_get_int(topic_val));

      ws->_inbound.push(message{
        topic,
        std::vector<uint8_t>(reinterpret_cast<const uint8_t *>(bytes), reinterpret_cast<const uint8_t *>(bytes) + length)
      });

      yyjson_doc_free(doc);
    } break;

    case LWS_CALLBACK_CLIENT_WRITEABLE: {
      struct message message;
      if (ws->_outbound.try_pop(message)) {
        const auto &payload = message.payload;
        const auto size = payload.size();
        const auto required = LWS_PRE + size;
        if (ws->_sendbuffer.size() < required) [[unlikely]]
          ws->_sendbuffer.resize(required);
        std::memcpy(ws->_sendbuffer.data() + LWS_PRE, payload.data(), size);
        lws_write(wsi, ws->_sendbuffer.data() + LWS_PRE, size, LWS_WRITE_TEXT);
        lws_callback_on_writable(wsi);
      } else if (ws->_pending_ping.exchange(false, std::memory_order_acq_rel)) {
        if (ws->_sendbuffer.size() < LWS_PRE) [[unlikely]]
          ws->_sendbuffer.resize(LWS_PRE);
        lws_write(wsi, ws->_sendbuffer.data() + LWS_PRE, 0, LWS_WRITE_PING);
      }
    } break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
    case LWS_CALLBACK_CLIENT_CLOSED: [[unlikely]]
      ws->_connected.store(false, std::memory_order_release);
      ws->_wsi.store(nullptr, std::memory_order_release);
      ws->_pending_disconnect.store(true, std::memory_order_release);
      break;

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

channel::channel(std::string url)
  : _url(std::move(url)), _netloc(_url) {
  lws_set_log_level(0, nullptr);
  _sendbuffer.reserve(LWS_PRE + 4096);
  _thread = std::thread([this] { run(); });
}

channel::~channel() {
  try {
    if (_on_disconnect != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, _on_disconnect);
      pcall(L, 0, 0);
    }
  } catch (...) {}

  luaL_unref(L, LUA_REGISTRYINDEX, _on_connect);
  _on_connect = LUA_NOREF;
  luaL_unref(L, LUA_REGISTRYINDEX, _on_disconnect);
  _on_disconnect = LUA_NOREF;

  for (auto &[topic, subscribers] : _subscriptions) {
    for (auto *subscriber : subscribers) {
      luaL_unref(L, LUA_REGISTRYINDEX, subscriber->_callback);
      subscriber->_callback = LUA_NOREF;
      subscriber->_active = false;
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

void channel::connect() {
  struct lws_context_creation_info creation {};
  creation.port = CONTEXT_PORT_NO_LISTEN;
  creation.protocols = protocols;
  creation.user = this;
  creation.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  creation.ka_time = 10;
  creation.ka_probes = 3;
  creation.ka_interval = 5;
  creation.timeout_secs = 30;

  _context = lws_create_context(&creation);
}

void channel::reconnect() {
  struct lws_client_connect_info connect_info {};
  connect_info.context = _context;
  connect_info.address = _netloc.host.c_str();
  connect_info.port = _netloc.port;
  connect_info.path = _netloc.path.c_str();
  connect_info.host = _netloc.host.c_str();
  connect_info.origin = _netloc.host.c_str();
  connect_info.protocol = protocols[0].name;
  connect_info.ssl_connection = _netloc.ssl ? LCCSCF_USE_SSL : 0;

  _wsi.store(lws_client_connect_via_info(&connect_info), std::memory_order_release);
}

void channel::run() {
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

    auto *current_wsi = _wsi.load(std::memory_order_acquire);
    if (_connected.load(std::memory_order_acquire) && current_wsi) [[likely]]
      lws_callback_on_writable(current_wsi);
  }

  if (_context) {
    lws_context_destroy(_context);
    _context = nullptr;
  }
}

void channel::send(message message) noexcept {
  _outbound.push(std::move(message));
  if (_context) [[likely]]
    lws_cancel_service(_context);
}

void channel::poll() {
  if (_pending_connect.exchange(false, std::memory_order_acq_rel)) {
    if (_on_connect != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, _on_connect);
      pcall(L, 0, 0);
    }
  }

  if (_pending_disconnect.exchange(false, std::memory_order_acq_rel)) {
    if (_on_disconnect != LUA_NOREF) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, _on_disconnect);
      pcall(L, 0, 0);
    }
  }

  message message;
  while (_inbound.try_pop(message)) {
    auto *doc = yyjson_read(reinterpret_cast<const char *>(message.payload.data()), message.payload.size(), 0);
    if (!doc) [[unlikely]]
      continue;

    auto *root = yyjson_doc_get_root(doc);
    yyjson_val *data_val = nullptr;
    if (yyjson_is_arr(root) && yyjson_arr_size(root) >= 3) [[likely]]
      data_val = yyjson_arr_get(root, 2);

    std::vector<int> references;
    {
      std::scoped_lock lock(_mutex);
      const auto it = _subscriptions.find(message.topic);
      if (it != _subscriptions.end()) [[likely]] {
        references.reserve(it->second.size());
        for (const auto *subscriber : it->second) {
          if (!subscriber->active()) [[unlikely]]
            continue;
          references.emplace_back(subscriber->callback());
        }
      }
    }

    for (const auto reference : references) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, reference);

      if (data_val)
        json_to_lua(L, data_val);
      else
        lua_pushnil(L);

      pcall(L, 1, 0);
    }

    yyjson_doc_free(doc);
  }
}

void channel::set_on_connect(int reference) noexcept {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_connect);
  _on_connect = reference;
}

void channel::set_on_disconnect(int reference) noexcept {
  luaL_unref(L, LUA_REGISTRYINDEX, _on_disconnect);
  _on_disconnect = reference;
}

void channel::add_subscription(subscription *subscriber) {
  {
    std::scoped_lock lock(_mutex);
    _subscriptions[subscriber->topic()].emplace_back(subscriber);
  }

  if (_connected.load(std::memory_order_acquire))
    send(message{subscriber->topic(), subscribe(subscriber->topic())});
}

void channel::remove_subscription(subscription *subscriber) {
  send(message{subscriber->topic(), unsubscribe(subscriber->topic())});

  std::scoped_lock lock(_mutex);
  auto it = _subscriptions.find(subscriber->topic());
  if (it != _subscriptions.end()) [[likely]] {
    std::erase(it->second, subscriber);
    if (it->second.empty())
      _subscriptions.erase(it);
  }
}

void channel::resubscribe() {
  std::scoped_lock lock(_mutex);
  for (const auto &[topic, subscribers] : _subscriptions) {
    for (const auto *subscriber : subscribers) {
      if (!subscriber->active()) [[unlikely]]
        continue;
      _outbound.push(message{topic, subscribe(topic)});
    }
  }
}

subscription::subscription(channel *owner, uint16_t topic, int callback_ref)
  : _owner(owner), _topic(topic), _callback(callback_ref) {
  _owner->add_subscription(this);
}

subscription::~subscription() {
  unsubscribe();
}

void subscription::publish(lua_State *state, int index) {
  if (!_active || !_owner) [[unlikely]]
    return;

  auto *doc = yyjson_mut_doc_new(nullptr);
  auto *data = lua_to_json(state, index, doc);
  _owner->send(message{_topic, publish(_topic, data, doc)});
}

void subscription::unsubscribe() {
  if (!_active) [[unlikely]] return;
  _active = false;

  if (_owner) [[likely]]
    _owner->remove_subscription(this);

  luaL_unref(L, LUA_REGISTRYINDEX, _callback);
  _callback = LUA_NOREF;
}

uint16_t subscription::topic() const noexcept {
  return _topic;
}

int subscription::callback() const noexcept {
  return _callback;
}

bool subscription::active() const noexcept {
  return _active;
}

void websocket::wire() {
  metatable(L, "Subscription", subscription_index, nullptr, subscription_gc);
  metatable(L, "WebSocket", websocket_index, nullptr, websocket_gc);

  lua_newtable(L);
  lua_pushcfunction(L, websocket_call);
  lua_setfield(L, -2, "new");
  lua_setglobal(L, "WebSocket");
}

void websocket::poll() {
  if (connection) [[likely]]
    connection->poll();
}

#endif
