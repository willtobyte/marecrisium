#include "overlay.hpp"

static bool on_event(void *userdata, SDL_Event *event) {
  auto *self = static_cast<overlay *>(userdata);
  switch (event->type) {
    case SDL_EVENT_TEXT_INPUT:
      // std::println("{}", event->text.text);
      break;

    default:
      break;
  }

  return true;
}

static int overlay_label(lua_State *state) {
  auto *self = argument<overlay>(state, 1, "Overlay");
  const auto *font = argument<const char *>(state, 2);
  const auto *text = argument<const char *>(state, 3);
  const auto x = argument<float>(state, 4);
  const auto y = argument<float>(state, 5);

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

        effect.xoffset = property<float>(state, -1, "xoffset", effect.xoffset);
        effect.yoffset = property<float>(state, -1, "yoffset", effect.yoffset);
        effect.scale = property<float>(state, -1, "scale", effect.scale);
        effect.angle = property<float>(state, -1, "angle", effect.angle);
        effect.alpha = property<float>(state, -1, "alpha", effect.alpha);

        if (index >= lenght) lenght = index + 1;
      }
    }

    lua_pop(state, 1);
  }

  self->render_label(font, text, x, y, std::span{effects.data(), lenght});
  return 0;
}

static int overlay_index(lua_State *state) {
  argument<void>(state, 1, "Overlay");
  const auto key = argument<std::string_view>(state, 2);

  if (key == "label") {
    lua_pushcfunction(state, overlay_label);
    return 1;
  }

  return lua_pushnil(state), 1;
}

overlay::overlay(std::string_view name) {
  const auto filename = std::format("overlays/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto label = std::format("@{}", filename);
  compile(L, buffer, label);

  pcall(L, 0, 1);

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

  _on_loop = acquire(L, -1, "on_loop");
  _on_paint = acquire(L, -1, "on_paint");

  lua_pop(L, 1);

  pushuserdata(L, this, "Overlay");
  _userdata_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  SDL_AddEventWatch(on_event, this);
  SDL_StartTextInput(SDL_GetRenderWindow(renderer));
}

overlay::~overlay() {
  SDL_StopTextInput(SDL_GetRenderWindow(renderer));
  SDL_RemoveEventWatch(on_event, this);
  release(L, _on_paint);
  release(L, _on_loop);
  release(L, _reference);
  release(L, _userdata_reference);
}

void overlay::wire() {
  metatable(L, "Overlay", overlay_index);
}

void overlay::set_foreground(std::string_view name) {
  if (_foreground)
    _foreground->disappear();

  _foreground = std::make_unique<foreground>(name);
  _foreground->expose();
  _foreground->appear();
}

void overlay::update(float delta) {
  if (_foreground)
    _foreground->update(delta);

  invoke(L, _on_loop, _reference, delta);
}

void overlay::draw() {
  if (_foreground)
    _foreground->draw();

  invoke(L, _on_paint, _reference);
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
