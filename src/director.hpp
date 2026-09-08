#pragma once

class scene;

class director final {
public:
  void wire();

  void navigate(std::string_view name);

  void destroy(std::string_view name);

  void enroll(std::string_view name);

  void update(float delta);

  void draw();

private:
  scene *_current{nullptr};
  scene *_pending{nullptr};

  std::unordered_map<std::string, scene, transparent_string_hash, std::equal_to<>> _scenes;
};
