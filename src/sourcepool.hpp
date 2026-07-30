#pragma once

class sourcepool final {
public:
  void insert(std::string_view name);

private:
  struct hash final {
    using is_transparent = void;

    std::size_t operator()(std::string_view value) const noexcept;
  };

  entt::dense_map<std::string, std::vector<uint8_t>, hash, std::equal_to<>> _pool;
};
