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

  void dismiss();

private:
  foreground *_foreground{nullptr};
  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<foreground>> _foregrounds;
  int _userdata_ref{LUA_NOREF};
  int _ref{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_paint{LUA_NOREF};
};
