namespace {
constexpr auto invalid = std::numeric_limits<uint32_t>::max();
constexpr auto paused = uint32_t{1} << (std::numeric_limits<uint32_t>::digits - 1);
constexpr auto mask = ~paused;
constexpr auto capacity = static_cast<std::size_t>(std::numeric_limits<int>::max() - 1);
constexpr auto retained = 64uz;
constexpr auto global = uint32_t{};
constexpr auto name = "TimerHandle";

struct ticket final {
  uint32_t group{invalid};
  uint32_t slot{invalid};
  uint32_t generation{};
};

struct record final {
  double period;
  double deadline;
  uint32_t slot;
};

struct slot final {
  uint32_t position{invalid};
  uint32_t generation{};
};

struct queue final {
  std::vector<record> list;
  std::vector<slot> slots;
  uint32_t free{invalid};
  std::size_t removed{};
  double now{};
  int roots{LUA_NOREF};
};

static_assert(sizeof(unsigned) == sizeof(uint32_t));
static_assert(sizeof(ticket) == 12);
static_assert(sizeof(record) == 24);
static_assert(sizeof(slot) == 8);

struct store final {
  store() = delete;

  static inline std::vector<std::unique_ptr<queue>> groups;
  static inline uint32_t active{global};
  static inline uint32_t owner{global};
  static inline uint32_t running{invalid};
  static inline uint32_t generation{1};
  static inline int meta{LUA_NOREF};
};

[[nodiscard]] constexpr int callback_slot(uint32_t index) noexcept {
  return static_cast<int>(index + 1);
}

[[nodiscard]] queue *queue_of(uint32_t group) noexcept {
  if (group >= store::groups.size()) [[unlikely]]
    return nullptr;

  return store::groups[group].get();
}

struct guard final {
  explicit guard(uint32_t owner) noexcept
      : prior(store::owner) {
    store::owner = owner;
  }

  ~guard() {
    store::owner = prior;
  }

  guard(const guard&) = delete;
  guard& operator=(const guard&) = delete;

  uint32_t prior;
};

struct runner final {
  explicit runner(uint32_t owner) noexcept
      : prior(store::running) {
    store::running = owner;
  }

  ~runner() {
    store::running = prior;
  }

  runner(const runner&) = delete;
  runner& operator=(const runner&) = delete;

  uint32_t prior;
};

[[nodiscard]] uint32_t create() {
  const auto begin = store::groups.empty()
    ? store::groups.end()
    : std::next(store::groups.begin());
  const auto available = std::find(begin, store::groups.end(), nullptr);
  if (available == store::groups.end() && store::groups.size() >= invalid) [[unlikely]]
    throw std::length_error{"too many timer groups"};

  lua_newtable(L);
  const auto roots = luaL_ref(L, LUA_REGISTRYINDEX);
  const auto group = available == store::groups.end()
    ? static_cast<uint32_t>(store::groups.size())
    : static_cast<uint32_t>(available - store::groups.begin());

  try {
    auto current = std::make_unique<queue>();
    current->roots = roots;
    available == store::groups.end()
      ? static_cast<void>(store::groups.emplace_back(std::move(current)))
      : static_cast<void>(*available = std::move(current));
  } catch (...) {
    luaL_unref(L, LUA_REGISTRYINDEX, roots);
    throw;
  }

  return group;
}

[[nodiscard]] double period(lua_State *state, int index) {
  const auto milliseconds = static_cast<double>(lua_tonumber(state, index));
  const auto valid = milliseconds > 0.0 && std::isfinite(milliseconds);
  assert(valid && "timer period must be positive and finite");
  [[assume(valid)]];
  return milliseconds;
}

struct found final {
  queue *group;
  record *current;
};

[[nodiscard]] found find(const ticket *owner) noexcept {
  auto *const group = queue_of(owner->group);
  if (!group || owner->slot >= group->slots.size()) [[unlikely]]
    return {};

  const auto &location = group->slots[owner->slot];
  if (location.generation != owner->generation) [[unlikely]]
    return {};

  auto &current = group->list[location.position];
  assert((current.slot & mask) == owner->slot);
  return {group, &current};
}

void erase(lua_State *state, int root, uint32_t index) {
  lua_pushnil(state);
  lua_rawseti(state, root, callback_slot(index));
}

void release(queue& group, uint32_t index) noexcept {
  group.slots[index] = {
    .position = group.free,
    .generation = invalid,
  };
  group.free = index;
}

void reset(queue& group) {
  std::vector<record>{}.swap(group.list);
  std::vector<slot>{}.swap(group.slots);
  group.removed = 0;
  group.free = invalid;

  lua_newtable(L);
  const auto roots = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_unref(L, LUA_REGISTRYINDEX, group.roots);
  group.roots = roots;
}

void cancel(ticket *owner) {
  const auto [group, current] = find(owner);
  if (!current) [[unlikely]]
    return;

  const auto index = owner->slot;
  owner->group = invalid;
  current->slot = invalid;
  release(*group, index);
  ++group->removed;

  if (store::running == invalid && current == &group->list.back()) {
    do {
      group->list.pop_back();
      --group->removed;
    } while (!group->list.empty() && group->list.back().slot == invalid);
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, group->roots);
  const auto root = lua_gettop(L);
  erase(L, root, index);
  lua_pop(L, 1);
  if (group->list.empty() &&
      (group->list.capacity() > retained || group->slots.capacity() > retained))
    reset(*group);
}

static int cancel_callback(lua_State *state) {
  cancel(static_cast<ticket *>(luaL_checkudata(state, 1, name)));
  lua_settop(state, 1);
  return 1;
}

static int pause_callback(lua_State *state) {
  const auto *owner = static_cast<ticket *>(luaL_checkudata(state, 1, name));
  const auto [group, current] = find(owner);
  if (current && (current->slot & paused) == 0) [[likely]] {
    current->deadline = std::max(current->deadline - group->now, 0.0);
    current->slot |= paused;
  }

  lua_settop(state, 1);
  return 1;
}

static int resume_callback(lua_State *state) {
  const auto *owner = static_cast<ticket *>(luaL_checkudata(state, 1, name));
  const auto [group, current] = find(owner);
  if (current && (current->slot & paused) != 0) [[likely]] {
    current->deadline += group->now;
    current->slot &= mask;
  }

  lua_settop(state, 1);
  return 1;
}

static int reset_callback(lua_State *state) {
  const auto *owner = static_cast<ticket *>(luaL_checkudata(state, 1, name));
  const auto [group, current] = find(owner);
  if (!current) [[unlikely]] {
    lua_settop(state, 1);
    return 1;
  }

  if (!lua_isnoneornil(state, 2))
    current->period = period(state, 2);

  current->deadline = (current->slot & paused) != 0
    ? current->period
    : group->now + current->period;

  lua_settop(state, 1);
  return 1;
}

static int is_active_callback(lua_State *state) {
  const auto *owner = static_cast<ticket *>(luaL_checkudata(state, 1, name));
  lua_pushboolean(state, find(owner).current != nullptr);
  return 1;
}

static int is_paused_callback(lua_State *state) {
  const auto *owner = static_cast<ticket *>(luaL_checkudata(state, 1, name));
  const auto current = find(owner).current;
  lua_pushboolean(state, current && (current->slot & paused) != 0);
  return 1;
}

static int add_callback(lua_State *state) {
  const auto duration = period(state, 2);
  const auto callable = lua_type(state, 3) == LUA_TFUNCTION;
  assert(callable && "timer callback must be a function");
  [[assume(callable)]];

  auto &group = *queue_of(store::owner);
  const auto reused = group.free != invalid;
  const auto available = group.list.size() <= capacity && (reused || group.slots.size() <= capacity);
  assert(available && "timer capacity must not be exceeded");
  [[assume(available)]];
  assert(store::generation != invalid && "timer generation must remain available");
  [[assume(store::generation != invalid)]];

  const auto index = reused
    ? group.free
    : static_cast<uint32_t>(group.slots.size());

  lua_rawgeti(state, LUA_REGISTRYINDEX, group.roots);
  auto *const owner = static_cast<ticket *>(lua_newuserdata(state, sizeof(ticket)));
  owner->group = store::owner;
  owner->slot = index;
  owner->generation = store::generation++;
  lua_rawgeti(state, LUA_REGISTRYINDEX, store::meta);
  lua_setmetatable(state, -2);

  lua_pushvalue(state, 3);
  lua_rawseti(state, -3, callback_slot(index));

  const auto position = static_cast<uint32_t>(group.list.size());
  auto added = false;
  try {
    if (!reused) {
      group.slots.push_back({
        .position = position,
        .generation = owner->generation,
      });
      added = true;
    }

    group.list.push_back({
      .period = duration,
      .deadline = group.now + duration,
      .slot = index,
    });
  } catch (const std::exception &error) {
    if (added)
      group.slots.pop_back();

    lua_pushnil(state);
    lua_rawseti(state, -3, callback_slot(index));
    return luaL_error(state, "%s", error.what());
  }

  if (reused) {
    group.free = group.slots[index].position;
    group.slots[index] = {
      .position = position,
      .generation = owner->generation,
    };
  }

  return 1;
}

void discard(queue& group) {
  if (group.removed == group.list.size())
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, group.roots);
  const auto root = lua_gettop(L);

  for (auto &current : group.list) {
    if (current.slot == invalid)
      continue;

    const auto index = current.slot & mask;
    current.slot = invalid;
    release(group, index);
    erase(L, root, index);
  }

  lua_pop(L, 1);
  group.removed = group.list.size();
}

void clear() {
  for (const auto& group : store::groups) {
    if (group)
      discard(*group);
  }
}

static int clear_callback(lua_State *) {
  clear();
  return 0;
}

void compact(queue& group) {
  const auto size = group.list.size();
  assert(group.removed <= size);
  if (group.removed == size) {
    group.list.clear();
    group.removed = 0;
    if (group.list.capacity() > retained || group.slots.capacity() > retained)
      reset(group);
    return;
  }

  std::size_t write{};
  for (std::size_t read = 0; read < size; ++read) {
    if (group.list[read].slot == invalid)
      continue;

    if (write != read) {
      group.list[write] = group.list[read];
      const auto index = group.list[write].slot & mask;
      group.slots[index].position = static_cast<uint32_t>(write);
    }

    ++write;
  }

  assert(write + group.removed == size);
  group.list.resize(write);
  group.removed = 0;
}

#ifndef _MSC_VER
__attribute__((aligned(16)))
#endif
void advance(queue& group, uint32_t owner, std::size_t limit) {
  struct anchor final {
    anchor() = default;

    ~anchor() {
      if (position != 0)
        lua_settop(L, position - 1);
    }

    anchor(const anchor&) = delete;
    anchor& operator=(const anchor&) = delete;

    int position{};
  } root;

  std::size_t position{};
  while (position < limit) {
    const auto &current = group.list[position];
    if (current.slot != invalid && (current.slot & paused) == 0 && group.now >= current.deadline) {
      const auto index = current.slot & mask;
      group.list[position].deadline += current.period;

      if (root.position == 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, group.roots);
        root.position = lua_gettop(L);
      }

      lua_rawgeti(L, root.position, callback_slot(index));
      {
        const runner running{owner};
        const guard context{owner};
        {
          const auto base = lua_gettop(L);
          lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
          lua_insert(L, base);
          const auto status = lua_pcall(L, 0, 0, base);
          lua_remove(L, base);
          if (status != LUA_OK) [[unlikely]] {
            lua_error(L);
            std::unreachable();
          }
        }
      }

      const auto &updated = group.list[position];
      if (updated.slot == invalid || (updated.slot & paused) != 0 || group.now < updated.deadline)
        ++position;
    } else {
      ++position;
    }
  }

  if (group.removed == 0) [[likely]]
    return;

  compact(group);
}
}

namespace timer {
group::group()
    : _id(create()) {
}

group::~group() noexcept {
  assert(_id != store::running);
  auto *const group = queue_of(_id);
  if (!group) [[unlikely]]
    return;

  if (store::active == _id)
    store::active = global;
  if (store::owner == _id)
    store::owner = store::active;

  luaL_unref(L, LUA_REGISTRYINDEX, group->roots);
  store::groups[_id].reset();
}

void group::activate() const noexcept {
  assert(queue_of(_id));
  store::active = _id;
  store::owner = _id;
}

scope::scope(const group& owner) noexcept
    : _prior(store::owner) {
  assert(queue_of(owner._id));
  store::owner = owner._id;
}

scope::~scope() noexcept {
  store::owner = _prior;
}

static int update_callback(lua_State *state) {
  timer::update(static_cast<double>(luaL_checknumber(state, 2)));
  return 0;
}

void wire() {
  assert(store::groups.empty());
  [[maybe_unused]] const auto root = create();
  assert(root == global);

  luaL_newmetatable(L, name);
  lua_pushstring(L, name);
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, cancel_callback);
  lua_setfield(L, -2, "cancel");
  lua_pushcfunction(L, pause_callback);
  lua_setfield(L, -2, "pause");
  lua_pushcfunction(L, resume_callback);
  lua_setfield(L, -2, "resume");
  lua_pushcfunction(L, reset_callback);
  lua_setfield(L, -2, "reset");
  lua_pushcfunction(L, is_active_callback);
  lua_setfield(L, -2, "is_active");
  lua_pushcfunction(L, is_paused_callback);
  lua_setfield(L, -2, "is_paused");
  lua_pushcfunction(L, cancel_callback);
  lua_setfield(L, -2, "__call");

  lua_pushvalue(L, -1);
  lua_setfield(L, -2, "__index");
  lua_pushvalue(L, -1);
  store::meta = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushcfunction(L, add_callback);
  lua_setfield(L, -2, "add");
  lua_pushcfunction(L, clear_callback);
  lua_setfield(L, -2, "clear");
  lua_pushcfunction(L, update_callback);
  lua_setfield(L, -2, "update");
  lua_setglobal(L, "timer");
}

void update(double delta) {
  const auto elapsed = delta * 1000.0;
  const auto active = store::active;
  auto &global_queue = *queue_of(global);
  const auto global_limit = global_queue.list.size();
  global_queue.now += elapsed;

  auto active_limit = std::size_t{};
  if (active != global) {
    auto &active_queue = *queue_of(active);
    active_limit = active_queue.list.size();
    active_queue.now += elapsed;
  }

  advance(global_queue, global, global_limit);

  if (active != global)
    advance(*queue_of(active), active, active_limit);
}
}
