#pragma once

class stage;

class director final {
public:
  void wire();

  void navigate(std::string name);

  void destroy(std::string_view name);

  template<typename T>
    requires std::convertible_to<T, std::string>
  void enroll(T&& name) {
    const auto key = entt::hashed_string{name.data(), name.size()};
    auto instance = std::make_unique<stage>(std::forward<T>(name));
    const auto [_, inserted] = _stages.emplace(key, std::move(instance));
    assert(inserted && "stage must not already be enrolled");
    [[assume(inserted)]];
  }

  void transition();

  void update(float delta);

  void draw();

private:
  stage *_current{nullptr};
  overlay _overlay{};

  std::optional<std::string> _pending;

  entt::dense_map<entt::id_type, std::unique_ptr<stage>> _stages;
};
