#pragma once

class fontpool final {
public:
  font* get(std::string_view family);

  void clear();

private:
  entt::dense_map<entt::id_type, std::unique_ptr<font>> _pool;
};
