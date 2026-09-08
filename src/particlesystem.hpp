#pragma once

class particleemitter;

class particlesystem final {
public:
  particleemitter* add(std::string_view name, std::string_view kind, float x, float y, bool active);

  void update(float delta);

  void draw();

  void clear();

private:
  std::unordered_map<std::string, particleemitter, transparent_string_hash, std::equal_to<>> _particles;
  std::vector<particleemitter*> _order;
  std::vector<int> _indices;
};
