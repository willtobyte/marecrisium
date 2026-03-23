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

  void preload(std::string name);

  void reset();

  void set_overlay(std::string name);

  void clear_overlay();

  void transition();

  void on_tick(uint64_t tick);

  void update(float delta);

  void draw();

private:
  stage *_current{nullptr};
  overlay *_overlay{nullptr};
  std::optional<std::string> _pending;
  std::unordered_map<std::string, std::unique_ptr<stage>, transparent_hash, std::equal_to<>> _stages;
  std::unordered_map<std::string, std::unique_ptr<overlay>, transparent_hash, std::equal_to<>> _overlays;
};
