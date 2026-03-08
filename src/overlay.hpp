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

  void enqueue(std::string_view font, std::string_view text, float x, float y);

private:
  std::string _name;
  fontpool& _fontpool;
  int _reference = LUA_NOREF;

  struct entry {
    std::string font;
    std::string text;
    float x;
    float y;
  };

  mutable std::vector<entry> _labels;
};
