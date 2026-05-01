inline static constexpr int32_t index(int32_t row, int32_t col, int32_t width) noexcept {
  return row * width + col;
}

inline static constexpr int32_t row(int32_t cell, int32_t width) noexcept {
  return cell / width;
}

inline static constexpr int32_t col(int32_t cell, int32_t width) noexcept {
  return cell % width;
}

inline static b2Vec2 center(int32_t cell, int32_t width, float size) noexcept {
  const auto x = static_cast<float>(col(cell, width));
  const auto y = static_cast<float>(row(cell, width));
  const auto half = size * .5f;
  return {x * size + half, y * size + half};
}

inline static float octile(int32_t c, int32_t r, int32_t goal_c, int32_t goal_r) noexcept {
  static constexpr float SQRT2_MINUS_1 = .41421356f;
  const auto dc = static_cast<float>(std::abs(c - goal_c));
  const auto dr = static_cast<float>(std::abs(r - goal_r));
  return dc > dr ? dc + SQRT2_MINUS_1 * dr : dr + SQRT2_MINUS_1 * dc;
}

inline static int32_t reach(int32_t d, int32_t step_dr, int32_t step_dc,
                                            int32_t delta_r, int32_t delta_c) noexcept {
  if (d < 4) {
    if (step_dr == 0) {
      if (delta_r == 0 && delta_c * step_dc > 0)
        return std::abs(delta_c);
    } else {
      if (delta_c == 0 && delta_r * step_dr > 0)
        return std::abs(delta_r);
    }
    return 0;
  }
  if (delta_r * step_dr > 0 && delta_c * step_dc > 0 && std::abs(delta_r) == std::abs(delta_c))
    return std::abs(delta_c);
  return 0;
}

static float hit(b2ShapeId, b2Vec2, b2Vec2, float, void* user_data) noexcept {
  *static_cast<bool*>(user_data) = true;
  return .0f;
}

static bool snap(int32_t& col, int32_t& row, int32_t width, int32_t height, const uint16_t* noalias components) noexcept {
  const auto i = static_cast<size_t>(row * width + col);
  if (components[i] != 0xFFFFu) return true;

  static constexpr int32_t MAX_SNAP = 4;
  for (int32_t r = 1; r <= MAX_SNAP; ++r) {
    for (int32_t dr = -r; dr <= r; ++dr) {
      for (int32_t dc = -r; dc <= r; ++dc) {
        if (std::abs(dr) != r && std::abs(dc) != r) continue;
        const auto nr = row + dr;
        const auto nc = col + dc;
        if (nr < 0 || nr >= height || nc < 0 || nc >= width) continue;
        if (components[static_cast<size_t>(nr * width + nc)] != 0xFFFFu) {
          col = nc;
          row = nr;
          return true;
        }
      }
    }
  }
  return false;
}

static bool vacant(const std::vector<uint32_t>& tiles) noexcept {
  for (const auto t : tiles)
    if (t != 0) return false;
  return true;
}

static void unwrap(tilemap::layer& layer, float size, float inverse) {
  const auto aw = static_cast<float>(layer.atlas->width());
  const auto ah = static_cast<float>(layer.atlas->height());
  const auto us = size / aw;
  const auto vs = size / ah;
  const auto tpr = static_cast<size_t>(aw * inverse);
  const auto tpc = static_cast<size_t>(ah * inverse);
  const auto count = tpr * tpc;
  const auto htu = .5f / aw;
  const auto htv = .5f / ah;

  layer.uvs.resize(count);
  for (size_t id = 0; id < count; ++id) {
    const auto column = static_cast<float>(id % tpr);
    const auto row = static_cast<float>(id / tpr);
    layer.uvs[id] = uv{
      column * us + htu,
      row * vs + htv,
      (column + 1.f) * us - htu,
      (row + 1.f) * vs - htv,
    };
  }
}

static void buffers(tilemap::layer& layer, size_t capacity) {
  layer.vertices.reserve(capacity * 4);
  layer.indices.resize(capacity * 6);

  for (size_t i = 0; i < capacity; ++i) {
    const auto base = static_cast<int32_t>(i * 4);
    auto* ip = layer.indices.data() + i * 6;

    *ip++ = base;
    *ip++ = base + 1;
    *ip++ = base + 2;
    *ip++ = base;
    *ip++ = base + 2;
    *ip++ = base + 3;
  }
}

static void prepare(tilemap::layer& layer, std::string_view name, std::string_view path, float size, float inverse) {
  if (layer.tiles.empty())
    return;

  layer.atlas = depot->pixmap.get(std::format("tilemaps/{}/{}", name, path));
  unwrap(layer, size, inverse);
}

static void pack(b2WorldId world, const uint8_t* noalias collision, int32_t width, int32_t height, float size) {
  [[assume(width > 0 && height > 0)]];

  const auto w = static_cast<size_t>(width);
  const auto h = static_cast<size_t>(height);
  const auto n = w * h;

  std::vector<bool> seen(n, false);

  for (size_t row = 0; row < h; ++row) {
    const auto ro = row * w;

    for (size_t column = 0; column < w; ++column) {
      const auto index = ro + column;
      if (collision[index] == 0 || seen[index]) [[unlikely]]
        continue;

      auto rw = size_t{1};
      while (column + rw < w && collision[index + rw] != 0 && !seen[index + rw])
        ++rw;

      auto rh = size_t{1};
      while (row + rh < h) {
        const auto co = (row + rh) * w + column;
        auto valid = true;

        for (size_t dx = 0; dx < rw; ++dx) {
          if (collision[co + dx] == 0 || seen[co + dx]) [[unlikely]] {
            valid = false;
            break;
          }
        }

        if (!valid)
          break;

        ++rh;
      }

      for (size_t dy = 0; dy < rh; ++dy) {
        const auto base = (row + dy) * w + column;
        for (size_t dx = 0; dx < rw; ++dx)
          seen[base + dx] = true;
      }

      const auto half = size * .5f;
      const auto bhx = static_cast<float>(rw) * half;
      const auto bhy = static_cast<float>(rh) * half;

      auto bdef = b2DefaultBodyDef();
      bdef.type = b2_staticBody;
      bdef.position = {static_cast<float>(column) * size + bhx, static_cast<float>(row) * size + bhy};
      const auto sdef = b2DefaultShapeDef();
      const auto polygon = b2MakeBox(bhx, bhy);
      b2CreatePolygonShape(b2CreateBody(world, &bdef), &sdef, &polygon);
    }
  }
}

static void emit(lua_State* state, const std::vector<int32_t>& path, int32_t width, float size) {
  lua_newtable(state);
  int index = 1;
  for (const auto cell : path) {
    const auto pos = center(cell, width, size);
    lua_createtable(state, 2, 0);
    lua_pushnumber(state, static_cast<lua_Number>(pos.x));
    lua_rawseti(state, -2, 1);
    lua_pushnumber(state, static_cast<lua_Number>(pos.y));
    lua_rawseti(state, -2, 2);
    lua_rawseti(state, -2, index++);
  }
}

tilemap::tilemap(std::string_view name, b2WorldId world) {
  const auto blob = io::read(std::format("tilemaps/{}.map", name));
  const auto* noalias bytes = blob.data();
  const auto bytes_size = blob.size();

  struct header final {
    uint32_t width;
    uint32_t height;
    float size;
    uint32_t radius_tiles;
    uint64_t source_hash;
  };
  static_assert(sizeof(header) == 24);
  static_assert(alignof(header) == 8);

  static constexpr auto HEADER = sizeof(header);
  static constexpr auto PER_CELL =
    sizeof(uint32_t) +
    sizeof(uint32_t) +
    sizeof(uint8_t) +
    sizeof(uint16_t) +
    8 * sizeof(int16_t);

  header h{};
  std::memcpy(&h, bytes, HEADER);

  _width = static_cast<int32_t>(h.width);
  _height = static_cast<int32_t>(h.height);
  _size = h.size;

  assert(_size > 0.f && "tilemap: invalid tile size");
  _inverse = 1.f / _size;

  const auto n = static_cast<size_t>(_width) * static_cast<size_t>(_height);

  [[assume(bytes_size == HEADER + PER_CELL * n)]];

  auto offset = HEADER;

  _background.tiles.resize(n);
  std::memcpy(_background.tiles.data(), bytes + offset, n * sizeof(uint32_t));
  offset += n * sizeof(uint32_t);

  _foreground.tiles.resize(n);
  std::memcpy(_foreground.tiles.data(), bytes + offset, n * sizeof(uint32_t));
  offset += n * sizeof(uint32_t);

  _collision.resize(n);
  std::memcpy(_collision.data(), bytes + offset, n * sizeof(uint8_t));
  offset += n * sizeof(uint8_t);

  _components.resize(n);
  std::memcpy(_components.data(), bytes + offset, n * sizeof(uint16_t));
  offset += n * sizeof(uint16_t);

  for (size_t d = 0; d < 8; ++d) {
    _jump[d].resize(n);
    std::memcpy(_jump[d].data(), bytes + offset, n * sizeof(int16_t));
    offset += n * sizeof(int16_t);
  }

  if (vacant(_background.tiles))
    _background.tiles.clear();
  if (vacant(_foreground.tiles))
    _foreground.tiles.clear();

  pack(world, _collision.data(), _width, _height, _size);

  _world = world;
  _pathfinder.g.resize(n);
  _pathfinder.generation.resize(n, 0);
  _pathfinder.parent.resize(n);
  _pathfinder.heap.reserve(n);

  prepare(_background, name, "background", _size, _inverse);
  prepare(_foreground, name, "foreground", _size, _inverse);

  const auto tx = static_cast<size_t>(viewport.width * _inverse) + 2;
  const auto ty = static_cast<size_t>(viewport.height * _inverse) + 2;
  const auto capacity = tx * ty;

  if (_background.atlas)
    buffers(_background, capacity);
  if (_foreground.atlas)
    buffers(_foreground, capacity);
}

void tilemap::draw_background() {
  if (!_background.atlas) [[unlikely]]
    return;

  if (_viewport_snapshot != viewport) [[unlikely]]
    _dirty = true;

  if (_dirty)
    tessellate(_background);

  const auto nv = static_cast<int>(_background.vertices.size());
  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_background.atlas),
    _background.vertices.data(),
    nv,
    _background.indices.data(),
    nv / 4 * 6
  );

  if (!_foreground.atlas) {
    _viewport_snapshot = viewport;
    _dirty = false;
  }
}

void tilemap::draw_foreground() {
  if (!_foreground.atlas) [[unlikely]]
    return;

  if (_dirty)
    tessellate(_foreground);

  _viewport_snapshot = viewport;
  _dirty = false;

  const auto nv = static_cast<int>(_foreground.vertices.size());
  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_foreground.atlas),
    _foreground.vertices.data(),
    nv,
    _foreground.indices.data(),
    nv / 4 * 6
  );
}

bool tilemap::greater(const node& a, const node& b) noexcept {
  if (a.f != b.f) return a.f > b.f;
  return a.tiebreak > b.tiebreak;
}

int tilemap::pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) {
  if (_components.empty() || _width == 0 || _height == 0) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  [[assume(_width > 0 && _height > 0)]];

  static constexpr int32_t DC[] = {1, -1, 0, 0, 1, 1, -1, -1};
  static constexpr int32_t DR[] = {0, 0, 1, -1, 1, -1, 1, -1};
  static constexpr float SQRT2 = 1.41421356f;

  auto sc = std::clamp(static_cast<int32_t>(x1 * _inverse), 0, _width - 1);
  auto sr = std::clamp(static_cast<int32_t>(y1 * _inverse), 0, _height - 1);
  auto ec = std::clamp(static_cast<int32_t>(x2 * _inverse), 0, _width - 1);
  auto er = std::clamp(static_cast<int32_t>(y2 * _inverse), 0, _height - 1);

  const auto* noalias components = _components.data();

  if (!snap(sc, sr, _width, _height, components) || !snap(ec, er, _width, _height, components)) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  const auto start = index(sr, sc, _width);
  const auto goal = index(er, ec, _width);

  if (start == goal ||
      components[static_cast<size_t>(start)] != components[static_cast<size_t>(goal)]) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  ++_pathfinder.current_generation;
  if (_pathfinder.current_generation == 0) [[unlikely]] {
    std::ranges::fill(_pathfinder.generation, 0u);
    _pathfinder.current_generation = 1;
  }

  const auto generation = _pathfinder.current_generation;

  auto* noalias costs = _pathfinder.g.data();
  auto* noalias generations = _pathfinder.generation.data();
  auto* noalias parents = _pathfinder.parent.data();
  const std::array<const int16_t* noalias, 8> jumps = {
    _jump[0].data(), _jump[1].data(), _jump[2].data(), _jump[3].data(),
    _jump[4].data(), _jump[5].data(), _jump[6].data(), _jump[7].data(),
  };

  [[assume(costs != nullptr)]];
  [[assume(generations != nullptr)]];
  [[assume(parents != nullptr)]];

  costs[static_cast<size_t>(start)] = .0f;
  generations[static_cast<size_t>(start)] = generation;
  parents[static_cast<size_t>(start)] = -1;

  _pathfinder.heap.clear();
  _pathfinder.heap.emplace_back(octile(sc, sr, ec, er), start, ++_pathfinder.tiebreak_counter);
  std::ranges::push_heap(_pathfinder.heap, greater);

  while (!_pathfinder.heap.empty()) {
    std::ranges::pop_heap(_pathfinder.heap, greater);
    const auto top = _pathfinder.heap.back();
    _pathfinder.heap.pop_back();

    const auto current = top.index;
    if (current == goal) break;

    const auto ci = static_cast<size_t>(current);
    const auto cg = costs[ci];
    const auto cr = row(current, _width);
    const auto cc_col = col(current, _width);

    if (top.f > cg + octile(cc_col, cr, ec, er) + .001f) continue;

    const auto gr = row(goal, _width);
    const auto gc_col = col(goal, _width);
    const auto delta_r = gr - cr;
    const auto delta_c = gc_col - cc_col;

    for (size_t d = 0; d < 8; ++d) {
      const auto k = static_cast<int32_t>(jumps[d][ci]);
      if (k == 0) continue;

      const auto step_dr = DR[d];
      const auto step_dc = DC[d];
      const auto max_steps = std::abs(k);
      const auto step_cost = (d < 4) ? 1.f : SQRT2;

      const auto goal_distance = reach(static_cast<int32_t>(d), step_dr, step_dc, delta_r, delta_c);

      int32_t target;
      float cost;
      if (goal_distance > 0 && goal_distance <= max_steps) {
        target = goal;
        cost = step_cost * static_cast<float>(goal_distance);
      } else if (k > 0) {
        target = current + k * (step_dr * _width + step_dc);
        cost = step_cost * static_cast<float>(k);
      } else {
        continue;
      }

      const auto ti = static_cast<size_t>(target);
      const auto new_g = cg + cost;
      if (generations[ti] == generation && new_g >= costs[ti]) continue;

      costs[ti] = new_g;
      generations[ti] = generation;
      parents[ti] = current;
      const auto target_r = row(target, _width);
      const auto target_c = col(target, _width);
      _pathfinder.heap.emplace_back(
        new_g + octile(target_c, target_r, ec, er),
        target,
        ++_pathfinder.tiebreak_counter
      );
      std::ranges::push_heap(_pathfinder.heap, greater);
    }
  }

  _pathfinder.path.clear();
  if (generations[static_cast<size_t>(goal)] == generation &&
      parents[static_cast<size_t>(goal)] != -1) [[likely]] {
    for (auto current = goal; current != -1; current = parents[static_cast<size_t>(current)])
      _pathfinder.path.emplace_back(current);
    std::ranges::reverse(_pathfinder.path);
  }

  if (_pathfinder.path.size() > 2) {
    const auto filter = b2DefaultQueryFilter();
    b2ShapeProxy proxy{};
    proxy.count = 1;
    proxy.radius = std::max(.0f, radius);
    size_t write = 1;
    for (size_t read = 1, end = _pathfinder.path.size() - 1; read < end; ++read) {
      const auto a = center(_pathfinder.path[write - 1], _width, _size);
      const auto c = center(_pathfinder.path[read + 1], _width, _size);
      proxy.points[0] = a;
      const b2Vec2 translation = {c.x - a.x, c.y - a.y};

      auto blocked = false;
      b2World_CastShape(_world, &proxy, translation, filter, hit, &blocked);

      if (blocked)
        _pathfinder.path[write++] = _pathfinder.path[read];
    }
    _pathfinder.path[write++] = _pathfinder.path.back();
    _pathfinder.path.resize(write);
  }

  emit(state, _pathfinder.path, _width, _size);
  return 1;
}

void tilemap::tessellate(layer& layer) {
  const auto sc = std::max(0, static_cast<int32_t>(viewport.x * _inverse));
  const auto sr = std::max(0, static_cast<int32_t>(viewport.y * _inverse));
  const auto ec = std::min(_width - 1, static_cast<int32_t>((viewport.x + viewport.width) * _inverse) + 1);
  const auto er = std::min(_height - 1, static_cast<int32_t>((viewport.y + viewport.height) * _inverse) + 1);

  if (sc > ec || sr > er) [[unlikely]] {
    layer.vertices.clear();
    return;
  }

  const auto capacity = static_cast<size_t>((ec - sc + 1) * (er - sr + 1));
  layer.vertices.resize(capacity * 4);

  auto* vp = layer.vertices.data();

  const SDL_FColor white{1.f, 1.f, 1.f, 1.f};

  auto ro = sr * _width;
  auto dy = static_cast<float>(sr) * _size - viewport.y;

  for (auto row = sr; row <= er; ++row, ro += _width, dy += _size) {
    const auto y1 = dy + _size;

    for (auto column = sc; column <= ec; ++column) {
      const auto ti = layer.tiles[static_cast<size_t>(ro + column)];
      if (ti == 0) [[unlikely]]
        continue;

      assert(static_cast<size_t>(ti - 1) < layer.uvs.size() && "tile index out of bounds");
      const auto& uv = layer.uvs[ti - 1];
      const auto x0 = static_cast<float>(column) * _size - viewport.x;
      const auto x1 = x0 + _size;

      *vp++ = SDL_Vertex{{x0, dy}, white, {uv.u0, uv.v0}};
      *vp++ = SDL_Vertex{{x1, dy}, white, {uv.u1, uv.v0}};
      *vp++ = SDL_Vertex{{x1, y1}, white, {uv.u1, uv.v1}};
      *vp++ = SDL_Vertex{{x0, y1}, white, {uv.u0, uv.v1}};
    }
  }

  layer.vertices.resize(static_cast<size_t>(vp - layer.vertices.data()));
}
