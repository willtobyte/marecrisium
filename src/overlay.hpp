#pragma once

class foreground;

class overlay final {
public:
  explicit overlay(std::string_view name);
  ~overlay();

  static void wire();

  void update(float delta);

  void draw();

  void expose();

  void set_foreground(std::string_view name);

  void render_label(std::string_view family, std::string_view text, float x, float y);

  void render_label(std::string_view family, std::string_view text, float x, float y, std::span<const glypheffect> effects);

private:
  std::unique_ptr<foreground> _foreground;
  int _userdata_ref{LUA_NOREF};
  int _ref{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_paint{LUA_NOREF};
};
