#pragma once

class pixmappool;
class soundpool;
class sourcepool;
class stage;

class director final {
public:
  director();
  ~director();

  void wire();

  void navigate(std::string_view name);

  void destroy(std::string_view name);

  void preload(std::string_view name);

  void transition();

  void update(float delta);

  void draw() const;

private:
  stage *_current = nullptr;
  std::optional<std::string> _pending;
  std::unordered_map<std::string, std::unique_ptr<stage>, transparent_hash, std::equal_to<>> _stages;
  std::unique_ptr<pixmappool> _pixmappool;
  std::unique_ptr<soundpool> _soundpool;
  std::unique_ptr<sourcepool> _sourcepool;
};
