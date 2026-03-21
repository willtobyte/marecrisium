#pragma once

class foreground final {
public:
  foreground() = default;
  ~foreground() = default;

  void update(float delta);

  void draw() const;
};
