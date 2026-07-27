#pragma once

class stage;

class director final {
public:
  director() = default;
  ~director() = default;

  void wire();

  void navigate(std::string name);

  void destroy(std::string_view name);

  template<typename T>
    requires std::convertible_to<T, std::string>
  void enroll(T&& name) {
    const auto key = entt::hashed_string{name.data(), name.size()};
    const auto [it, inserted] = _stages.try_emplace(key);
    if (inserted)
      it->second = std::make_unique<stage>(std::forward<T>(name));
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
