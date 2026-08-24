namespace {
constexpr auto invalid = std::numeric_limits<std::uint32_t>::max();
constexpr auto live = std::uint8_t{1};
constexpr auto hold = std::uint8_t{2};
constexpr auto done = std::uint8_t{3};
constexpr auto capacity = 8uz;
}

struct timer::record final {
  double deadline{};
  double period{};
  std::uint64_t data{};
  callback call{};
  callback release{};
  record* previous{};
  record* next{};
  std::uint32_t slot{};
  std::uint32_t generation{};
  std::uint8_t status{done};
  bool repeat{};
};

struct timer::state final {
  record* head{};
  record* free{};
  record* cursor{};
  double now{};
  std::array<record, capacity> fixed{};
  std::deque<record> overflow{};

  state() noexcept {
    for (auto i = capacity; i > 0; --i) {
      auto& current = fixed[i - 1];
      current.slot = static_cast<std::uint32_t>(i - 1);
      current.next = free;
      free = &current;
    }
  }

  ~state() noexcept {
    for (auto& current : fixed) {
      if (current.release) [[likely]]
        current.release(current.data);
    }

    for (auto& current : overflow) {
      if (current.release) [[likely]]
        current.release(current.data);
    }
  }
};

timer::timer()
    : _state(std::make_shared<state>()) {
}

timer::~timer() noexcept = default;

timer::record* timer::find(state& current, const handle& value) noexcept {
  record* result;
  if (value.slot < capacity)
    result = &current.fixed[value.slot];
  else {
    const auto index = static_cast<std::size_t>(value.slot) - capacity;
    if (index >= current.overflow.size()) [[unlikely]]
      return nullptr;
    result = &current.overflow[index];
  }

  if (result->generation != value.generation) [[unlikely]]
    return nullptr;

  return result;
}

void timer::deactivate(state& current, record& node, bool release) noexcept {
  if (node.status == done) [[unlikely]]
    return;

  if (release && current.cursor == &node)
    current.cursor = node.next;

  if (node.previous)
    node.previous->next = node.next;
  else
    current.head = node.next;

  if (node.next)
    node.next->previous = node.previous;

  node.status = done;
  node.previous = nullptr;
  node.next = current.free;
  current.free = &node;

  if (release && node.release) [[likely]] {
    node.release(node.data);
    node.release = nullptr;
    node.call = nullptr;
  }
}

timer::handle timer::add(double milliseconds, bool repeat, callback call, callback release, std::uint64_t data) {
  const auto valid = milliseconds > 0.0 && std::isfinite(milliseconds);
  assert(valid && "timer period must be positive and finite");
  [[assume(valid)]];
  assert(call && "timer callback must be provided");
  assert(release && "timer release callback must be provided");

  auto& current = *_state;
  record* node;
  if (current.free) {
    node = current.free;
    current.free = node->next;
    const auto available = node->generation < invalid;
    assert(available && "timer handle generation must remain available");
    [[assume(available)]];
    ++node->generation;
  } else {
    current.overflow.emplace_back();
    node = &current.overflow.back();
    node->slot = static_cast<std::uint32_t>(capacity + current.overflow.size() - 1);
    node->generation = 1;
  }

  node->deadline = current.now + milliseconds;
  node->period = milliseconds;
  node->data = data;
  node->call = call;
  node->release = release;
  node->previous = nullptr;
  node->next = current.head;
  node->status = live;
  node->repeat = repeat;
  current.head = node;
  if (node->next)
    node->next->previous = node;

  return handle{
    .life = _state,
    .slot = node->slot,
    .generation = node->generation,
  };
}

void timer::cancel(const handle& value) noexcept {
  auto current = std::static_pointer_cast<state>(value.life.lock());
  if (!current) [[unlikely]]
    return;

  if (auto* const result = find(*current, value))
    deactivate(*current, *result, true);
}

void timer::pause(const handle& value) noexcept {
  auto current = std::static_pointer_cast<state>(value.life.lock());
  if (!current) [[unlikely]]
    return;

  if (auto* const result = find(*current, value); result && result->status == live) {
    result->deadline = std::max(result->deadline - current->now, 0.0);
    result->status = hold;
  }
}

void timer::resume(const handle& value) noexcept {
  auto current = std::static_pointer_cast<state>(value.life.lock());
  if (!current) [[unlikely]]
    return;

  if (auto* const result = find(*current, value); result && result->status == hold) {
    result->deadline += current->now;
    result->status = live;
  }
}

bool timer::active(const handle& value) noexcept {
  auto current = std::static_pointer_cast<state>(value.life.lock());
  if (!current) [[unlikely]]
    return false;

  const auto* const result = find(*current, value);
  return result && result->status != done;
}

void timer::clear() noexcept {
  auto& current = *_state;
  auto* node = current.head;
  while (node) {
    auto* const next = node->next;
    deactivate(current, *node, true);
    node = next;
  }
}

void timer::update(float delta) {
  auto current = _state;
  current->now += static_cast<double>(delta) * 1000.0;

  auto* node = current->head;
  while (node) {
    current->cursor = node->next;
    if (node->status != live || current->now < node->deadline) {
      node = current->cursor;
      continue;
    }

    const auto generation = node->generation;
    const auto repeat = node->repeat;
    if (repeat)
      node->deadline += node->period;
    else
      deactivate(*current, *node, false);

    const auto call = node->call;
    const auto release = node->release;
    const auto data = node->data;
    if (!repeat) {
      node->call = nullptr;
      node->release = nullptr;
    }

    try {
      call(data);
    } catch (...) {
      if (!repeat) [[likely]]
        release(data);
      current->cursor = nullptr;
      throw;
    }

    if (!repeat) [[likely]]
      release(data);

    if (node->generation == generation && node->status == live && current->now >= node->deadline) {
      continue;
    }

    node = current->cursor;
  }

  current->cursor = nullptr;
}

namespace callbacks {
constexpr auto name = "TimerHandle";

struct handle final {
  timer::handle value{};
};

void invoke(std::uint64_t data) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, static_cast<int>(data));
  if (lua_pcall(L, 0, 0, 0) != LUA_OK) [[unlikely]]
    throw std::runtime_error{lua_tostring(L, -1)};
}

void release(std::uint64_t data) noexcept {
  luaL_unref(L, LUA_REGISTRYINDEX, static_cast<int>(data));
}

handle* check(lua_State* state) {
  return static_cast<handle *>(luaL_checkudata(state, 1, name));
}

int cancel(lua_State* state) {
  auto* const value = check(state);
  timer::cancel(value->value);
  lua_settop(state, 1);
  return 1;
}

int pause(lua_State* state) {
  auto* const value = check(state);
  timer::pause(value->value);
  lua_settop(state, 1);
  return 1;
}

int resume(lua_State* state) {
  auto* const value = check(state);
  timer::resume(value->value);
  lua_settop(state, 1);
  return 1;
}

int index(lua_State* state) {
  auto* const value = check(state);
  std::size_t length;
  const auto* const data = luaL_checklstring(state, 2, &length);
  const std::string_view key{data, length};

  if (key == "active") {
    lua_pushboolean(state, timer::active(value->value));
    return 1;
  }

  if (key == "cancel")
    lua_pushvalue(state, lua_upvalueindex(1));
  else if (key == "pause")
    lua_pushvalue(state, lua_upvalueindex(2));
  else if (key == "resume")
    lua_pushvalue(state, lua_upvalueindex(3));
  else
    lua_pushnil(state);

  return 1;
}

int gc(lua_State* state) {
  auto* const value = check(state);
  value->~handle();
  return 0;
}

int schedule(lua_State* state, bool repeat) {
  auto* const current = static_cast<timer *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto milliseconds = static_cast<double>(luaL_checknumber(state, 2));
  const auto valid = milliseconds > 0.0 && std::isfinite(milliseconds);
  assert(valid && "timer period must be positive and finite");
  [[assume(valid)]];

  const auto callable = lua_isfunction(state, 3);
  assert(callable && "timer callback must be a function");
  [[assume(callable)]];

  lua_pushvalue(state, 3);
  const auto callback = luaL_ref(state, LUA_REGISTRYINDEX);
  auto* const memory = new (lua_newuserdata(state, sizeof(handle))) handle{};
  try {
    memory->value = current->add(milliseconds, repeat, invoke, release, static_cast<std::uint64_t>(callback));
  } catch (...) {
    memory->~handle();
    lua_pop(state, 1);
    luaL_unref(state, LUA_REGISTRYINDEX, callback);
    throw;
  }

  luaL_getmetatable(state, name);
  lua_setmetatable(state, -2);
  return 1;
}

int add(lua_State* state) {
  return schedule(state, true);
}

int singleshot(lua_State* state) {
  return schedule(state, false);
}

int clear(lua_State* state) {
  auto* const current = static_cast<timer *>(lua_touserdata(state, lua_upvalueindex(1)));
  current->clear();
  return 0;
}

void wire(timer& current) {
  if (luaL_newmetatable(L, name)) {
    lua_pushstring(L, name);
    lua_setfield(L, -2, "__name");

    lua_pushcfunction(L, cancel);
    lua_pushcfunction(L, pause);
    lua_pushcfunction(L, resume);
    lua_pushcclosure(L, index, 3);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, gc);
    lua_setfield(L, -2, "__gc");
  }
  lua_pop(L, 1);

  lua_newtable(L);

  lua_pushlightuserdata(L, &current);
  lua_pushcclosure(L, add, 1);
  lua_setfield(L, -2, "add");

  lua_pushlightuserdata(L, &current);
  lua_pushcclosure(L, singleshot, 1);
  lua_setfield(L, -2, "singleshot");

  lua_pushlightuserdata(L, &current);
  lua_pushcclosure(L, clear, 1);
  lua_setfield(L, -2, "clear");

  current._table = luaL_ref(L, LUA_REGISTRYINDEX);
}
}
