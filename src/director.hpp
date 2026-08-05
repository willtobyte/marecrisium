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
    const auto key = entt::hashed_string{name.data(), name.size()};
    auto instance = std::make_unique<scene>(std::forward<T>(name));
    const auto [_, inserted] = _scenes.emplace(key, std::move(instance));
    assert(inserted && "scene must not already be enrolled");
    [[assume(inserted)]];
  }

  void transition();

  void update(float delta);

  void draw();

private:
  scene *_current{nullptr};
  std::optional<std::string> _pending;

  entt::dense_map<entt::id_type, std::unique_ptr<scene>> _scenes;
};
