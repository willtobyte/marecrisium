#pragma once

class fontpool;

class overlay final {
public:
  overlay(std::string_view name, fontpool& fonts);
  ~overlay();

  void update(float delta);

  void draw() const;

  void wire();

  static void unwire();

  void render_label(std::string_view family, std::string_view text, float x, float y) const;

private:
  std::string _name;
  fontpool& _fontpool;
  int _reference = LUA_NOREF;
};
