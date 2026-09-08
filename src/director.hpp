#pragma once

class scene;

class director final {
public:
  void wire();

  void navigate(std::string name);

  void destroy(std::string_view name);

  void enroll(std::string name);

  void update(float delta);

  void draw();

private:
  scene *_current{nullptr};
  std::optional<std::string> _pending;

  std::unordered_map<std::string, std::optional<scene>, transparent_string_hash, std::equal_to<>> _scenes;
};
