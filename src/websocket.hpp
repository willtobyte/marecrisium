#pragma once

struct message {
  std::string topic;
  std::string payload;
};

template <typename T, size_t N = 256>
class ringbuffer final {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");

public:
  void push(T item) noexcept {
    const auto head = _head.load(std::memory_order_relaxed);
    const auto tail = _tail.load(std::memory_order_acquire);
    if (head - tail == N)
      return;
    _data[head & MASK] = std::move(item);
    _head.store(head + 1, std::memory_order_release);
  }

  [[nodiscard]] bool try_pop(T& out) noexcept {
    const auto tail = _tail.load(std::memory_order_relaxed);
    const auto head = _head.load(std::memory_order_acquire);
    if (tail == head) return false;
    out = std::move(_data[tail & MASK]);
    _tail.store(tail + 1, std::memory_order_release);
    return true;
  }

private:
  static constexpr size_t MASK = N - 1;
  alignas(64) std::atomic<size_t> _head{0};
  alignas(64) std::atomic<size_t> _tail{0};
  std::array<T, N> _data{};
};

struct netloc {
  explicit netloc(std::string_view url);

  std::string host;
  std::string path;
  int port{443};
  bool ssl{true};
};

class subscription;

class socketconn final {
public:
  explicit socketconn(std::string_view url);
  ~socketconn();

  socketconn(const socketconn&) = delete;
  socketconn& operator=(const socketconn&) = delete;
  socketconn(socketconn&&) = delete;
  socketconn& operator=(socketconn&&) = delete;

  void send(message message) noexcept;
  void poll();

  void add_subscription(subscription* subscription);
  void remove_subscription(subscription* subscription);

  void set_on_connect(int ref) noexcept;
  void set_on_disconnect(int ref) noexcept;

private:
  friend int lws_callback(struct lws*, enum lws_callback_reasons, void*, void*, size_t);

  void fire(int ref);
  void run();
  void connect();
  void reconnect();
  void resubscribe();

  std::string _url;
  netloc _netloc;

  struct lws_context* _context{nullptr};
  std::atomic<struct lws*> _wsi{nullptr};
  std::atomic<bool> _connected{false};
  std::atomic<bool> _pending_connect{false};
  std::atomic<bool> _pending_disconnect{false};

  ringbuffer<message> _inbound;
  ringbuffer<message> _outbound;

  std::mutex _mutex;
  std::unordered_map<std::string, std::vector<subscription*>, transparent_hash, std::equal_to<>> _subscriptions;
  std::vector<uint8_t> _sendbuffer;

  std::atomic<bool> _stop{false};
  std::atomic<bool> _pending_ping{false};
  std::thread _thread;

  int _on_connect{LUA_NOREF};
  int _on_disconnect{LUA_NOREF};
};

class subscription final {
public:
  subscription(socketconn* owner, std::string topic, int callback_ref);
  ~subscription();

  subscription(const subscription&) = delete;
  subscription& operator=(const subscription&) = delete;
  subscription(subscription&&) = delete;
  subscription& operator=(subscription&&) = delete;

  void publish(lua_State* state, int idx);
  void unsubscribe();

  [[nodiscard]] const std::string& topic() const noexcept;
  [[nodiscard]] int callback() const noexcept;
  [[nodiscard]] bool active() const noexcept;

private:
  friend class socketconn;
  socketconn* _owner;
  std::string _topic;
  int _callback{LUA_NOREF};
  bool _active{true};
};

namespace websocket {
  void wire();
  void poll();
}
