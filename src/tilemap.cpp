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
  const auto label = std::format("@{}", filename);
  compile(L, buffer, label);

  pcall(L, 0, 1);

  _size = get<float>(L, -1, "size");
  _width = get<int>(L, -1, "width");
  _height = get<int>(L, -1, "height");

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

        const auto half  = _size * .5f;
        const auto box_hx = static_cast<float>(run_width)  * half;
        const auto box_hy = static_cast<float>(run_height) * half;

        auto bdef = b2DefaultBodyDef();
        bdef.type     = b2_staticBody;
        bdef.position = {static_cast<float>(column) * _size + box_hx,
                         static_cast<float>(row)    * _size + box_hy};
        const auto sdef    = b2DefaultShapeDef();
        const auto polygon = b2MakeBox(box_hx, box_hy);
        b2CreatePolygonShape(b2CreateBody(world, &bdef), &sdef, &polygon);
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

int tilemap::pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) noexcept {
  if (_collision.empty() || _width == 0 || _height == 0) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  const auto start_column = to_column(x1, _inverse_size, _width);
  const auto start_row = to_row(y1, _inverse_size, _height);
  const auto end_column = to_column(x2, _inverse_size, _width);
  const auto end_row = to_row(y2, _inverse_size, _height);

  if (start_column == end_column && start_row == end_row) {
    lua_newtable(state);
    return 1;
  }

  const auto margin = static_cast<int32_t>(radius * _inverse_size);
  const auto padding = margin + 8;

  const auto box_x0 = std::max(0, std::min(start_column, end_column) - padding);
  const auto box_y0 = std::max(0, std::min(start_row, end_row) - padding);
  const auto box_x1 = std::min(_width - 1, std::max(start_column, end_column) + padding);
  const auto box_y1 = std::min(_height - 1, std::max(start_row, end_row) + padding);

  const auto box_w = box_x1 - box_x0 + 1;
  const auto box_h = box_y1 - box_y0 + 1;
  const auto local_total = static_cast<size_t>(box_w) * static_cast<size_t>(box_h);

  _pathfinder.local_blocked.resize(local_total);
  auto* noalias blocked = _pathfinder.local_blocked.data();
  const auto* noalias collision = _collision.data();

  if (margin == 0) {
    for (int32_t row = 0; row < box_h; ++row) {
      const auto global_offset = static_cast<size_t>((row + box_y0) * _width + box_x0);
      const auto local_offset = static_cast<size_t>(row * box_w);
      std::memcpy(blocked + local_offset, collision + global_offset, static_cast<size_t>(box_w));
    }
  } else {
    std::memset(blocked, 0, local_total);

    const auto src_x0 = std::max(0, box_x0 - margin);
    const auto src_y0 = std::max(0, box_y0 - margin);
    const auto src_x1 = std::min(_width - 1, box_x1 + margin);
    const auto src_y1 = std::min(_height - 1, box_y1 + margin);

    for (int32_t row = src_y0; row <= src_y1; ++row) {
      const auto row_offset = static_cast<size_t>(row) * static_cast<size_t>(_width);
      for (int32_t column = src_x0; column <= src_x1; ++column) {
        if (collision[row_offset + static_cast<size_t>(column)] == 0)
          continue;

        const auto min_row = std::max(box_y0, row - margin);
        const auto max_row = std::min(box_y1, row + margin);
        const auto min_column = std::max(box_x0, column - margin);
        const auto max_column = std::min(box_x1, column + margin);

        for (auto er = min_row; er <= max_row; ++er) {
          const auto local_row_offset = static_cast<size_t>(er - box_y0) * static_cast<size_t>(box_w);
          std::memset(blocked + local_row_offset + static_cast<size_t>(min_column - box_x0), 1,
                      static_cast<size_t>(max_column - min_column + 1));
        }
      }
    }
  }

  const auto local_start_column = start_column - box_x0;
  const auto local_start_row = start_row - box_y0;
  const auto local_end_column = end_column - box_x0;
  const auto local_end_row = end_row - box_y0;

  const auto start = local_start_row * box_w + local_start_column;
  const auto goal = local_end_row * box_w + local_end_column;

  if (blocked[static_cast<size_t>(start)] != 0 || blocked[static_cast<size_t>(goal)] != 0) [[unlikely]] {
    lua_newtable(state);
    return 1;
  }

  if (_pathfinder.g.size() < local_total) {
    _pathfinder.g.resize(local_total);
    _pathfinder.generation.resize(local_total, 0);
    _pathfinder.parent.resize(local_total);
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

  const auto start_index = static_cast<size_t>(start);
  costs[start_index] = .0f;
  generations[start_index] = generation;
  parents[start_index] = -1;

  constexpr int32_t direction_column[] = { 1, -1, 0, 0, 1, 1, -1, -1 };
  constexpr int32_t direction_row[] = { 0, 0, 1, -1, 1, -1, 1, -1 };
  constexpr float direction_cost[] = { 1.f, 1.f, 1.f, 1.f, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f };

  constexpr auto SQRT2_MINUS_1 = .41421356f;

  const auto octile = [local_end_column, local_end_row](int32_t column, int32_t row) noexcept -> float {
    const auto delta_column = static_cast<float>(std::abs(column - local_end_column));
    const auto delta_row = static_cast<float>(std::abs(row - local_end_row));
    return delta_column > delta_row
      ? delta_column + SQRT2_MINUS_1 * delta_row
      : delta_row + SQRT2_MINUS_1 * delta_column;
  };

  _pathfinder.heap.push_back({octile(local_start_column, local_start_row), start});

  const auto compare = [](const node& a, const node& b) noexcept { return a.f > b.f; };

  auto iterations = local_total;
  while (!_pathfinder.heap.empty() && iterations > 0) {
    --iterations;
    std::pop_heap(_pathfinder.heap.begin(), _pathfinder.heap.end(), compare);
    const auto [priority, current] = _pathfinder.heap.back();
    _pathfinder.heap.pop_back();

    if (current == goal) break;

    const auto current_index = static_cast<size_t>(current);
    if (generations[current_index] == generation &&
        priority > costs[current_index] + octile(current % box_w, current / box_w))
      continue;

    const auto current_row = current / box_w;
    const auto current_column = current % box_w;
    const auto current_cost = costs[current_index];

    for (int direction = 0; direction < 8; ++direction) {
      const auto neighbor_column = current_column + direction_column[direction];
      const auto neighbor_row = current_row + direction_row[direction];

      if (static_cast<uint32_t>(neighbor_column) >= static_cast<uint32_t>(box_w) ||
          static_cast<uint32_t>(neighbor_row) >= static_cast<uint32_t>(box_h)) [[unlikely]]
        continue;

      const auto neighbor_index = static_cast<size_t>(neighbor_row * box_w + neighbor_column);

      if (blocked[neighbor_index] != 0)
        continue;

      if (direction >= 4) {
        if (blocked[static_cast<size_t>(current_row * box_w + neighbor_column)] != 0 ||
            blocked[static_cast<size_t>(neighbor_row * box_w + current_column)] != 0)
          continue;
      }

      const auto neighbor_cost = current_cost + direction_cost[direction];

      if (generations[neighbor_index] == generation && neighbor_cost >= costs[neighbor_index])
        continue;

      costs[neighbor_index] = neighbor_cost;
      generations[neighbor_index] = generation;
      parents[neighbor_index] = current;

      _pathfinder.heap.push_back({neighbor_cost + octile(neighbor_column, neighbor_row), static_cast<int32_t>(neighbor_index)});
      std::push_heap(_pathfinder.heap.begin(), _pathfinder.heap.end(), compare);
    }
  }

  if (generations[static_cast<size_t>(goal)] == generation && parents[static_cast<size_t>(goal)] != -1) {
    for (auto current = goal; current != -1; current = parents[static_cast<size_t>(current)])
      _pathfinder.path.emplace_back(current);
    std::reverse(_pathfinder.path.begin(), _pathfinder.path.end());
  }

  lua_newtable(state);
  const auto half = _size * .5f;
  int index = 1;
  for (const auto cell : _pathfinder.path) {
    const auto local_column = cell % box_w;
    const auto local_row = cell / box_w;
    const auto global_column = local_column + box_x0;
    const auto global_row = local_row + box_y0;
    lua_newtable(state);
    lua_pushnumber(state, static_cast<double>(static_cast<float>(global_column) * _size + half));
    lua_rawseti(state, -2, 1);
    lua_pushnumber(state, static_cast<double>(static_cast<float>(global_row) * _size + half));
    lua_rawseti(state, -2, 2);
    lua_rawseti(state, -2, index++);
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
