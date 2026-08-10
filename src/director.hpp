#pragma once

class scene;

class director final {
public:
  void wire();

  void navigate(std::string name);

  void destroy(std::string_view name);

  template<typename T>
    requires std::convertible_to<T, std::string>
  void enroll(T&& name) {
    std::string key{name};
    auto instance = std::make_unique<scene>(std::forward<T>(name));
    const auto [_, inserted] = _scenes.emplace(std::move(key), std::move(instance));
    assert(inserted && "scene must not already be enrolled");
    [[assume(inserted)]];
  }

  void update(float delta);

  void draw();

private:
  scene *_current{nullptr};
  std::optional<std::string> _pending;

  std::unordered_map<std::string, std::unique_ptr<scene>, transparent_string_hash, std::equal_to<>> _scenes;
};
