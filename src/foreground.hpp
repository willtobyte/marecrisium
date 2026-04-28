#pragma once

class foreground final {
public:
  explicit foreground(std::string_view name);
  ~foreground();

  static void wire();

  void update(float delta);

  void draw();

  void appear();

  void disappear();

  int _ref{LUA_NOREF};

  pixmap *_texture{nullptr};
  std::vector<SDL_Vertex> _vertices;
  std::vector<int32_t> _indices;

private:
  bool _visible{false};
  int _on_loop{LUA_NOREF};
  int _on_paint{LUA_NOREF};
  int _on_appear{LUA_NOREF};
  int _on_disappear{LUA_NOREF};
  int _userdata_ref{LUA_NOREF};
};
