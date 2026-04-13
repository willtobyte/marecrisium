#pragma once

class overlay;
class stage;

class director final {
public:
  director() = default;
  ~director() = default;

  void wire();

  void navigate(std::string name);

  void destroy(std::string_view name);

  void enroll(std::string name);

  void set_overlay(std::string_view name);

  void clear_overlay();

  void transition();

  void on_tick(uint64_t tick);

  void update(float delta);

  void draw();

private:
  stage *_current{nullptr};
  overlay *_overlay{nullptr};

  std::optional<std::string> _pending;

  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<stage>> _stages;
  ankerl::unordered_dense::map<entt::id_type, std::unique_ptr<overlay>> _overlays;
};
