#pragma once

class foreground final {
public:
  explicit foreground(std::string_view name);
  ~foreground();

  static void wire();

  void update(float delta);

  void draw();

  void expose();

  struct drawcall {
    pixmap *texture;
    float x;
    float y;
    float width;
    float height;
    uint8_t alpha{255};
    double angle{};
  };

  void enqueue(pixmap *texture, float x, float y, float width, float height, uint8_t alpha, double angle);

  int _reference{LUA_NOREF};

private:
  std::vector<drawcall> _batch;
  int _on_loop{LUA_NOREF};
  int _on_paint{LUA_NOREF};
  int _userdata_reference{LUA_NOREF};
};
