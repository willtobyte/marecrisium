#pragma once

class stage;

class director final {
public:
  director() = default;
  ~director() = default;

  void wire();

  void navigate(std::string_view name);

  void transition();

  void update(float delta);

  void draw() const;

private:
  stage *_current = nullptr;
  std::optional<std::string> _pending;
  std::unordered_map<std::string, std::unique_ptr<stage>, transparent_hash, std::equal_to<>> _stages;
};
