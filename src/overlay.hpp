#pragma once

class overlay final {
public:
  explicit overlay(std::string_view name);
  ~overlay();

  overlay(const overlay&) = delete;
  overlay& operator=(const overlay&) = delete;

  void update(float delta);

  void draw();

  void appear();

  void disappear();

private:
  int _table{LUA_NOREF};
  int _on_loop{LUA_NOREF};
  int _on_paint{LUA_NOREF};
  int _on_appear{LUA_NOREF};
  int _on_disappear{LUA_NOREF};
};
