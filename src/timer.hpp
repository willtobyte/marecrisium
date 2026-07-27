#pragma once

namespace timer {
  class group final {
  public:
    group();
    ~group() noexcept;

    group(const group&) = delete;
    group& operator=(const group&) = delete;

    void activate() const noexcept;

  private:
    unsigned _id;

    friend class scope;
  };

  class scope final {
  public:
    explicit scope(const group& owner) noexcept;
    ~scope() noexcept;

    scope(const scope&) = delete;
    scope& operator=(const scope&) = delete;

  private:
    unsigned _prior;
  };

  void wire();
  void update(double delta);
}
