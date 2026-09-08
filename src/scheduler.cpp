scheduler::~scheduler() noexcept {
  clear();
}

scheduler::entry* scheduler::find(const handle& value) noexcept {
  if (value.life.expired()) [[unlikely]]
    return nullptr;

  assert(value.owner && "timer handle must have an owner");
  [[assume(value.owner)]];
  assert(value.slot < capacity && "timer handle slot must be valid");
  [[assume(value.slot < capacity)]];

  const auto bit = static_cast<std::uint16_t>(1u << value.slot);
  auto& current = value.owner->_entries[value.slot];
  if ((value.owner->_used & bit) == 0 || current.generation != value.generation) [[unlikely]]
    return nullptr;

  return &current;
}

scheduler::handle scheduler::add(double milliseconds, bool repeat, callback call, callback release, std::uint64_t data) {
  const auto free = static_cast<std::uint16_t>(~_used);
  const auto available = free != 0;
  assert(available && "scene must not exceed 16 timers");
  [[assume(available)]];
  const auto index = static_cast<std::size_t>(std::countr_zero(free));
  const auto bit = static_cast<std::uint16_t>(1u << index);
  auto& current = _entries[index];
  const auto room = current.generation < std::numeric_limits<std::uint32_t>::max();
  assert(room && "timer handle generation must remain available");
  [[assume(room)]];
  const auto generation = current.generation + 1;
  current = {
    .call = call,
    .release = release,
    .data = data,
    .deadline = _now + milliseconds,
    .period = milliseconds,
    .generation = generation,
  };

  _used |= bit;
  _hold &= static_cast<std::uint16_t>(~bit);
  if (repeat)
    _repeat |= bit;
  else
    _repeat &= static_cast<std::uint16_t>(~bit);
  if (_active)
    _live |= bit;
  else
    _live &= static_cast<std::uint16_t>(~bit);

  return handle{
    .life = _life,
    .owner = this,
    .slot = static_cast<std::uint32_t>(index),
    .generation = generation,
  };
}

void scheduler::cancel(const handle& value) noexcept {
  auto* const current = find(value);
  if (!current) [[unlikely]]
    return;

  auto* const owner = value.owner;
  const auto bit = static_cast<std::uint16_t>(1u << value.slot);
  const auto release = current->release;
  const auto data = current->data;
  const auto generation = current->generation;
  owner->_used &= static_cast<std::uint16_t>(~bit);
  owner->_live &= static_cast<std::uint16_t>(~bit);
  owner->_hold &= static_cast<std::uint16_t>(~bit);
  owner->_repeat &= static_cast<std::uint16_t>(~bit);
  *current = {.generation = generation};
  release(data);
}

void scheduler::pause(const handle& value) noexcept {
  auto* const current = find(value);
  if (!current) [[unlikely]]
    return;

  auto* const owner = value.owner;
  const auto bit = static_cast<std::uint16_t>(1u << value.slot);
  if ((owner->_hold & bit) != 0) [[unlikely]]
    return;

  current->deadline = std::max(current->deadline - owner->_now, 0.0);
  owner->_live &= static_cast<std::uint16_t>(~bit);
  owner->_hold |= bit;
}

void scheduler::resume(const handle& value) noexcept {
  auto* const current = find(value);
  if (!current) [[unlikely]]
    return;

  auto* const owner = value.owner;
  const auto bit = static_cast<std::uint16_t>(1u << value.slot);
  if ((owner->_hold & bit) == 0) [[unlikely]]
    return;

  owner->_hold &= static_cast<std::uint16_t>(~bit);
  current->deadline += owner->_now;
  if (owner->_active)
    owner->_live |= bit;
}

bool scheduler::active(const handle& value) noexcept {
  return find(value) != nullptr;
}

void scheduler::update(float delta) {
  auto bits = _live;
  if (!bits) [[likely]]
    return;

  _now += delta * 1000.0f;
  while (bits) {
    const auto index = static_cast<std::size_t>(std::countr_zero(bits));
    const auto bit = static_cast<std::uint16_t>(1u << index);
    bits &= static_cast<std::uint16_t>(bits - 1);
    if ((_live & bit) == 0) [[unlikely]]
      continue;

    auto& current = _entries[index];
    if (_now < current.deadline) [[likely]]
      continue;

    const auto call = current.call;
    const auto release = current.release;
    const auto data = current.data;
    if ((_repeat & bit) != 0) {
      current.deadline = _now + current.period;
      call(data);
      continue;
    }

    const auto generation = current.generation;
    _used &= static_cast<std::uint16_t>(~bit);
    _live &= static_cast<std::uint16_t>(~bit);
    _hold &= static_cast<std::uint16_t>(~bit);
    _repeat &= static_cast<std::uint16_t>(~bit);
    current = {.generation = generation};
    try {
      call(data);
    } catch (...) {
      release(data);
      throw;
    }
    release(data);
  }
}

void scheduler::clear() noexcept {
  auto bits = std::exchange(_used, std::uint16_t{});
  _live = 0;
  _hold = 0;
  _repeat = 0;
  while (bits) {
    const auto index = static_cast<std::size_t>(std::countr_zero(bits));
    bits &= static_cast<std::uint16_t>(bits - 1);
    auto& current = _entries[index];
    const auto release = current.release;
    const auto data = current.data;
    const auto generation = current.generation;
    current = {.generation = generation};
    release(data);
  }
}

void scheduler::activate() {
  _active = true;
  _live = static_cast<std::uint16_t>(_used & static_cast<std::uint16_t>(~_hold));
}

void scheduler::suspend() noexcept {
  _active = false;
  _live = 0;
}

namespace {
constexpr auto name = "TimerHandle";

struct handle final {
  scheduler::handle value{};
};

void invoke(std::uint64_t data) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, static_cast<int>(data));
  if (pcall(L, 0, 0) != LUA_OK) [[unlikely]]
    propagate();
}

void release(std::uint64_t data) noexcept {
  luaL_unref(L, LUA_REGISTRYINDEX, static_cast<int>(data));
}

handle* check(lua_State* state) {
  return static_cast<handle *>(luaL_checkudata(state, 1, name));
}

int cancel_callback(lua_State* state) {
  auto* const value = check(state);
  scheduler::cancel(value->value);
  lua_settop(state, 1);

  return 1;
}

int pause_callback(lua_State* state) {
  auto* const value = check(state);
  scheduler::pause(value->value);
  lua_settop(state, 1);

  return 1;
}

int resume_callback(lua_State* state) {
  auto* const value = check(state);
  scheduler::resume(value->value);
  lua_settop(state, 1);

  return 1;
}

int index(lua_State* state) {
  auto* const value = check(state);
  std::size_t length;
  const auto* const data = luaL_checklstring(state, 2, &length);
  const std::string_view key{data, length};

  if (key == "active") {
    lua_pushboolean(state, scheduler::active(value->value));

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
  auto* const current = static_cast<scheduler *>(lua_touserdata(state, lua_upvalueindex(1)));
  const auto milliseconds = static_cast<double>(luaL_checknumber(state, 2));

  lua_pushvalue(state, 3);
  const auto callback = luaL_ref(state, LUA_REGISTRYINDEX);
  auto* const memory = new (lua_newuserdata(state, sizeof(handle))) handle{};
  memory->value = current->add(milliseconds, repeat, invoke, release, static_cast<std::uint64_t>(callback));

  luaL_getmetatable(state, name);
  lua_setmetatable(state, -2);

  return 1;
}

int add_callback(lua_State* state) {
  return schedule(state, true);
}

int singleshot_callback(lua_State* state) {
  return schedule(state, false);
}

int clear_callback(lua_State* state) {
  auto* const current = static_cast<scheduler *>(lua_touserdata(state, lua_upvalueindex(1)));
  current->clear();

  return 0;
}
}

void scheduler::wire() {
  if (luaL_newmetatable(L, name)) {
    lua_pushstring(L, name);
    lua_setfield(L, -2, "__name");

    lua_pushcfunction(L, cancel_callback);
    lua_pushcfunction(L, pause_callback);
    lua_pushcfunction(L, resume_callback);
    lua_pushcclosure(L, index, 3);
    lua_setfield(L, -2, "__index");

    lua_pushcfunction(L, gc);
    lua_setfield(L, -2, "__gc");
  }

  lua_pop(L, 1);

  lua_newtable(L);

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, add_callback, 1);
  lua_setfield(L, -2, "add");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, singleshot_callback, 1);
  lua_setfield(L, -2, "singleshot");

  lua_pushlightuserdata(L, this);
  lua_pushcclosure(L, clear_callback, 1);
  lua_setfield(L, -2, "clear");

  _table = luaL_ref(L, LUA_REGISTRYINDEX);
}
