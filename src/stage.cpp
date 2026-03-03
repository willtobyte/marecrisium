#include "stage.hpp"

stage::stage(std::string_view name) : _name(name) {
  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = {.0f, .0f};
  _world = b2CreateWorld(&def);

  _registry.on_destroy<objectproxy>().connect<&objectproxy::on_destroy>();

  lua_newtable(L);
  lua_newtable(L);
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  _environment_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  const auto filename = std::format("stages/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
  lua_setfenv(L, -2);

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  lua_getfield(L, -1, "objects");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    lua_newtable(L);

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -2, i);
      const std::string_view object_name = lua_tostring(L, -1);
      lua_pop(L, 1);

      _pixmaps.load(_name, object_name);

      const auto entity = _registry.create();
      _registry.emplace<transform>(entity);
      auto& proxy = _registry.emplace<objectproxy>(entity, _registry, entity, _name, object_name, _environment_reference);

      lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.self_reference);
      lua_setfield(L, -2, object_name.data());
    }

    _pool_reference = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
    lua_setfield(L, -2, "pool");
    lua_pop(L, 1);
  }
  lua_pop(L, 1);

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);
}

stage::~stage() {
  luaL_unref(L, LUA_REGISTRYINDEX, _pool_reference);
  luaL_unref(L, LUA_REGISTRYINDEX, _environment_reference);
  luaL_unref(L, LUA_REGISTRYINDEX, _reference);
  b2DestroyWorld(_world);
}

void stage::on_enter() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_enter");

  if (lua_isfunction(L, -1)) {
    if (lua_pcall(L, 0, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(error);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::on_leave() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_leave");

  if (lua_isfunction(L, -1)) {
    if (lua_pcall(L, 0, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(error);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::update(float delta) {
  _accumulator += delta;

  while (_accumulator >= FIXED_TIMESTEP) {
    b2World_Step(_world, FIXED_TIMESTEP, WORLD_SUBSTEPS);
    _accumulator -= FIXED_TIMESTEP;
  }

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  lua_getfield(L, -1, "on_loop");

  if (lua_isfunction(L, -1)) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_pushnumber(L, static_cast<lua_Number>(delta));

    if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
      std::string error = lua_tostring(L, -1);
      lua_pop(L, 2);
      throw std::runtime_error(error);
    }
  } else {
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
}

void stage::draw() const {
  #ifdef DEVELOPMENT
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

    const b2AABB aabb = {{0, 0}, {viewport.width, viewport.height}};
    const b2QueryFilter filter = b2DefaultQueryFilter();

    b2World_OverlapAABB(_world, aabb, filter, [](b2ShapeId shape, void*) -> bool {
      const auto box = b2Shape_GetAABB(shape);
      const SDL_FRect r{
        box.lowerBound.x,
        box.lowerBound.y,
        box.upperBound.x - box.lowerBound.x,
        box.upperBound.y - box.lowerBound.y
      };

      SDL_RenderRect(renderer, &r);

      return true;
    }, nullptr);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  #endif
}
