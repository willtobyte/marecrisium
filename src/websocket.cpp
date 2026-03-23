#include "websocket.hpp"

namespace {
std::unique_ptr<channel> connection;

[[nodiscard]] int abs_index(lua_State *state, int index) noexcept {
  return (index > 0 || index <= LUA_REGISTRYINDEX)
    ? index
    : lua_gettop(state) + index + 1;
}

void cbor_to_lua(lua_State *state, cbor_item_t *item) {
  if (cbor_is_null(item) || cbor_is_undef(item)) {
    lua_pushnil(state);
    return;
  }

  if (cbor_is_bool(item)) {
    lua_pushboolean(state, cbor_get_bool(item) ? 1 : 0);
    return;
  }

  if (cbor_isa_uint(item)) [[likely]] {
    lua_pushinteger(state, static_cast<lua_Integer>(cbor_get_int(item)));
    return;
  }

  if (cbor_isa_negint(item)) {
    lua_pushinteger(state, -1 - static_cast<lua_Integer>(cbor_get_int(item)));
    return;
  }

  if (cbor_is_float(item)) {
    lua_pushnumber(state, cbor_float_get_float(item));
    return;
  }

  if (cbor_isa_string(item)) {
    lua_pushlstring(state,
      reinterpret_cast<const char *>(cbor_string_handle(item)),
      cbor_string_length(item));
    return;
  }

  if (cbor_isa_array(item)) {
    const auto size = cbor_array_size(item);
    lua_createtable(state, static_cast<int>(size), 0);
    auto *elements = cbor_array_handle(item);
    for (size_t i = 0; i < size; ++i) {
      cbor_to_lua(state, elements[i]);
      lua_rawseti(state, -2, static_cast<int>(i + 1));
    }
    return;
  }

  if (cbor_isa_map(item)) {
    const auto size = cbor_map_size(item);
    lua_createtable(state, 0, static_cast<int>(size));
    auto *pairs = cbor_map_handle(item);
    for (size_t i = 0; i < size; ++i) {
      cbor_to_lua(state, pairs[i].key);
      cbor_to_lua(state, pairs[i].value);
      lua_rawset(state, -3);
    }
    return;
  }

  lua_pushnil(state);
}

[[nodiscard]] bool lua_table_is_array(lua_State *state, int index) {
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

[[nodiscard]] cbor_item_t *lua_to_cbor(lua_State *state, int index) {
  const auto abs = abs_index(state, index);
  const auto type = lua_type(state, abs);

  switch (type) {
    case LUA_TNIL:
      return cbor_new_null();

    case LUA_TBOOLEAN:
      return cbor_build_bool(lua_toboolean(state, abs) != 0);

    case LUA_TNUMBER: {
      const auto value = lua_tonumber(state, abs);
      const auto as_int = static_cast<lua_Integer>(value);
      if (static_cast<lua_Number>(as_int) == value) {
        if (as_int >= 0)
          return cbor_build_uint64(static_cast<uint64_t>(as_int));
        return cbor_build_negint64(static_cast<uint64_t>(-1 - as_int));
      }
      return cbor_build_float8(value);
    }

    case LUA_TSTRING: {
      size_t len = 0;
      const auto *str = lua_tolstring(state, abs, &len);
      return cbor_build_stringn(str, len);
    }

    case LUA_TTABLE: {
      if (lua_table_is_array(state, abs)) [[likely]] {
        const auto length = static_cast<int>(lua_objlen(state, abs));
        auto *arr = cbor_new_definite_array(static_cast<size_t>(length));
        for (auto i = 1; i <= length; ++i) {
          lua_rawgeti(state, abs, i);
          std::ignore = cbor_array_push(arr, cbor_move(lua_to_cbor(state, -1)));
          lua_pop(state, 1);
        }
        return arr;
      }

      auto *map = cbor_new_definite_map(0);
      lua_pushnil(state);
      while (lua_next(state, abs) != 0) {
        if (lua_type(state, -2) == LUA_TSTRING) {
          auto *key = lua_to_cbor(state, -2);
          auto *val = lua_to_cbor(state, -1);
          std::ignore = cbor_map_add(map, {cbor_move(key), cbor_move(val)});
        }
        lua_pop(state, 1);
      }
      return map;
    }

    default: [[unlikely]]
      return cbor_new_null();
  }
}

[[nodiscard]] std::vector<uint8_t> serialize(cbor_item_t *item) {
  unsigned char *buffer = nullptr;
  size_t length = 0;
  cbor_serialize_alloc(item, &buffer, &length);
  cbor_decref(&item);
  std::vector<uint8_t> result(buffer, buffer + length);
  free(buffer);
  return result;
}

[[nodiscard]] std::vector<uint8_t> subscribe(uint16_t topic) {
  auto *arr = cbor_new_definite_array(2);
  std::ignore = cbor_array_push(arr, cbor_move(cbor_build_uint8(std::to_underlying(opcode::subscribe))));
  std::ignore = cbor_array_push(arr, cbor_move(cbor_build_uint16(topic)));
  return serialize(arr);
}

[[nodiscard]] std::vector<uint8_t> unsubscribe(uint16_t topic) {
  auto *arr = cbor_new_definite_array(2);
  std::ignore = cbor_array_push(arr, cbor_move(cbor_build_uint8(std::to_underlying(opcode::unsubscribe))));
  std::ignore = cbor_array_push(arr, cbor_move(cbor_build_uint16(topic)));
  return serialize(arr);
}

[[nodiscard]] std::vector<uint8_t> publish(uint16_t topic, cbor_item_t *data) {
  auto *arr = cbor_new_definite_array(3);
  std::ignore = cbor_array_push(arr, cbor_move(cbor_build_uint8(std::to_underlying(opcode::publish))));
  std::ignore = cbor_array_push(arr, cbor_move(cbor_build_uint16(topic)));
  std::ignore = cbor_array_push(arr, cbor_move(data));
  return serialize(arr);
}

int subscription_publish(lua_State* state) {
  auto *self = argument<subscription>(state, 1, "Subscription");
  luaL_checkany(state, 2);
  self->publish(state, 2);
  return 0;
}

int subscription_unsubscribe(lua_State* state) {
  auto *self = argument<subscription>(state, 1, "Subscription");
  self->unsubscribe();
  return 0;
}

int subscription_index(lua_State* state) {
  argument<void>(state, 1, "Subscription");
  const auto key = argument<std::string_view>(state, 2);

  if (key == "publish") {
    lua_pushcfunction(state, subscription_publish);
    return 1;
  }

  if (key == "unsubscribe") {
    lua_pushcfunction(state, subscription_unsubscribe);
    return 1;
  }

  if (key == "topic") {
    auto *self = argument<subscription>(state, 1, "Subscription");
    lua_pushinteger(state, self->topic());
    return 1;
  }

  return lua_pushnil(state), 1;
}

int subscription_gc(lua_State* state) {
  delete argument<subscription>(state, 1, "Subscription");
  return 0;
}

int websocket_on_connect(lua_State* state) {
  auto* instance = argument<channel>(state, 1, "WebSocket");
  instance->set_on_connect(capture(state, 2));
  return 0;
}

int websocket_on_disconnect(lua_State* state) {
  auto* instance = argument<channel>(state, 1, "WebSocket");
  instance->set_on_disconnect(capture(state, 2));
  return 0;
}

int websocket_subscribe(lua_State* state) {
  auto* instance = argument<channel>(state, 1, "WebSocket");
  const auto raw = luaL_checkinteger(state, 2);
  luaL_argcheck(state, raw >= 0 && raw <= std::numeric_limits<uint16_t>::max(), 2, "topic must be 0..65535");
  const auto topic = static_cast<uint16_t>(raw);
  const auto reference = capture(state, 3);

  subscription *sub = nullptr;
  try {
    sub = new subscription(instance, topic, reference);
  } catch (...) {
    luaL_unref(state, LUA_REGISTRYINDEX, reference);
    throw;
  }

  pushuserdata(state, sub, "Subscription");
  return 1;
}

int websocket_index(lua_State* state) {
  argument<void>(state, 1, "WebSocket");
  const auto key = argument<std::string_view>(state, 2);

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

int websocket_gc(lua_State* state) {
  auto **pointer = static_cast<channel **>(argument<void>(state, 1, "WebSocket"));
  if (*pointer == connection.get())
    connection.reset();
  *pointer = nullptr;
  return 0;
}

int websocket_call(lua_State* state) {
  auto url = argument<std::string>(state, 1);

  connection = std::make_unique<channel>(std::move(url));

  pushuserdata(state, connection.get(), "WebSocket");
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
  auto* ws = static_cast<channel*>(lws_context_user(context));

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
      const auto *bytes = static_cast<const unsigned char *>(in);
      cbor_load_result result;
      auto *root = cbor_load(bytes, length, &result);
      if (!root || result.error.code != CBOR_ERR_NONE) [[unlikely]] {
        if (root) cbor_decref(&root);
        break;
      }

      if (!cbor_isa_array(root) || cbor_array_size(root) < 2) [[unlikely]] {
        cbor_decref(&root);
        break;
      }

      auto *topic_item = cbor_array_get(root, 1);
      if (!cbor_isa_uint(topic_item)) [[unlikely]] {
        cbor_decref(&root);
        break;
      }

      const auto topic = static_cast<uint16_t>(cbor_get_int(topic_item));

      ws->_inbound.push(message{
        topic,
        std::vector<uint8_t>(bytes, bytes + length)
      });

      cbor_decref(&root);
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
        lws_write(wsi, ws->_sendbuffer.data() + LWS_PRE, size, LWS_WRITE_BINARY);
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
  if (_on_disconnect != LUA_NOREF) {
    try {
      lua_rawgeti(L, LUA_REGISTRYINDEX, _on_disconnect);
      pcall(L, 0, 0);
    } catch (...) {}
  }

  release(L, _on_connect);
  release(L, _on_disconnect);

  for (auto& [topic, subscribers] : _subscriptions) {
    for (auto* subscriber : subscribers) {
      release(L, subscriber->_callback);
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
  struct lws_context_creation_info creation{};
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
  struct lws_client_connect_info connect_info{};
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

    auto* current_wsi = _wsi.load(std::memory_order_acquire);
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
  if (_pending_connect.exchange(false, std::memory_order_acq_rel))
    invoke(L, _on_connect, LUA_NOREF);

  if (_pending_disconnect.exchange(false, std::memory_order_acq_rel))
    invoke(L, _on_disconnect, LUA_NOREF);

  message message;
  while (_inbound.try_pop(message)) {
    cbor_load_result result;
    auto *root = cbor_load(message.payload.data(), message.payload.size(), &result);
    if (!root || result.error.code != CBOR_ERR_NONE) [[unlikely]] {
      if (root) cbor_decref(&root);
      continue;
    }

    cbor_item_t *data_value = nullptr;
    if (cbor_isa_array(root) && cbor_array_size(root) >= 3) [[likely]]
      data_value = cbor_array_get(root, 2);

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

      if (data_value)
        cbor_to_lua(L, data_value);
      else
        lua_pushnil(L);

      pcall(L, 1, 0);
    }

    cbor_decref(&root);
  }
}

void channel::set_on_connect(int reference) noexcept {
  release(L, _on_connect);
  _on_connect = reference;
}

void channel::set_on_disconnect(int reference) noexcept {
  release(L, _on_disconnect);
  _on_disconnect = reference;
}

void channel::add_subscription(subscription* subscriber) {
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

subscription::subscription(channel* owner, uint16_t topic, int callback_ref)
  : _owner(owner), _topic(topic), _callback(callback_ref) {
  _owner->add_subscription(this);
}

subscription::~subscription() {
  unsubscribe();
}

void subscription::publish(lua_State *state, int index) {
  if (!_active || !_owner) [[unlikely]]
    return;

  auto *data = lua_to_cbor(state, index);
  auto *arr = cbor_new_definite_array(3);
  std::ignore = cbor_array_push(arr, cbor_move(cbor_build_uint8(std::to_underlying(opcode::publish))));
  std::ignore = cbor_array_push(arr, cbor_move(cbor_build_uint16(_topic)));
  std::ignore = cbor_array_push(arr, cbor_move(data));
  _owner->send(message{_topic, serialize(arr)});
}

void subscription::unsubscribe() {
  if (!_active) [[unlikely]] return;
  _active = false;

  if (_owner) [[likely]]
    _owner->remove_subscription(this);

  release(L, _callback);
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
