#pragma once

class foreground;

class overlay final {
public:
  overlay();
  ~overlay();

  static void wire();

  void show(std::string_view name);

  void hide(std::string_view name);

  void clear();

  void update(float delta);

  void draw();

private:
  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<foreground>> _foregrounds;
  std::vector<foreground *> _active;
  int _userdata_reference{LUA_NOREF};
};
