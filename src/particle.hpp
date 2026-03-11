#pragma once

class pixmap;

struct config final {
  size_t count{};
  std::pair<float, float> xspawn{}, yspawn{};
  std::pair<float, float> radius{}, angle{};
  std::pair<float, float> scale{1.f, 1.f}, life{1.f, 1.f};
  std::pair<float, float> xvel{}, yvel{};
  std::pair<float, float> gx{}, gy{};
  std::pair<float, float> rforce{}, rvel{};
};

class particle final {
public:
  particle() = delete;

  particle(const config& cfg, const pixmap& texture, float x, float y, bool active);

  ~particle() = default;

  particle(particle&&) noexcept = default;
  particle& operator=(particle&&) noexcept = default;

  void update(float delta) noexcept;
  void draw(float camera_x, float camera_y) const noexcept;

  [[nodiscard]] float x() const noexcept;
  void set_x(float value) noexcept;
  [[nodiscard]] float y() const noexcept;
  void set_y(float value) noexcept;
  [[nodiscard]] bool active() const noexcept;
  void set_active(bool value) noexcept;

private:
  const pixmap* _texture;
  float _x;
  float _y;
  float _hw;
  float _hh;
  bool _active;
  size_t _count;

  std::vector<float> _px, _py, _vx, _vy, _gx, _gy;
  std::vector<float> _life, _scale, _angle, _av, _af;
  std::vector<size_t> _respawn;

  mutable std::vector<SDL_Vertex> _vertices;
  std::vector<int> _indices;

  std::uniform_real_distribution<float> _xspawnd, _yspawnd, _radiusd, _angled;
  std::uniform_real_distribution<float> _xveld, _yveld, _gxd, _gyd;
  std::uniform_real_distribution<float> _scaled, _lifed, _rotforced, _rotveld;

  static std::mt19937& rng() noexcept;
};
