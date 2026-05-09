inline static constexpr int32_t index(int32_t row, int32_t column, int32_t width) noexcept {
  return row * width + column;
}

inline static constexpr int32_t row(int32_t cell, int32_t width) noexcept {
  return cell / width;
}

inline static constexpr int32_t column(int32_t cell, int32_t width) noexcept {
  return cell % width;
}

inline static b2Vec2 center(int32_t cell, int32_t width, float size) noexcept {
  const auto x = static_cast<float>(column(cell, width));
  const auto y = static_cast<float>(row(cell, width));
  const auto half = size * .5f;
  return {x * size + half, y * size + half};
}

inline static float octile(int32_t c, int32_t r, int32_t gc, int32_t gr) noexcept {
  static constexpr float SQRT2MINUS1 = .41421356f;
  const auto dc = static_cast<float>(std::abs(c - gc));
  const auto dr = static_cast<float>(std::abs(r - gr));
  return dc > dr ? dc + SQRT2MINUS1 * dr : dr + SQRT2MINUS1 * dc;
}

static float hit(b2ShapeId, b2Vec2, b2Vec2, float, void* userdata) noexcept {
  *static_cast<bool*>(userdata) = true;
  return .0f;
}

static bool snap(int32_t& column, int32_t& row, int32_t width, int32_t height, const uint16_t* noalias components) noexcept {
  const auto i = static_cast<size_t>(row * width + column);
  if (components[i] != 0xFFFFu) return true;

  static constexpr auto MAXSNAP = 4;
  for (int32_t r = 1; r <= MAXSNAP; ++r) {
    for (int32_t dr = -r; dr <= r; ++dr) {
      for (int32_t dc = -r; dc <= r; ++dc) {
        if (std::abs(dr) != r && std::abs(dc) != r) continue;
        const auto nr = row + dr;
        const auto nc = column + dc;
        if (nr < 0 || nr >= height || nc < 0 || nc >= width) continue;
        if (components[static_cast<size_t>(nr * width + nc)] != 0xFFFFu) {
          column = nc;
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

static void buffers(tilemap::layer& layer, size_t capacity) {
  layer.vertices.reserve(capacity * 4);
  layer.indices.resize(capacity * 6);

  auto* noalias indices = layer.indices.data();

  [[assume(indices != nullptr)]];
  [[assume(capacity > 0)]];

  for (size_t i = 0; i < capacity; ++i) {
    const auto base = static_cast<int32_t>(i * 4);
    auto* ip = indices + i * 6;

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

  const auto aw = static_cast<float>(layer.atlas->width());
  const auto ah = static_cast<float>(layer.atlas->height());
  const auto us = size / aw;
  const auto vs = size / ah;
  const auto tpr = static_cast<size_t>(aw * inverse);
  const auto tpc = static_cast<size_t>(ah * inverse);
  const auto count = tpr * tpc;
  const auto htu = .5f / aw;
  const auto htv = .5f / ah;

  assert(tpr > 0 && tpc > 0 && "prepare: degenerate tile/atlas ratio");
  [[assume(tpr > 0)]];
  [[assume(tpc > 0)]];

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

tilemap::tilemap(std::string_view name, b2WorldId world) {
  const auto blob = io::read(std::format("tilemaps/{}.map", name));
  const auto* noalias bytes = blob.data();
  const auto length = blob.size();

  struct header final {
    uint32_t width;
    uint32_t height;
    float size;
    uint32_t radius;
    uint64_t hash;
  };
  static_assert(sizeof(header) == 24, "tilemap header layout must match on-disk size");
  static_assert(alignof(header) == 8, "tilemap header must be 8-byte aligned");

  static constexpr auto HEADER = sizeof(header);
  static constexpr auto PERCELL =
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

  [[assume(length == HEADER + PERCELL * n)]];

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

  {
    [[assume(_width > 0 && _height > 0)]];

    const auto* noalias collision = _collision.data();
    const auto columns = static_cast<size_t>(_width);
    const auto rows = static_cast<size_t>(_height);

    std::vector<bool> seen(n, false);

    for (size_t row = 0; row < rows; ++row) {
      const auto ro = row * columns;

      for (size_t column = 0; column < columns; ++column) {
        const auto index = ro + column;
        if (collision[index] == 0 || seen[index]) [[unlikely]]
          continue;

        auto rw = size_t{1};
        while (column + rw < columns && collision[index + rw] != 0 && !seen[index + rw])
          ++rw;

        auto rh = size_t{1};
        while (row + rh < rows) {
          const auto co = (row + rh) * columns + column;
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
          const auto base = (row + dy) * columns + column;
          for (size_t dx = 0; dx < rw; ++dx)
            seen[base + dx] = true;
        }

        const auto half = _size * .5f;
        const auto bhx = static_cast<float>(rw) * half;
        const auto bhy = static_cast<float>(rh) * half;

        auto bdef = b2DefaultBodyDef();
        bdef.type = b2_staticBody;
        bdef.position = {static_cast<float>(column) * _size + bhx, static_cast<float>(row) * _size + bhy};
        const auto sdef = b2DefaultShapeDef();
        const auto polygon = b2MakeBox(bhx, bhy);
        b2CreatePolygonShape(b2CreateBody(world, &bdef), &sdef, &polygon);
      }
    }
  }

  _world = world;
  _pathfinder.g.resize(n);
  _pathfinder.generations.resize(n, 0);
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

  if (_snapshot != viewport) [[unlikely]]
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
    _snapshot = viewport;
    _dirty = false;
  }
}

void tilemap::draw_foreground() {
  if (!_foreground.atlas) [[unlikely]]
    return;

  if (_dirty)
    tessellate(_foreground);

  _snapshot = viewport;
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

  ++_pathfinder.generation;
  if (_pathfinder.generation == 0) [[unlikely]] {
    std::ranges::fill(_pathfinder.generations, 0u);
    _pathfinder.generation = 1;
  }

  const auto generation = _pathfinder.generation;

  auto* noalias costs = _pathfinder.g.data();
  auto* noalias generations = _pathfinder.generations.data();
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
  _pathfinder.heap.emplace_back(octile(sc, sr, ec, er), start, ++_pathfinder.tiebreak);
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
    const auto cc = column(current, _width);

    if (top.f > cg + octile(cc, cr, ec, er) + .001f) continue;

    const auto gr = row(goal, _width);
    const auto gc = column(goal, _width);
    const auto dr = gr - cr;
    const auto dc = gc - cc;
    const auto absdr = std::abs(dr);
    const auto absdc = std::abs(dc);

    for (size_t d = 0; d < 8; ++d) {
      const auto k = static_cast<int32_t>(jumps[d][ci]);
      if (k == 0) continue;

      const auto sdr = DR[d];
      const auto sdc = DC[d];
      const auto maxsteps = std::abs(k);
      const auto stepcost = (d < 4) ? 1.f : SQRT2;

      const auto collinear = dr * sdc == dc * sdr;
      const auto codirectional = dr * sdr + dc * sdc > 0;
      const auto distance = (collinear && codirectional) ? std::max(absdr, absdc) : 0;

      int32_t target;
      float cost;
      if (distance > 0 && distance <= maxsteps) {
        target = goal;
        cost = stepcost * static_cast<float>(distance);
      } else if (k > 0) {
        target = current + k * (sdr * _width + sdc);
        cost = stepcost * static_cast<float>(k);
      } else {
        continue;
      }

      const auto ti = static_cast<size_t>(target);
      const auto newg = cg + cost;
      if (generations[ti] == generation && newg >= costs[ti]) continue;

      costs[ti] = newg;
      generations[ti] = generation;
      parents[ti] = current;
      const auto tr = row(target, _width);
      const auto tc = column(target, _width);
      _pathfinder.heap.emplace_back(
        newg + octile(tc, tr, ec, er),
        target,
        ++_pathfinder.tiebreak
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

  lua_newtable(state);

  auto index = 1;
  for (const auto cell : _pathfinder.path) {
    const auto pos = center(cell, _width, _size);
    lua_createtable(state, 2, 0);
    lua_pushnumber(state, static_cast<lua_Number>(pos.x));
    lua_rawseti(state, -2, 1);
    lua_pushnumber(state, static_cast<lua_Number>(pos.y));
    lua_rawseti(state, -2, 2);
    lua_rawseti(state, -2, index++);
  }

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
