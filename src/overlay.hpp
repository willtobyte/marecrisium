#pragma once

class overlay final {
public:
  explicit overlay(std::string_view name);
  ~overlay();

  void update(float delta);

  void draw() const;

  void wire();

  void render_label(std::string_view family, std::string_view text, float x, float y) const;

  void render_label(std::string_view family, std::string_view text, float x, float y, std::span<const glypheffect> effects) const;

private:
  std::string _name;
  int _reference = LUA_NOREF;
};
