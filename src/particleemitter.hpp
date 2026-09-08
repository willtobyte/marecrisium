#pragma once

class pixmap;

struct config final {
  size_t count{};
  struct {
    std::pair<float, float> x{}, y{};
  } spawn;
  std::pair<float, float> radius{}, angle{};
  std::pair<float, float> scale{1.f, 1.f}, life{1.f, 1.f};
  struct {
    std::pair<float, float> x{}, y{};
  } velocity;
  struct {
    std::pair<float, float> x{}, y{};
  } gravity;
  struct {
    std::pair<float, float> force{}, velocity{};
  } rotation;
};

class particleemitter final {
public:
  particleemitter(const config& config, const pixmap& texture, float x, float y, bool active);

  void update(float delta);

  void draw(const int* indices);

  float x() const;
  void set_x(float value);

  float y() const;
  void set_y(float value);

  bool active() const;
  void set_active(bool value);

  static void wire();

private:
  size_t _count;

  const pixmap* _texture;

  float _x;
  float _y;
  bool _active;
  bool _idle;

  struct {
    float width;
    float height;
  } _half;

  std::unique_ptr<float[]> _values;

  std::unique_ptr<SDL_Vertex[]> _vertices;

  std::pair<float, float> _spawn_x_range, _spawn_y_range, _radius_range, _angle_range;
  std::pair<float, float> _velocity_x_range, _velocity_y_range, _gravity_x_range, _gravity_y_range;
  std::pair<float, float> _scale_range, _life_range, _rotation_force_range, _rotation_velocity_range;
};
