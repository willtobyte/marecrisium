namespace {
constexpr auto invalid = std::numeric_limits<uint32_t>::max();
constexpr auto paused = uint32_t{1} << (std::numeric_limits<uint32_t>::digits - 1);
constexpr auto mask = ~paused;
constexpr auto capacity = static_cast<std::size_t>(std::numeric_limits<int>::max() - 1);
constexpr auto retained = 64uz;
constexpr auto none = std::numeric_limits<double>::infinity();
constexpr auto dirty = -none;
constexpr auto name = "TimerHandle";

namespace lookup {
constexpr auto active = "active"_hs;
constexpr auto cancel = "cancel"_hs;
constexpr auto pause = "pause"_hs;
constexpr auto resume = "resume"_hs;
}

struct ticket final {
  uint32_t group{invalid};
  uint32_t slot{invalid};
  uint32_t generation{};
};

struct record final {
  double period;
  double deadline;
  uint32_t slot;
  bool repeat;
};

struct slot final {
  uint32_t position{invalid};
  uint32_t generation{};
};

struct queue final {
  std::vector<record> list;
  std::vector<slot> slots;
  uint32_t free{invalid};
  int roots{LUA_NOREF};
  std::size_t removed{};
  double next{none};
  double now{};
};

static_assert(sizeof(unsigned) == sizeof(uint32_t));
static_assert(sizeof(ticket) == 12);
static_assert(sizeof(record) == 24);
static_assert(sizeof(slot) == 8);
static_assert(sizeof(queue) == 80);

struct store final {
  store() = delete;

  static inline std::vector<std::unique_ptr<queue>> groups;
  static inline uint32_t active{invalid};
  static inline uint32_t owner{invalid};
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
  const auto available = std::find(store::groups.begin(), store::groups.end(), nullptr);
  const auto room = available != store::groups.end() || store::groups.size() < invalid;
  assert(room && "timer group capacity must not be exceeded");
  [[assume(room)]];

  lua_newtable(L);
  const auto roots = luaL_ref(L, LUA_REGISTRYINDEX);
  const auto group = available == store::groups.end()
    ? static_cast<uint32_t>(store::groups.size())
    : static_cast<uint32_t>(available - store::groups.begin());
  auto current = std::make_unique<queue>();
  current->roots = roots;
  available == store::groups.end()
    ? static_cast<void>(store::groups.emplace_back(std::move(current)))
    : static_cast<void>(*available = std::move(current));
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

void deactivate(queue& group, record& current, int root, bool reschedule) {
  const auto index = current.slot & mask;
  if (reschedule && (current.slot & paused) == 0 && current.deadline == group.next)
    group.next = dirty;

  current.slot = invalid;
  release(group, index);
  ++group.removed;
  erase(L, root, index);
}

void deactivate(queue& group, record& current, bool reschedule = true) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, group.roots);
  const auto root = lua_gettop(L);
  deactivate(group, current, root, reschedule);
  lua_pop(L, 1);
}

void reset(queue& group) {
  std::vector<record>{}.swap(group.list);
  std::vector<slot>{}.swap(group.slots);
  group.removed = 0;
  group.free = invalid;
  group.next = none;

  lua_newtable(L);
  const auto roots = luaL_ref(L, LUA_REGISTRYINDEX);
  luaL_unref(L, LUA_REGISTRYINDEX, group.roots);
  group.roots = roots;
}

void cancel(ticket *owner) {
  const auto [group, current] = find(owner);
  if (!current) [[unlikely]]
    return;

  owner->group = invalid;
  deactivate(*group, *current, group->removed + 1 != group->list.size());

  if (store::running == invalid && current == &group->list.back()) {
    do {
      group->list.pop_back();
      --group->removed;
    } while (!group->list.empty() && group->list.back().slot == invalid);
  }

  if (group->list.empty())
    group->next = none;

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
    const auto deadline = current->deadline;
    current->deadline = std::max(deadline - group->now, 0.0);
    current->slot |= paused;
    if (deadline == group->next)
      group->next = dirty;
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
    group->next = std::min(group->next, current->deadline);
  }

  lua_settop(state, 1);
  return 1;
}

static int active_callback(lua_State *state) {
  const auto *owner = static_cast<ticket *>(luaL_checkudata(state, 1, name));
  lua_pushboolean(state, find(owner).current != nullptr);
  return 1;
}

static int index_callback(lua_State *state) {
  const auto value = entt::hashed_string{luaL_checkstring(state, 2)};
  switch (value) {
    case lookup::active: return active_callback(state);
    case lookup::cancel: lua_pushvalue(state, lua_upvalueindex(1)); break;
    case lookup::pause: lua_pushvalue(state, lua_upvalueindex(2)); break;
    case lookup::resume: lua_pushvalue(state, lua_upvalueindex(3)); break;
    default: lua_pushnil(state); break;
  }
  return 1;
}

template<bool repeat>
int add(lua_State *state) {
  const auto duration = period(state, 2);
  const auto callable = lua_type(state, 3) == LUA_TFUNCTION;
  assert(callable && "timer callback must be a function");
  [[assume(callable)]];

  auto *const current = queue_of(store::owner);
  assert(current && "timer requires an active stage");
  [[assume(current)]];

  auto &group = *current;
  const auto reused = group.free != invalid;
  const auto available = group.list.size() <= capacity && (reused || group.slots.size() <= capacity);

  assert(available && "timer capacity must not be exceeded");
  [[assume(available)]];

  assert(store::generation != invalid && "timer generation must remain available");
  [[assume(store::generation != invalid)]];

  const auto index = reused
    ? group.free
    : static_cast<uint32_t>(group.slots.size());
  const auto position = static_cast<uint32_t>(group.list.size());
  const auto deadline = group.now + duration;

  lua_rawgeti(state, LUA_REGISTRYINDEX, group.roots);
  auto *const owner = static_cast<ticket *>(lua_newuserdata(state, sizeof(ticket)));
  owner->group = store::owner;
  owner->slot = index;
  owner->generation = store::generation++;
  lua_rawgeti(state, LUA_REGISTRYINDEX, store::meta);
  lua_setmetatable(state, -2);

  lua_pushvalue(state, 3);
  lua_rawseti(state, -3, callback_slot(index));

  if (!reused) {
    group.slots.push_back({
      .position = position,
      .generation = owner->generation,
    });
  }

  group.list.push_back({
    .period = duration,
    .deadline = deadline,
    .slot = index,
    .repeat = repeat,
  });

  if (reused) {
    group.free = group.slots[index].position;
    group.slots[index] = {
      .position = position,
      .generation = owner->generation,
    };
  }

  group.next = std::min(group.next, deadline);
  return 1;
}

static int add_callback(lua_State *state) {
  return add<true>(state);
}

static int singleshot_callback(lua_State *state) {
  return add<false>(state);
}

void compact(queue& group);

void discard(queue& group) {
  if (group.removed != group.list.size()) {
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

  if (store::running == invalid) {
    if (group.removed != 0)
      compact(group);
    group.next = none;
  } else {
    group.next = dirty;
  }
}

void clear() {
  auto *const group = queue_of(store::owner);
  if (group)
    discard(*group);
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

[[nodiscard]] double earliest(const queue& group) noexcept {
  auto next = none;
  for (const auto &current : group.list) {
    if (current.slot != invalid && (current.slot & paused) == 0)
      next = std::min(next, current.deadline);
  }
  return next;
}

#ifndef _MSC_VER
__attribute__((aligned(16)))
#endif
bool advance(queue& group, uint32_t owner, std::size_t limit) {
  struct cycle final {
    explicit cycle(queue& target) noexcept
        : group(target) {
    }

    ~cycle() {
      if (group.removed != 0)
        compact(group);
    }

    cycle(const cycle&) = delete;
    cycle& operator=(const cycle&) = delete;

    queue& group;
  } cycle{group};

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

  const runner running{owner};
  const guard context{owner};
  std::size_t position{};
  while (position < limit) {
    const auto &current = group.list[position];
    if (current.slot != invalid && (current.slot & paused) == 0 && group.now >= current.deadline) {
      const auto index = current.slot & mask;
      const auto repeat = current.repeat;

      if (root.position == 0) {
        lua_rawgeti(L, LUA_REGISTRYINDEX, group.roots);
        root.position = lua_gettop(L);
      }

      lua_rawgeti(L, root.position, callback_slot(index));
      {
        store::owner = owner;
        if (repeat)
          group.list[position].deadline += current.period;
        else
          deactivate(group, group.list[position], root.position, true);

        const auto base = lua_gettop(L);
        lua_rawgeti(L, LUA_REGISTRYINDEX, traceback::slot);
        lua_insert(L, base);

        const auto status = lua_pcall(L, 0, 0, base);

        lua_remove(L, base);

        if (status != LUA_OK) [[unlikely]] {
          lua_remove(L, root.position);
          root.position = 0;
          store::running = running.prior;
          store::owner = context.prior;
          if (group.removed != 0)
            compact(group);
          lua_error(L);
          std::unreachable();
        }
      }

      const auto &updated = group.list[position];
      if (updated.slot == invalid || (updated.slot & paused) != 0 || group.now < updated.deadline)
        ++position;
    } else {
      ++position;
    }
  }

  return root.position != 0;
}

void schedule(queue& group, uint32_t owner, std::size_t limit) {
  if (group.now < group.next) [[likely]]
    return;

  group.next = advance(group, owner, limit)
    ? (group.list.empty() ? none : group.now)
    : earliest(group);
}
}

namespace timer {
group::group()
    : _id(create()) {
}

group::~group() noexcept {
  assert(_id != store::running);
  auto *const queue = queue_of(_id);
  if (!queue) [[unlikely]]
    return;

  if (store::active == _id)
    store::active = invalid;
  if (store::owner == _id)
    store::owner = store::active;

  luaL_unref(L, LUA_REGISTRYINDEX, queue->roots);
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

  luaL_newmetatable(L, name);
  lua_pushstring(L, name);
  lua_setfield(L, -2, "__name");

  lua_pushcfunction(L, cancel_callback);
  lua_pushcfunction(L, pause_callback);
  lua_pushcfunction(L, resume_callback);
  lua_pushcclosure(L, index_callback, 3);
  lua_setfield(L, -2, "__index");

  lua_pushvalue(L, -1);
  store::meta = luaL_ref(L, LUA_REGISTRYINDEX);
  lua_pop(L, 1);

  lua_newtable(L);
  lua_pushcfunction(L, add_callback);
  lua_setfield(L, -2, "add");
  lua_pushcfunction(L, singleshot_callback);
  lua_setfield(L, -2, "singleshot");
  lua_pushcfunction(L, clear_callback);
  lua_setfield(L, -2, "clear");
  lua_pushcfunction(L, update_callback);
  lua_setfield(L, -2, "update");
  lua_setglobal(L, "timer");
}

void update(double delta) {
  if (store::running != invalid) [[unlikely]]
    return;

  const auto active = store::active;
  auto *const group = queue_of(active);
  if (!group) [[unlikely]]
    return;

  const auto limit = group->list.size();
  group->now += delta * 1000.0;
  schedule(*group, active, limit);
}
}
