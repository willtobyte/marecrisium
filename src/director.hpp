#pragma once

#include "overlay.hpp"

class stage;

class director final {
public:
  director() = default;
  ~director() = default;

  void wire();

  template<typename T>
    requires std::convertible_to<T, std::string>
  void navigate(T&& name) { _pending = std::forward<T>(name); }

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

  void on_tick(uint64_t tick);

  void update(float delta);

  void draw();

private:
  stage *_current{nullptr};
  overlay _overlay{};

  std::optional<std::string> _pending;

  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<stage>> _stages;
};
