#pragma once

template<typename T>
struct overwrite : std::allocator<T> {
  using std::allocator<T>::allocator;

  template<typename U>
  struct rebind final {
    using other = overwrite<U>;
  };

  template<typename U, typename... Args>
  void construct(U *ptr, Args&&... args) {
    if constexpr (sizeof...(Args) == 0)
      ::new (static_cast<void *>(ptr)) U;
    else
      std::construct_at(ptr, std::forward<Args>(args)...);
  }
};

using bytes = std::vector<uint8_t, overwrite<uint8_t>>;

class io final {
public:
  io() = delete;
  ~io() = delete;

  static void mount(std::string_view filename);
  static bool exists(std::string_view filename) noexcept;
  static bytes read(std::string_view filename);
};
