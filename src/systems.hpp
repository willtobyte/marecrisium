#pragma once

class systems final {
public:
  explicit systems(entt::registry& registry) noexcept : _registry{registry} {}

  void prepare(std::size_t capacity);

  entt::entity spawn(int pool, std::string_view kind, const std::string& label, float x, float y);

  [[nodiscard]] entt::entity pick(float x, float y) noexcept;

  void hover(entt::entity& hovered, entt::entity picked);

  void loop(float delta);

  void animate(float delta);

  void sort();

  void draw() const;

  void press(entt::entity hovered, int table, uint32_t& previous, uint32_t buttons, float mouse_x, float mouse_y, int on_press, int on_release);

private:
  entt::registry& _registry;
};
