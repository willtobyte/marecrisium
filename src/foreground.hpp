#pragma once

class foreground final {
public:
  explicit foreground(std::string_view name);
  ~foreground();

  foreground(const foreground&) = delete;
  foreground& operator=(const foreground&) = delete;

  void update(float delta);

  void draw();

  void appear();

  void disappear();

  int _table_ref{LUA_NOREF};

  pixmap *_texture{nullptr};
  std::vector<SDL_Vertex> _vertices;
  std::vector<int32_t> _indices;

private:
  bool _visible{false};
  int _on_loop_ref{LUA_NOREF};
  int _on_paint_ref{LUA_NOREF};
  int _on_appear_ref{LUA_NOREF};
  int _on_disappear_ref{LUA_NOREF};
};
