#include "tilemap.hpp"


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

  assert(_size > 0.f && "tilemap: invalid tile size");
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

  _world = world;
  _pathfinder.generation.resize(total, 0);
  _pathfinder.parent.resize(total);
  _pathfinder.queue.reserve(total);

  lua_pop(L, 1);

  prepare(_background, name, "background", _size, _inverse);
  prepare(_foreground, name, "foreground", _size, _inverse);

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
    tessellate(_background);

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
    tessellate(_foreground);

  _viewport_x = viewport.x;
  _viewport_y = viewport.y;
  _viewport_width = viewport.width;
  _viewport_height = viewport.height;
  _dirty = false;

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_foreground.atlas),
    _foreground.vertices.data(),
    static_cast<int>(_foreground.vertices.size()),
    _foreground.indices.data(),
    static_cast<int>(_foreground.indices.size())
  );
}

int tilemap::pathfind(lua_State* state, float x1, float y1, float x2, float y2, float) noexcept {
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
  auto* noalias generations = _pathfinder.generation.data();
  auto* noalias parents = _pathfinder.parent.data();
  const auto* noalias collision = _collision.data();

  constexpr int32_t DC[] = {1, -1, 0, 0, 1, 1, -1, -1};
  constexpr int32_t DR[] = {0, 0, 1, -1, 1, -1, 1, -1};

  _pathfinder.queue.clear();
  _pathfinder.queue.emplace_back(start);
  generations[static_cast<size_t>(start)] = generation;
  parents[static_cast<size_t>(start)] = -1;

  for (size_t head = 0; head < _pathfinder.queue.size(); ++head) {
    const auto current = _pathfinder.queue[head];
    if (current == goal) break;

    const auto cr = current / _width;
    const auto cc = current % _width;

    for (int d = 0; d < 8; ++d) {
      const auto nc = cc + DC[d];
      const auto nr = cr + DR[d];

      if (static_cast<uint32_t>(nc) >= static_cast<uint32_t>(_width) ||
          static_cast<uint32_t>(nr) >= static_cast<uint32_t>(_height))
        continue;

      const auto ni = nr * _width + nc;
      if (collision[ni] != 0 || generations[ni] == generation)
        continue;

      if (d >= 4 && (collision[cr * _width + nc] != 0 || collision[nr * _width + cc] != 0))
        continue;

      generations[ni] = generation;
      parents[ni] = current;
      _pathfinder.queue.emplace_back(ni);
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

  const auto filter = b2DefaultQueryFilter();
  for (size_t i = 0; i + 2 < _pathfinder.path.size();) {
    const auto a = to_world(_pathfinder.path[i]);
    const auto b = to_world(_pathfinder.path[i + 2]);
    const b2Vec2 translation = {b.x - a.x, b.y - a.y};

    auto blocked = false;
    b2World_CastRay(_world, a, translation, filter,
      +[](b2ShapeId, b2Vec2, b2Vec2, float, void* userdata) -> float {
        *static_cast<bool*>(userdata) = true;
        return .0f;
      }, &blocked);

    if (!blocked)
      _pathfinder.path.erase(_pathfinder.path.begin() + static_cast<ptrdiff_t>(i + 1));
    else
      ++i;
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

void tilemap::tessellate(layer& layer) noexcept {
  const auto sc = std::max(0, static_cast<int32_t>(viewport.x * _inverse));
  const auto sr = std::max(0, static_cast<int32_t>(viewport.y * _inverse));
  const auto ec = std::min(_width - 1, static_cast<int32_t>((viewport.x + viewport.width) * _inverse) + 1);
  const auto er = std::min(_height - 1, static_cast<int32_t>((viewport.y + viewport.height) * _inverse) + 1);

  if (sc > ec || sr > er) [[unlikely]] {
    layer.vertices.clear();
    layer.indices.clear();
    return;
  }

  const auto capacity = static_cast<size_t>((ec - sc + 1) * (er - sr + 1));
  layer.vertices.resize(capacity * 4);
  layer.indices.resize(capacity * 6);

  auto* noalias vertices = layer.vertices.data();
  auto* noalias indices = layer.indices.data();

  constexpr SDL_FColor white{1.f, 1.f, 1.f, 1.f};

  auto visible = 0uz;
  auto ro = sr * _width;
  auto dy = static_cast<float>(sr) * _size - viewport.y;

  for (auto row = sr; row <= er; ++row, ro += _width, dy += _size) {
    const auto y0 = dy;
    const auto y1 = dy + _size;

    for (auto column = sc; column <= ec; ++column) {
      const auto ti = layer.tiles[static_cast<size_t>(ro + column)];
      if (ti == 0) [[likely]]
        continue;

      assert(static_cast<size_t>(ti - 1) < layer.uvs.size() && "tile index out of bounds");
      const auto& entry = layer.uvs[ti - 1];
      const auto dx = static_cast<float>(column) * _size - viewport.x;
      const auto x0 = dx;
      const auto x1 = dx + _size;
      const auto base = static_cast<int32_t>(visible * 4);

      auto* vertex = vertices + visible * 4;
      vertex[0] = {{x0, y0}, white, {entry.u0, entry.v0}};
      vertex[1] = {{x1, y0}, white, {entry.u1, entry.v0}};
      vertex[2] = {{x1, y1}, white, {entry.u1, entry.v1}};
      vertex[3] = {{x0, y1}, white, {entry.u0, entry.v1}};

      auto* index = indices + visible * 6;
      index[0] = base;
      index[1] = base + 1;
      index[2] = base + 2;
      index[3] = base;
      index[4] = base + 2;
      index[5] = base + 3;

      ++visible;
    }
  }

  layer.vertices.resize(visible * 4);
  layer.indices.resize(visible * 6);
}
