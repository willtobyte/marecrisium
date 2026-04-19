#pragma once

void sincos(float x, float& sine, float& cosine);

class pixmap;

struct config final {
  size_t count{};
  std::pair<float, float> spawn_x{}, spawn_y{};
  std::pair<float, float> radius{}, angle{};
  std::pair<float, float> scale{1.f, 1.f}, life{1.f, 1.f};
  std::pair<float, float> velocity_x{}, velocity_y{};
  std::pair<float, float> gravity_x{}, gravity_y{};
  std::pair<float, float> rotation_force{}, rotation_velocity{};
};

class particle final {
public:
  particle() = delete;

  particle(const config& configuration, const pixmap& texture, float x, float y, bool active);

  ~particle() = default;

  particle(particle&&) = default;
  particle& operator=(particle&&) = default;

  void update(float delta);
  void draw();

  [[nodiscard]] float x() const;
  void set_x(float value);
  [[nodiscard]] float y() const;
  void set_y(float value);
  [[nodiscard]] bool active() const;
  void set_active(bool value);

  static void wire();

private:
  size_t _count;

  const pixmap* _texture;

  float _x;
  float _y;
  float _half_width;
  float _half_height;
  bool _active;

  std::vector<float> _position_x, _position_y, _velocity_x, _velocity_y, _gravity_x, _gravity_y;
  std::vector<float> _life, _scale, _angle, _angular_velocity, _angular_force;

  std::vector<SDL_Vertex> _vertices;
  std::vector<int> _indices;

  std::pair<float, float> _spawn_x_range, _spawn_y_range, _radius_range, _angle_range;
  std::pair<float, float> _velocity_x_range, _velocity_y_range, _gravity_x_range, _gravity_y_range;
  std::pair<float, float> _scale_range, _life_range, _rotation_force_range, _rotation_velocity_range;
};
