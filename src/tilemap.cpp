#include "tilemap.hpp"


static void ingest(tilemap::layer& layer, const char* field, size_t total) {
  lua_getfield(L, -1, field);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  layer.tiles.resize(total);
  auto* noalias out = layer.tiles.data();

  for (size_t i = 0; i < total; ++i) {
    lua_rawgeti(L, -1, static_cast<int>(i + 1));
    out[i] = lua_isnumber(L, -1) ? static_cast<uint32_t>(lua_tonumber(L, -1)) : 0u;
    lua_pop(L, 1);
  }

  lua_pop(L, 1);

  if (std::ranges::none_of(layer.tiles, [](auto t) { return t != 0; }))
    layer.tiles.clear();
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

  layer.uvs.reserve(count);
  std::generate_n(std::back_inserter(layer.uvs), count, [&, id = 0uz]() mutable {
    const auto column = static_cast<float>(id % tpr);
    const auto row = static_cast<float>(id / tpr);
    ++id;
    return uv{column * us + htu, row * vs + htv, (column + 1.f) * us - htu, (row + 1.f) * vs - htv};
  });
}

tilemap::tilemap(std::string_view name, b2WorldId world) {
  const auto filename = std::format("tilemaps/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto chunk = std::format("@{}", filename);
  compile(L, buffer, chunk);

  pcall(L, 0, 1);

  lua_getfield(L, -1, "size");
  _size = lua_isnumber(L, -1) ? static_cast<float>(lua_tonumber(L, -1)) : .0f;
  lua_pop(L, 1);

  lua_getfield(L, -1, "width");
  _width = lua_isnumber(L, -1) ? static_cast<int>(lua_tonumber(L, -1)) : 0;
  lua_pop(L, 1);

  lua_getfield(L, -1, "height");
  _height = lua_isnumber(L, -1) ? static_cast<int>(lua_tonumber(L, -1)) : 0;
  lua_pop(L, 1);

  assert(_size > 0.f && "tilemap: invalid tile size");
  _inverse = 1.f / _size;

  const auto total = static_cast<size_t>(_width) * static_cast<size_t>(_height);

  ingest(_background, "background", total);
  ingest(_foreground, "foreground", total);

  {
    _collision.resize(total);

    lua_getfield(L, -1, "collision");
    if (lua_istable(L, -1)) {
      for (size_t i = 0; i < total; ++i) {
        lua_rawgeti(L, -1, static_cast<int>(i + 1));
        const auto value = lua_isnumber(L, -1) ? static_cast<uint32_t>(lua_tonumber(L, -1)) : 0u;
        lua_pop(L, 1);
        _collision[i] = static_cast<uint8_t>(value);
      }
    }
    lua_pop(L, 1);

    const auto w = static_cast<size_t>(_width);
    const auto h = static_cast<size_t>(_height);

    std::vector<bool> seen(total, false);
    const auto* noalias tiles = _collision.data();

    for (size_t row = 0; row < h; ++row) {
      const auto ro = row * w;

      for (size_t column = 0; column < w; ++column) {
        const auto index = ro + column;
        if (tiles[index] == 0 || seen[index]) [[unlikely]]
          continue;

        auto rw = size_t{1};
        while (column + rw < w && tiles[index + rw] != 0 && !seen[index + rw]) {
          ++rw;
        }

        auto rh = size_t{1};
        while (row + rh < h) {
          const auto co = (row + rh) * w + column;
          auto valid = true;

          for (size_t dx = 0; dx < rw; ++dx) {
            if (tiles[co + dx] == 0 || seen[co + dx]) [[unlikely]] {
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
          for (size_t dx = 0; dx < rw; ++dx) seen[base + dx] = true;
        }

        const auto half  = _size * .5f;
        const auto bhx = static_cast<float>(rw)  * half;
        const auto bhy = static_cast<float>(rh) * half;

        auto bdef = b2DefaultBodyDef();
        bdef.type = b2_staticBody;
        bdef.position = {static_cast<float>(column) * _size + bhx, static_cast<float>(row)    * _size + bhy};
        const auto sdef = b2DefaultShapeDef();
        const auto polygon = b2MakeBox(bhx, bhy);
        b2CreatePolygonShape(b2CreateBody(world, &bdef), &sdef, &polygon);
      }
    }
  }

  _world = world;
  _pathfinder.g.resize(total);
  _pathfinder.generation.resize(total, 0);
  _pathfinder.parent.resize(total);
  _pathfinder.heap.reserve(total);

  lua_pop(L, 1);

  prepare(_background, name, "background", _size, _inverse);
  prepare(_foreground, name, "foreground", _size, _inverse);

  {
    const auto tx = static_cast<size_t>(viewport.width * _inverse) + 2;
    const auto ty = static_cast<size_t>(viewport.height * _inverse) + 2;
    const auto capacity = tx * ty;

    const auto allocate = [](layer& l, size_t capacity) {
      l.vertices.reserve(capacity * 4);
      l.indices.resize(capacity * 6);

      for (auto i = 0uz; i < capacity; ++i) {
        const auto base = static_cast<int32_t>(i * 4);

        auto* ip = l.indices.data() + i * 6;

        *ip++ = base; *ip++ = base + 1; *ip++ = base + 2;
        *ip++ = base; *ip++ = base + 2; *ip++ = base + 3;
      }
    };

    if (_background.atlas)
      allocate(_background, capacity);
    if (_foreground.atlas)
      allocate(_foreground, capacity);
  }
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

int tilemap::pathfind(lua_State* state, float x1, float y1, float x2, float y2, float) {
  if (_collision.empty() || _width == 0 || _height == 0) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  const auto sc = std::clamp(static_cast<int32_t>(x1 * _inverse), 0, _width - 1);
  const auto sr = std::clamp(static_cast<int32_t>(y1 * _inverse), 0, _height - 1);
  const auto ec = std::clamp(static_cast<int32_t>(x2 * _inverse), 0, _width - 1);
  const auto er = std::clamp(static_cast<int32_t>(y2 * _inverse), 0, _height - 1);

  const auto start = sr * _width + sc;
  const auto goal = er * _width + ec;

  if (start == goal ||
      _collision[static_cast<size_t>(start)] != 0 ||
      _collision[static_cast<size_t>(goal)] != 0) [[unlikely]] {
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
  const auto* noalias collision = _collision.data();

  static constexpr int32_t DC[] = {1, -1, 0, 0, 1, 1, -1, -1};
  static constexpr int32_t DR[] = {0, 0, 1, -1, 1, -1, 1, -1};
  static constexpr float COST[] = {1.f, 1.f, 1.f, 1.f, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f};
  static constexpr auto SQRT2_MINUS_1 = .41421356f;

  const auto octile = [ec, er](int32_t c, int32_t r) -> float {
    const auto dc = static_cast<float>(std::abs(c - ec));
    const auto dr = static_cast<float>(std::abs(r - er));
    return dc > dr ? dc + SQRT2_MINUS_1 * dr : dr + SQRT2_MINUS_1 * dc;
  };

  const auto compare = [](const tilemap::node& a, const tilemap::node& b) {
    return a.f > b.f;
  };

  costs[static_cast<size_t>(start)] = .0f;
  generations[static_cast<size_t>(start)] = generation;
  parents[static_cast<size_t>(start)] = -1;

  _pathfinder.heap.clear();
  _pathfinder.heap.emplace_back(octile(sc, sr), start);
  std::ranges::push_heap(_pathfinder.heap, compare);

  while (!_pathfinder.heap.empty()) {
    std::ranges::pop_heap(_pathfinder.heap, compare);
    const auto [f, current] = _pathfinder.heap.back();
    _pathfinder.heap.pop_back();

    if (current == goal) break;

    const auto ci = static_cast<size_t>(current);
    const auto cg = costs[ci];
    const auto cr = current / _width;
    const auto cc = current % _width;

    if (f > cg + octile(cc, cr) + .001f)
      continue;

    for (int d = 0; d < 8; ++d) {
      const auto nc = cc + DC[d];
      const auto nr = cr + DR[d];

      if (static_cast<uint32_t>(nc) >= static_cast<uint32_t>(_width) ||
          static_cast<uint32_t>(nr) >= static_cast<uint32_t>(_height))
        continue;

      const auto ni = static_cast<size_t>(nr * _width + nc);
      if (collision[ni] != 0)
        continue;

      if (d >= 4 && (collision[static_cast<size_t>(cr * _width + nc)] != 0 ||
                     collision[static_cast<size_t>(nr * _width + cc)] != 0))
        continue;

      const auto ng = cg + COST[d];
      if (generations[ni] == generation && ng >= costs[ni])
        continue;

      costs[ni] = ng;
      generations[ni] = generation;
      parents[ni] = current;
      _pathfinder.heap.emplace_back(ng + octile(nc, nr), static_cast<int32_t>(ni));
      std::ranges::push_heap(_pathfinder.heap, compare);
    }
  }

  _pathfinder.path.clear();
  if (generations[static_cast<size_t>(goal)] == generation &&
      parents[static_cast<size_t>(goal)] != -1) [[likely]] {
    for (auto current = goal; current != -1; current = parents[static_cast<size_t>(current)])
      _pathfinder.path.emplace_back(current);
    std::ranges::reverse(_pathfinder.path);
  }

  const auto half = _size * .5f;
  const auto to_world = [&](int32_t cell) -> b2Vec2 {
    const auto col = static_cast<float>(cell % _width);
    const auto row = static_cast<float>(cell / _width);
    return {col * _size + half, row * _size + half};
  };

  if (_pathfinder.path.size() > 2) {
    const auto filter = b2DefaultQueryFilter();
    size_t write = 1;
    for (size_t read = 1, end = _pathfinder.path.size() - 1; read < end; ++read) {
      const auto a = to_world(_pathfinder.path[write - 1]);
      const auto c = to_world(_pathfinder.path[read + 1]);
      const b2Vec2 translation = {c.x - a.x, c.y - a.y};

      auto hit = false;
      b2World_CastRay(_world, a, translation, filter,
        +[](b2ShapeId, b2Vec2, b2Vec2, float, void* ud) -> float {
          *static_cast<bool*>(ud) = true;
          return .0f;
        }, &hit);

      if (hit)
        _pathfinder.path[write++] = _pathfinder.path[read];
    }
    _pathfinder.path[write++] = _pathfinder.path.back();
    _pathfinder.path.resize(write);
  }

  lua_newtable(state);
  int index = 1;
  for (const auto cell : _pathfinder.path) {
    const auto pos = to_world(cell);
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
