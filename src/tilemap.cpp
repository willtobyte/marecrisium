#include "tilemap.hpp"

static int32_t to_column(float x, float inverse_size, int32_t width) noexcept {
  return std::clamp(static_cast<int32_t>(x * inverse_size), 0, width - 1);
}

static int32_t to_row(float y, float inverse_size, int32_t height) noexcept {
  return std::clamp(static_cast<int32_t>(y * inverse_size), 0, height - 1);
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
    layer.tiles.emplace_back(static_cast<uint32_t>(lua_tonumber(L, -1)));
    lua_pop(L, 1);
  }

  lua_pop(L, 1);
  if (std::ranges::none_of(layer.tiles, [](const uint32_t id) { return id != 0; }))
    layer.tiles.clear();
}

static void load_atlas(tilemap::layer& layer, std::string_view name, const char* path, float size, float inverse_size) {
  if (layer.tiles.empty())
    return;

  layer.atlas = &depot->pixmap.get(std::format("tilemaps/{}/{}", name, path));
  const auto atlas_width = static_cast<float>(layer.atlas->width());
  const auto atlas_height = static_cast<float>(layer.atlas->height());
  const auto u_scale = size / atlas_width;
  const auto v_scale = size / atlas_height;
  const auto tiles_per_row = static_cast<size_t>(atlas_width * inverse_size);
  const auto tiles_per_column = static_cast<size_t>(atlas_height * inverse_size);
  const auto count = tiles_per_row * tiles_per_column;
  const auto half_texel_u = .5f / atlas_width;
  const auto half_texel_v = .5f / atlas_height;

  layer.uvs.reserve(count);
  for (size_t id = 0; id < count; ++id) {
    const auto column = static_cast<float>(id % tiles_per_row);
    const auto row = static_cast<float>(id / tiles_per_row);
    layer.uvs.emplace_back(uv{
      column * u_scale + half_texel_u,
      row * v_scale + half_texel_v,
      (column + 1.f) * u_scale - half_texel_u,
      (row + 1.f) * v_scale - half_texel_v
    });
  }
}

tilemap::tilemap(std::string_view name, b2WorldId world) {
  const auto filename = std::format("tilemaps/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto* data = reinterpret_cast<const char*>(buffer.data());
  const auto length = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, length, label.c_str()) != 0) [[unlikely]] {
    std::string error(lua_tostring(L, -1));
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error(lua_tostring(L, -1));
    lua_pop(L, 1);
    throw std::runtime_error{std::move(error)};
  }

  lua_getfield(L, -1, "size");
  _size = static_cast<float>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "width");
  _width = static_cast<int32_t>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "height");
  _height = static_cast<int32_t>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  _inverse_size = 1.f / _size;

  const auto total = static_cast<size_t>(_width) * static_cast<size_t>(_height);

  load_tiles(_background, "background", total);
  load_tiles(_foreground, "foreground", total);

  {
    _collision.resize(total);

    lua_getfield(L, -1, "collision");
    if (lua_istable(L, -1)) {
      for (size_t i = 0; i < total; ++i) {
        lua_rawgeti(L, -1, static_cast<int>(i + 1));
        _collision[i] = static_cast<uint8_t>(lua_tonumber(L, -1));
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);

    const auto half = _size * .5f;
    const auto w = static_cast<size_t>(_width);
    const auto h = static_cast<size_t>(_height);

    std::vector<uint8_t> visited(total);
    const auto* noalias tiles = _collision.data();
    auto* noalias visited_data = visited.data();

    for (size_t row = 0; row < h; ++row) {
      const auto row_offset = row * w;

      for (size_t column = 0; column < w; ++column) {
        const auto index = row_offset + column;
        if (tiles[index] == 0 || visited_data[index]) [[likely]]
          continue;

        auto run_width = size_t{1};
        while (column + run_width < w &&
               tiles[index + run_width] != 0 &&
               !visited_data[index + run_width]) {
          ++run_width;
        }

        auto run_height = size_t{1};
        while (row + run_height < h) {
          const auto check_offset = (row + run_height) * w + column;
          auto valid = true;

          for (size_t dx = 0; dx < run_width; ++dx) {
            if (tiles[check_offset + dx] == 0 || visited_data[check_offset + dx]) {
              valid = false;
              break;
            }
          }

          if (!valid)
            break;

          ++run_height;
        }

        for (size_t dy = 0; dy < run_height; ++dy) {
          std::memset(visited_data + (row + dy) * w + column, 1, run_width);
        }

        const auto box_hx = static_cast<float>(run_width) * half;
        const auto box_hy = static_cast<float>(run_height) * half;

        const auto position_x = static_cast<float>(column) * _size + box_hx;
        const auto position_y = static_cast<float>(row) * _size + box_hy;

        auto bdef = b2DefaultBodyDef();
        bdef.type = b2_staticBody;
        bdef.position = {position_x, position_y};
        const auto body_id = b2CreateBody(world, &bdef);

        const auto polygon = b2MakeBox(box_hx, box_hy);
        auto sdef = b2DefaultShapeDef();
        sdef.enableContactEvents = true;
        b2CreatePolygonShape(body_id, &sdef, &polygon);
      }
    }
  }

  lua_pop(L, 1);

  load_atlas(_background, name, "background", _size, _inverse_size);
  load_atlas(_foreground, name, "foreground", _size, _inverse_size);

  {
    const auto tiles_x = static_cast<size_t>(viewport.width * _inverse_size) + 2;
    const auto tiles_y = static_cast<size_t>(viewport.height * _inverse_size) + 2;
    const auto max_tiles = tiles_x * tiles_y;
    if (_background.atlas) {
      _background.vertices.reserve(max_tiles * 4);
      _background.indices.reserve(max_tiles * 6);
    }
    if (_foreground.atlas) {
      _foreground.vertices.reserve(max_tiles * 4);
      _foreground.indices.reserve(max_tiles * 6);
    }
  }
}

void tilemap::draw_background() noexcept {
  if (!_background.atlas) [[unlikely]]
    return;

  if (_viewport_x != viewport.x || _viewport_y != viewport.y || _viewport_width != viewport.width || _viewport_height != viewport.height)
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

int tilemap::pathfind(lua_State* state, float x1, float y1, float x2, float y2) noexcept {
  if (_collision.empty() || _width == 0 || _height == 0) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  const auto width  = static_cast<int32_t>(_width);
  const auto height = static_cast<int32_t>(_height);

  const auto start_column = to_column(x1, _inverse_size, width);
  const auto start_row    = to_row(y1, _inverse_size, height);
  const auto end_column   = to_column(x2, _inverse_size, width);
  const auto end_row      = to_row(y2, _inverse_size, height);

  if (start_column == end_column && start_row == end_row) {
    lua_newtable(state);
    return 1;
  }

  const auto index = [width](int32_t column, int32_t row) { return row * width + column; };
  const auto total = static_cast<size_t>(width * height);

  _pathfinder.g.assign(total, std::numeric_limits<float>::max());
  _pathfinder.parent.assign(total, -1);
  _pathfinder.path.clear();
  _pathfinder.heap.clear();

  const auto start = index(start_column, start_row);
  const auto goal  = index(end_column, end_row);

  _pathfinder.g[static_cast<size_t>(start)] = .0f;
  _pathfinder.heap.push_back({static_cast<float>(std::abs(end_column - start_column) + std::abs(end_row - start_row)), start});

  constexpr int32_t dx[8] = { 1, -1,  0,  0,  1,  1, -1, -1 };
  constexpr int32_t dy[8] = { 0,  0,  1, -1,  1, -1,  1, -1 };
  constexpr float   dc[8] = { 1.f, 1.f, 1.f, 1.f, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f };

  const auto cmp = [](const node& a, const node& b) noexcept { return a.f > b.f; };

  std::push_heap(_pathfinder.heap.begin(), _pathfinder.heap.end(), cmp);

  while (!_pathfinder.heap.empty()) {
    std::pop_heap(_pathfinder.heap.begin(), _pathfinder.heap.end(), cmp);
    const auto [f, current] = _pathfinder.heap.back();
    _pathfinder.heap.pop_back();

    if (current == goal) break;

    const auto current_row    = current / width;
    const auto current_column = current % width;

    for (int d = 0; d < 8; ++d) {
      const auto neighbor_column = current_column + dx[d];
      const auto neighbor_row    = current_row    + dy[d];

      if (neighbor_column < 0 || neighbor_column >= width || neighbor_row < 0 || neighbor_row >= height) [[unlikely]]
        continue;

      const auto neighbor_index   = index(neighbor_column, neighbor_row);
      const auto neighbor_index_u = static_cast<size_t>(neighbor_index);

      if (_collision[neighbor_index_u] != 0) continue;

      const auto cost = _pathfinder.g[static_cast<size_t>(current)] + dc[d];
      if (cost >= _pathfinder.g[neighbor_index_u]) continue;

      _pathfinder.g[neighbor_index_u]      = cost;
      _pathfinder.parent[neighbor_index_u] = current;

      const auto heuristic = static_cast<float>(std::abs(neighbor_column - end_column) + std::abs(neighbor_row - end_row));
      _pathfinder.heap.push_back({cost + heuristic, neighbor_index});
      std::push_heap(_pathfinder.heap.begin(), _pathfinder.heap.end(), cmp);
    }
  }

  if (_pathfinder.parent[static_cast<size_t>(goal)] != -1) {
    for (auto cur = goal; cur != -1; cur = _pathfinder.parent[static_cast<size_t>(cur)])
      _pathfinder.path.emplace_back(cur);
    std::reverse(_pathfinder.path.begin(), _pathfinder.path.end());
  }

  lua_newtable(state);
  const auto half = _size * .5f;
  int i = 1;
  for (const auto node_index : _pathfinder.path) {
    const auto wx = static_cast<float>(node_index % width) * _size + half;
    const auto wy = static_cast<float>(node_index / width) * _size + half;
    lua_newtable(state);
    lua_pushnumber(state, static_cast<double>(wx));
    lua_rawseti(state, -2, 1);
    lua_pushnumber(state, static_cast<double>(wy));
    lua_rawseti(state, -2, 2);
    lua_rawseti(state, -2, i++);
  }

  return 1;
}

void tilemap::build_layer(layer& layer) noexcept {
  layer.vertices.clear();
  layer.indices.clear();

  const auto start_column = std::max(0, static_cast<int32_t>(viewport.x * _inverse_size));
  const auto start_row = std::max(0, static_cast<int32_t>(viewport.y * _inverse_size));
  const auto end_column = std::min(_width - 1, static_cast<int32_t>((viewport.x + viewport.width) * _inverse_size) + 1);
  const auto end_row = std::min(_height - 1, static_cast<int32_t>((viewport.y + viewport.height) * _inverse_size) + 1);

  if (start_column > end_column || start_row > end_row) [[unlikely]]
    return;

  constexpr SDL_FColor white{1.f, 1.f, 1.f, 1.f};

  auto row_offset = start_row * _width;
  auto dy = static_cast<float>(start_row) * _size - viewport.y;

  for (auto row = start_row; row <= end_row; ++row, row_offset += _width, dy += _size) {
    const auto snapped_y0 = std::roundf(dy);
    const auto snapped_y1 = std::roundf(dy + _size);

    for (auto column = start_column; column <= end_column; ++column) {
      const auto tile_id = layer.tiles[static_cast<size_t>(row_offset + column)];
      if (tile_id == 0) [[likely]]
        continue;

      const auto& entry = layer.uvs[tile_id - 1];
      const auto dx = static_cast<float>(column) * _size - viewport.x;
      const auto snapped_x0 = std::roundf(dx);
      const auto snapped_x1 = std::roundf(dx + _size);
      const auto base = static_cast<int32_t>(layer.vertices.size());

      layer.vertices.emplace_back(SDL_Vertex{{snapped_x0, snapped_y0}, white, {entry.u0, entry.v0}});
      layer.vertices.emplace_back(SDL_Vertex{{snapped_x1, snapped_y0}, white, {entry.u1, entry.v0}});
      layer.vertices.emplace_back(SDL_Vertex{{snapped_x1, snapped_y1}, white, {entry.u1, entry.v1}});
      layer.vertices.emplace_back(SDL_Vertex{{snapped_x0, snapped_y1}, white, {entry.u0, entry.v1}});

      layer.indices.emplace_back(base);
      layer.indices.emplace_back(base + 1);
      layer.indices.emplace_back(base + 2);
      layer.indices.emplace_back(base);
      layer.indices.emplace_back(base + 2);
      layer.indices.emplace_back(base + 3);
    }
  }
}
