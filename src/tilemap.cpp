#include "tilemap.hpp"

static int32_t to_column(float x, float inverse, int32_t width) noexcept {
  return std::clamp(static_cast<int32_t>(x * inverse), 0, width - 1);
}

static int32_t to_row(float y, float inverse, int32_t height) noexcept {
  return std::clamp(static_cast<int32_t>(y * inverse), 0, height - 1);
}

static void load_tiles(tilemap::layer& layer, const char* field, size_t total) {
  lua_getfield(L, -1, field);
  if (!lua_istable(L, -1)) {
    lua_pop(L, 1);
    return;
  }

  layer.tiles.reserve(total);
  for (size_t i = 0; i < total; ++i) {
    lua_rawgeti(L, -1, static_cast<int>(i + 1));
    const auto value = lua_isnumber(L, -1) ? static_cast<uint32_t>(lua_tonumber(L, -1)) : 0u;
    lua_pop(L, 1);
    layer.tiles.emplace_back(value);
  }

  lua_pop(L, 1);
  if (std::ranges::none_of(layer.tiles, std::identity{}))
    layer.tiles.clear();
}

static void load_atlas(tilemap::layer& layer, std::string_view name, std::string_view path, float size, float inverse) {
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
  for (size_t id = 0; id < count; ++id) {
    const auto column = static_cast<float>(id % tpr);
    const auto row = static_cast<float>(id / tpr);
    layer.uvs.emplace_back(uv{
      column * us + htu,
      row * vs + htv,
      (column + 1.f) * us - htu,
      (row + 1.f) * vs - htv
    });
  }
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

  _inverse = 1.f / _size;

  const auto total = static_cast<size_t>(_width) * static_cast<size_t>(_height);

  load_tiles(_background, "background", total);
  load_tiles(_foreground, "foreground", total);

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

    std::vector<uint8_t> visited(total);
    const auto* noalias tiles = _collision.data();
    auto* noalias data = visited.data();

    for (size_t row = 0; row < h; ++row) {
      const auto ro = row * w;

      for (size_t column = 0; column < w; ++column) {
        const auto index = ro + column;
        if (tiles[index] == 0 || data[index]) [[likely]]
          continue;

        auto rw = size_t{1};
        while (column + rw < w &&
               tiles[index + rw] != 0 &&
               !data[index + rw]) {
          ++rw;
        }

        auto rh = size_t{1};
        while (row + rh < h) {
          const auto co = (row + rh) * w + column;
          auto valid = true;

          for (size_t dx = 0; dx < rw; ++dx) {
            if (tiles[co + dx] == 0 || data[co + dx]) [[likely]] {
              valid = false;
              break;
            }
          }

          if (!valid)
            break;

          ++rh;
        }

        for (size_t dy = 0; dy < rh; ++dy) {
          std::memset(data + (row + dy) * w + column, 1, rw);
        }

        const auto half  = _size * .5f;
        const auto bhx = static_cast<float>(rw)  * half;
        const auto bhy = static_cast<float>(rh) * half;

        auto bdef = b2DefaultBodyDef();
        bdef.type     = b2_staticBody;
        bdef.position = {static_cast<float>(column) * _size + bhx,
                         static_cast<float>(row)    * _size + bhy};
        const auto sdef    = b2DefaultShapeDef();
        const auto polygon = b2MakeBox(bhx, bhy);
        b2CreatePolygonShape(b2CreateBody(world, &bdef), &sdef, &polygon);
      }
    }
  }

  lua_pop(L, 1);

  load_atlas(_background, name, "background", _size, _inverse);
  load_atlas(_foreground, name, "foreground", _size, _inverse);

  {
    const auto tx = static_cast<size_t>(viewport.width * _inverse) + 2;
    const auto ty = static_cast<size_t>(viewport.height * _inverse) + 2;
    const auto capacity = tx * ty;
    if (_background.atlas) {
      _background.vertices.reserve(capacity * 4);
      _background.indices.reserve(capacity * 6);
    }
    if (_foreground.atlas) {
      _foreground.vertices.reserve(capacity * 4);
      _foreground.indices.reserve(capacity * 6);
    }
  }
}

void tilemap::draw_background() noexcept {
  if (!_background.atlas) [[unlikely]]
    return;

  if (_viewport_x != viewport.x
      || _viewport_y != viewport.y
      || _viewport_width != viewport.width
      || _viewport_height != viewport.height) [[unlikely]]
    _dirty = true;

  if (_dirty)
    build_layer(_background);

  if (_background.vertices.empty()) [[unlikely]]
    return;

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_background.atlas),
    _background.vertices.data(),
    static_cast<int>(_background.vertices.size()),
    _background.indices.data(),
    static_cast<int>(_background.indices.size())
  );

  if (!_foreground.atlas) {
    _viewport_x = viewport.x;
    _viewport_y = viewport.y;
    _viewport_width = viewport.width;
    _viewport_height = viewport.height;
    _dirty = false;
  }
}

void tilemap::draw_foreground() noexcept {
  if (!_foreground.atlas) [[unlikely]]
    return;

  if (_dirty)
    build_layer(_foreground);

  _viewport_x = viewport.x;
  _viewport_y = viewport.y;
  _viewport_width = viewport.width;
  _viewport_height = viewport.height;
  _dirty = false;

  if (_foreground.vertices.empty()) [[unlikely]]
    return;

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_foreground.atlas),
    _foreground.vertices.data(),
    static_cast<int>(_foreground.vertices.size()),
    _foreground.indices.data(),
    static_cast<int>(_foreground.indices.size())
  );
}

int tilemap::pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) noexcept {
  if (_collision.empty() || _width == 0 || _height == 0) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  const auto sc = to_column(x1, _inverse, _width);
  const auto sr = to_row(y1, _inverse, _height);
  const auto ec = to_column(x2, _inverse, _width);
  const auto er = to_row(y2, _inverse, _height);

  if (sc == ec && sr == er) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  const auto margin = static_cast<int32_t>(radius * _inverse);
  const auto padding = margin + 8;

  const auto bx0 = std::max(0, std::min(sc, ec) - padding);
  const auto by0 = std::max(0, std::min(sr, er) - padding);
  const auto bx1 = std::min(_width - 1, std::max(sc, ec) + padding);
  const auto by1 = std::min(_height - 1, std::max(sr, er) + padding);

  const auto bw = bx1 - bx0 + 1;
  const auto bh = by1 - by0 + 1;
  const auto lt = static_cast<size_t>(bw) * static_cast<size_t>(bh);

  _pathfinder.local_blocked.resize(lt);
  auto* noalias blocked = _pathfinder.local_blocked.data();
  const auto* noalias collision = _collision.data();

  if (margin == 0) {
    for (int32_t row = 0; row < bh; ++row) {
      const auto go = static_cast<size_t>((row + by0) * _width + bx0);
      const auto lo = static_cast<size_t>(row * bw);
      std::memcpy(blocked + lo, collision + go, static_cast<size_t>(bw));
    }
  } else {
    std::memset(blocked, 0, lt);

    const auto sx0 = std::max(0, bx0 - margin);
    const auto sy0 = std::max(0, by0 - margin);
    const auto sx1 = std::min(_width - 1, bx1 + margin);
    const auto sy1 = std::min(_height - 1, by1 + margin);

    for (int32_t row = sy0; row <= sy1; ++row) {
      const auto ro = static_cast<size_t>(row) * static_cast<size_t>(_width);
      for (int32_t column = sx0; column <= sx1; ++column) {
        if (collision[ro + static_cast<size_t>(column)] == 0)
          continue;

        const auto mr0 = std::max(by0, row - margin);
        const auto mr1 = std::min(by1, row + margin);
        const auto mc0 = std::max(bx0, column - margin);
        const auto mc1 = std::min(bx1, column + margin);

        for (auto r = mr0; r <= mr1; ++r) {
          const auto lro = static_cast<size_t>(r - by0) * static_cast<size_t>(bw);
          std::memset(blocked + lro + static_cast<size_t>(mc0 - bx0), 1,
                      static_cast<size_t>(mc1 - mc0 + 1));
        }
      }
    }
  }

  const auto lsc = sc - bx0;
  const auto lsr = sr - by0;
  const auto lec = ec - bx0;
  const auto ler = er - by0;

  const auto start = lsr * bw + lsc;
  const auto goal = ler * bw + lec;

  if (blocked[static_cast<size_t>(start)] != 0 || blocked[static_cast<size_t>(goal)] != 0) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  if (_pathfinder.g.size() < lt) {
    _pathfinder.g.resize(lt);
    _pathfinder.generation.resize(lt, 0);
    _pathfinder.parent.resize(lt);
    _pathfinder.current_generation = 0;
  }

  ++_pathfinder.current_generation;
  if (_pathfinder.current_generation == 0) [[unlikely]] {
    std::fill(_pathfinder.generation.begin(), _pathfinder.generation.end(), 0u);
    _pathfinder.current_generation = 1;
  }

  const auto generation = _pathfinder.current_generation;
  auto* noalias costs = _pathfinder.g.data();
  auto* noalias generations = _pathfinder.generation.data();
  auto* noalias parents = _pathfinder.parent.data();

  _pathfinder.path.clear();
  _pathfinder.heap.clear();

  const auto si = static_cast<size_t>(start);
  costs[si] = .0f;
  generations[si] = generation;
  parents[si] = -1;

  constexpr int32_t DC[] = { 1, -1, 0, 0, 1, 1, -1, -1 };
  constexpr int32_t DR[] = { 0, 0, 1, -1, 1, -1, 1, -1 };
  constexpr float COST[] = { 1.f, 1.f, 1.f, 1.f, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f };

  constexpr auto SQRT2_MINUS_1 = .41421356f;

  const auto octile = [lec, ler](int32_t column, int32_t row) noexcept -> float {
    const auto dc = static_cast<float>(std::abs(column - lec));
    const auto dr = static_cast<float>(std::abs(row - ler));
    return dc > dr
      ? dc + SQRT2_MINUS_1 * dr
      : dr + SQRT2_MINUS_1 * dc;
  };

  _pathfinder.heap.push_back({octile(lsc, lsr), start});

  const auto compare = [](const node& a, const node& b) noexcept { return a.f > b.f; };

  auto iterations = lt;
  while (!_pathfinder.heap.empty() && iterations > 0) {
    --iterations;
    std::pop_heap(_pathfinder.heap.begin(), _pathfinder.heap.end(), compare);
    const auto [priority, current] = _pathfinder.heap.back();
    _pathfinder.heap.pop_back();

    if (current == goal) break;

    const auto ci = static_cast<size_t>(current);
    if (generations[ci] == generation &&
        priority > costs[ci] + octile(current % bw, current / bw))
      continue;

    const auto cr = current / bw;
    const auto cc = current % bw;
    const auto cg = costs[ci];

    for (int direction = 0; direction < 8; ++direction) {
      const auto nc = cc + DC[direction];
      const auto nr = cr + DR[direction];

      if (static_cast<uint32_t>(nc) >= static_cast<uint32_t>(bw) ||
          static_cast<uint32_t>(nr) >= static_cast<uint32_t>(bh)) [[unlikely]]
        continue;

      const auto ni = static_cast<size_t>(nr * bw + nc);

      if (blocked[ni] != 0)
        continue;

      if (direction >= 4) {
        if (blocked[static_cast<size_t>(cr * bw + nc)] != 0 ||
            blocked[static_cast<size_t>(nr * bw + cc)] != 0)
          continue;
      }

      const auto ng = cg + COST[direction];

      if (generations[ni] == generation && ng >= costs[ni])
        continue;

      costs[ni] = ng;
      generations[ni] = generation;
      parents[ni] = current;

      _pathfinder.heap.push_back({ng + octile(nc, nr), static_cast<int32_t>(ni)});
      std::push_heap(_pathfinder.heap.begin(), _pathfinder.heap.end(), compare);
    }
  }

  if (generations[static_cast<size_t>(goal)] == generation
      && parents[static_cast<size_t>(goal)] != -1) [[likely]] {
    for (auto current = goal; current != -1; current = parents[static_cast<size_t>(current)])
      _pathfinder.path.emplace_back(current);
    std::reverse(_pathfinder.path.begin(), _pathfinder.path.end());
  }

  lua_newtable(state);
  const auto half = _size * .5f;
  int index = 1;
  for (const auto cell : _pathfinder.path) {
    const auto lc = cell % bw;
    const auto lr = cell / bw;
    const auto gc = lc + bx0;
    const auto gr = lr + by0;
    lua_createtable(state, 2, 0);
    lua_pushnumber(state, static_cast<lua_Number>(gc * _size + half));
    lua_rawseti(state, -2, 1);
    lua_pushnumber(state, static_cast<lua_Number>(gr * _size + half));
    lua_rawseti(state, -2, 2);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

void tilemap::build_layer(layer& layer) noexcept {
  layer.vertices.clear();
  layer.indices.clear();

  const auto sc = std::max(0, static_cast<int32_t>(viewport.x * _inverse));
  const auto sr = std::max(0, static_cast<int32_t>(viewport.y * _inverse));
  const auto ec = std::min(_width - 1, static_cast<int32_t>((viewport.x + viewport.width) * _inverse) + 1);
  const auto er = std::min(_height - 1, static_cast<int32_t>((viewport.y + viewport.height) * _inverse) + 1);

  if (sc > ec || sr > er) [[unlikely]]
    return;

  constexpr SDL_FColor white{1.f, 1.f, 1.f, 1.f};

  auto ro = sr * _width;
  auto dy = static_cast<float>(sr) * _size - viewport.y;

  for (auto row = sr; row <= er; ++row, ro += _width, dy += _size) {
    const auto y0 = dy;
    const auto y1 = dy + _size;

    for (auto column = sc; column <= ec; ++column) {
      const auto ti = layer.tiles[static_cast<size_t>(ro + column)];
      if (ti == 0) [[likely]]
        continue;

      const auto& entry = layer.uvs[ti - 1];
      const auto dx = static_cast<float>(column) * _size - viewport.x;
      const auto x0 = dx;
      const auto x1 = dx + _size;
      const auto base = static_cast<int32_t>(layer.vertices.size());

      layer.vertices.emplace_back(SDL_Vertex{{x0, y0}, white, {entry.u0, entry.v0}});
      layer.vertices.emplace_back(SDL_Vertex{{x1, y0}, white, {entry.u1, entry.v0}});
      layer.vertices.emplace_back(SDL_Vertex{{x1, y1}, white, {entry.u1, entry.v1}});
      layer.vertices.emplace_back(SDL_Vertex{{x0, y1}, white, {entry.u0, entry.v1}});

      layer.indices.emplace_back(base);
      layer.indices.emplace_back(base + 1);
      layer.indices.emplace_back(base + 2);
      layer.indices.emplace_back(base);
      layer.indices.emplace_back(base + 2);
      layer.indices.emplace_back(base + 3);
    }
  }
}
