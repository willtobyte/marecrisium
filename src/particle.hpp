#pragma once

void sincos(float x, float& sine, float& cosine) noexcept;

class pixmap;
class sound;

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

  particle(particle&&) noexcept = default;
  particle& operator=(particle&&) noexcept = default;

  void update(float delta) noexcept;
  void draw() noexcept;

  float x() const noexcept;
  void set_x(float value) noexcept;
  float y() const noexcept;
  void set_y(float value) noexcept;
  bool active() const noexcept;
  void set_active(bool value) noexcept;

  void set_sound(class sound* sound, float distance, float volume) noexcept;
  class sound* sound() const noexcept;

  static void wire();

private:
  size_t _count;

  const pixmap* _texture;
  class sound* _sound{nullptr};

  float _x;
  float _y;
  float _half_width;
  float _half_height;
  float _distance{300.f};
  float _inverse_distance{1.f / 300.f};
  float _volume{1.f};
  bool _active;

  std::vector<float> _position_x, _position_y, _velocity_x, _velocity_y, _gravity_x, _gravity_y;
  std::vector<float> _life, _scale, _angle, _angular_velocity, _angular_force;
  std::vector<size_t> _respawn;

  std::vector<SDL_Vertex> _vertices;
  std::vector<int> _indices;

  std::uniform_real_distribution<float> _spawn_x_distribution, _spawn_y_distribution, _radius_distribution, _angle_distribution;
  std::uniform_real_distribution<float> _velocity_x_distribution, _velocity_y_distribution, _gravity_x_distribution, _gravity_y_distribution;
  std::uniform_real_distribution<float> _scale_distribution, _life_distribution, _rotation_force_distribution, _rotation_velocity_distribution;

  static std::mt19937& rng() noexcept;
};
