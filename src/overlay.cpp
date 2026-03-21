#include "overlay.hpp"

static bool on_event(void *userdata, SDL_Event *event) {
  auto *self = static_cast<overlay *>(userdata);
  switch (event->type) {
    case SDL_EVENT_TEXT_INPUT: {
      // std::println("{}", event->text.text);
    } break;

    default:
      break;
  }

  return true;
}

static int overlay_label(lua_State *state) {
  auto **ptr = static_cast<overlay **>(luaL_checkudata(state, 1, "Overlay"));
  auto *self = *ptr;
  const auto *font = luaL_checkstring(state, 2);
  const auto *text = luaL_checkstring(state, 3);
  const auto x = static_cast<float>(luaL_checknumber(state, 4));
  const auto y = static_cast<float>(luaL_checknumber(state, 5));

  if (!lua_istable(state, 6)) [[likely]] {
    self->render_label(font, text, x, y);
    return 0;
  }

  static std::array<glypheffect, 256> effects{};
  auto lenght = 0uz;

  lua_pushnil(state);
  while (lua_next(state, 6) != 0) {
    if (lua_isnumber(state, -2) && lua_istable(state, -1)) {
      const auto index = static_cast<std::size_t>(lua_tointeger(state, -2)) - 1;

      if (index < effects.size()) {
        auto &effect = effects[index];

        lua_getfield(state, -1, "xoffset");
        if (lua_isnumber(state, -1)) effect.xoffset = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);

        lua_getfield(state, -1, "yoffset");
        if (lua_isnumber(state, -1)) effect.yoffset = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);

        lua_getfield(state, -1, "scale");
        if (lua_isnumber(state, -1)) effect.scale = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);

        lua_getfield(state, -1, "r");
        if (lua_isnumber(state, -1)) effect.r = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);

        lua_getfield(state, -1, "g");
        if (lua_isnumber(state, -1)) effect.g = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);

        lua_getfield(state, -1, "b");
        if (lua_isnumber(state, -1)) effect.b = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);

        lua_getfield(state, -1, "angle");
        if (lua_isnumber(state, -1)) effect.angle = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);

        lua_getfield(state, -1, "alpha");
        if (lua_isnumber(state, -1)) effect.alpha = static_cast<float>(lua_tonumber(state, -1));
        lua_pop(state, 1);

        if (index >= lenght) lenght = index + 1;
      }
    }

    lua_pop(state, 1);
  }

  self->render_label(font, text, x, y, std::span{effects.data(), lenght});
  return 0;
}

static int overlay_index(lua_State *state) {
  luaL_checkudata(state, 1, "Overlay");
  const std::string_view key = luaL_checkstring(state, 2);

  if (key == "label") {
    lua_pushcfunction(state, overlay_label);
    return 1;
  }

  return lua_pushnil(state), 1;
}

overlay::overlay(std::string_view name) {
  const auto filename = std::format("overlays/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error{lua_tostring(L, -1)};
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error{lua_tostring(L, -1)};
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  lua_getfield(L, -1, "fonts");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      if (lua_isstring(L, -1)) {
        std::ignore = depot->font.get(lua_tostring(L, -1));
      }

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

  lua_getfield(L, -1, "on_loop");
  if (lua_isfunction(L, -1))
    _on_loop = luaL_ref(L, LUA_REGISTRYINDEX);
  else
    lua_pop(L, 1);

  lua_getfield(L, -1, "on_paint");
  if (lua_isfunction(L, -1))
    _on_paint = luaL_ref(L, LUA_REGISTRYINDEX);
  else
    lua_pop(L, 1);

  lua_pop(L, 1);

  auto **memory = static_cast<overlay **>(lua_newuserdata(L, sizeof(overlay *)));
  *memory = this;

  luaL_getmetatable(L, "Overlay");
  lua_setmetatable(L, -2);
  _userdata_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  SDL_AddEventWatch(on_event, this);
  SDL_StartTextInput(SDL_GetRenderWindow(renderer));
}

overlay::~overlay() {
  SDL_StopTextInput(SDL_GetRenderWindow(renderer));
  SDL_RemoveEventWatch(on_event, this);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_paint);
  luaL_unref(L, LUA_REGISTRYINDEX, _on_loop);
  luaL_unref(L, LUA_REGISTRYINDEX, _reference);
  luaL_unref(L, LUA_REGISTRYINDEX, _userdata_reference);
}

void overlay::wire() {
  luaL_newmetatable(L, "Overlay");
  lua_pushcfunction(L, overlay_index);
  lua_setfield(L, -2, "__index");
  lua_pop(L, 1);
}

void overlay::update(float delta) {
  if (_on_loop != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_loop);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
    lua_pushnumber(L, static_cast<lua_Number>(delta));

    if (lua_pcall(L, 2, 0, 0) != 0) [[unlikely]] {
      std::string error{lua_tostring(L, -1)};
      lua_pop(L, 1);
      throw std::runtime_error{std::move(error)};
    }
  }
}

void overlay::draw() {
  if (_on_paint != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_paint);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

    if (lua_pcall(L, 1, 0, 0) != 0) [[unlikely]] {
      std::string error{lua_tostring(L, -1)};
      lua_pop(L, 1);
      throw std::runtime_error{std::move(error)};
    }
  }
}

void overlay::expose() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _userdata_reference);
  lua_setglobal(L, "overlay");
}

void overlay::render_label(std::string_view family, std::string_view text, float x, float y) {
  auto& f = depot->font.get(family);
  f.draw(text, x, y);
}

void overlay::render_label(std::string_view family, std::string_view text, float x, float y, std::span<const glypheffect> effects) {
  auto& f = depot->font.get(family);
  f.draw(text, x, y, effects);
}
