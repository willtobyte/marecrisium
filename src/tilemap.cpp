#include "tilemap.hpp"

tilemap::tilemap(std::string_view name, b2WorldId world) {
  const auto filename = std::format("tilemaps/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto* data = reinterpret_cast<const char*>(buffer.data());
  const auto length = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, length, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(std::move(error));
  }

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(std::move(error));
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

  _inv_size = 1.f / _size;

  const auto total = static_cast<size_t>(_width) * static_cast<size_t>(_height);

  _background_tiles.resize(total);
  _foreground_tiles.resize(total);

  lua_getfield(L, -1, "background");
  if (lua_istable(L, -1)) {
    for (size_t i = 0; i < total; ++i) {
      lua_rawgeti(L, -1, static_cast<int>(i + 1));
      _background_tiles[i] = static_cast<uint32_t>(lua_tonumber(L, -1));
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "foreground");
  if (lua_istable(L, -1)) {
    for (size_t i = 0; i < total; ++i) {
      lua_rawgeti(L, -1, static_cast<int>(i + 1));
      _foreground_tiles[i] = static_cast<uint32_t>(lua_tonumber(L, -1));
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  {
    std::vector<uint8_t> collision(total);

    lua_getfield(L, -1, "collision");
    if (lua_istable(L, -1)) {
      for (size_t i = 0; i < total; ++i) {
        lua_rawgeti(L, -1, static_cast<int>(i + 1));
        collision[i] = static_cast<uint8_t>(lua_tonumber(L, -1));
        lua_pop(L, 1);
      }
    }
    lua_pop(L, 1);

    const auto half = _size * .5f;
    const auto w = static_cast<size_t>(_width);
    const auto h = static_cast<size_t>(_height);

    std::vector<uint8_t> visited(total);
    const auto* RESTRICT tiles = collision.data();
    auto* RESTRICT visited_data = visited.data();

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

  _backdrop = &resources.pixmap.get(std::format("tilemaps/{}/backdrop", name));
  _background_atlas = &resources.pixmap.get(std::format("tilemaps/{}/background", name));
  _foreground_atlas = &resources.pixmap.get(std::format("tilemaps/{}/foreground", name));

  {
    const auto atlas_width = static_cast<float>(_background_atlas->width());
    const auto atlas_height = static_cast<float>(_background_atlas->height());
    const auto u_scale = _size / atlas_width;
    const auto v_scale = _size / atlas_height;
    const auto tiles_per_row = static_cast<size_t>(atlas_width * _inv_size);
    const auto tiles_per_column = static_cast<size_t>(atlas_height * _inv_size);
    const auto count = tiles_per_row * tiles_per_column;

    _background_uvs.resize(count);

    const auto half_texel_u = .5f / atlas_width;
    const auto half_texel_v = .5f / atlas_height;

    for (size_t id = 0; id < count; ++id) {
      const auto column = static_cast<float>(id % tiles_per_row);
      const auto row = static_cast<float>(id / tiles_per_row);
      auto& entry = _background_uvs[id];
      entry.u0 = column * u_scale + half_texel_u;
      entry.v0 = row * v_scale + half_texel_v;
      entry.u1 = (column + 1.f) * u_scale - half_texel_u;
      entry.v1 = (row + 1.f) * v_scale - half_texel_v;
    }
  }

  {
    const auto atlas_width = static_cast<float>(_foreground_atlas->width());
    const auto atlas_height = static_cast<float>(_foreground_atlas->height());
    const auto u_scale = _size / atlas_width;
    const auto v_scale = _size / atlas_height;
    const auto tiles_per_row = static_cast<size_t>(atlas_width * _inv_size);
    const auto tiles_per_column = static_cast<size_t>(atlas_height * _inv_size);
    const auto count = tiles_per_row * tiles_per_column;

    _foreground_uvs.resize(count);

    const auto half_texel_u = .5f / atlas_width;
    const auto half_texel_v = .5f / atlas_height;

    for (size_t id = 0; id < count; ++id) {
      const auto column = static_cast<float>(id % tiles_per_row);
      const auto row = static_cast<float>(id / tiles_per_row);
      auto& entry = _foreground_uvs[id];
      entry.u0 = column * u_scale + half_texel_u;
      entry.v0 = row * v_scale + half_texel_v;
      entry.u1 = (column + 1.f) * u_scale - half_texel_u;
      entry.v1 = (row + 1.f) * v_scale - half_texel_v;
    }
  }

  {
    const auto tiles_x = static_cast<size_t>(viewport.width * _inv_size) + 2;
    const auto tiles_y = static_cast<size_t>(viewport.height * _inv_size) + 2;
    const auto max_tiles = tiles_x * tiles_y;

    _background_vertices.reserve(max_tiles * 4);
    _background_indices.reserve(max_tiles * 6);
    _foreground_vertices.reserve(max_tiles * 4);
    _foreground_indices.reserve(max_tiles * 6);
  }
}

void tilemap::set_camera(float x, float y, float w, float h) noexcept {
  if (_camera_x == x && _camera_y == y && _camera_w == w && _camera_h == h) [[likely]]
    return;

  _camera_x = x;
  _camera_y = y;
  _camera_w = w;
  _camera_h = h;
  _dirty = true;
}

void tilemap::draw_background() noexcept {
  _backdrop->draw(
    .0f, .0f,
    static_cast<float>(_backdrop->width()),
    static_cast<float>(_backdrop->height()),
    .0f, .0f, viewport.width, viewport.height
  );

  if (_dirty) {
    build_layer(
      _background_tiles.data(),
      _background_uvs.data(),
      _background_vertices,
      _background_indices
    );
  }

  if (_background_vertices.empty()) [[unlikely]]
    return;

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_background_atlas),
    _background_vertices.data(),
    static_cast<int>(_background_vertices.size()),
    _background_indices.data(),
    static_cast<int>(_background_indices.size())
  );
}

void tilemap::draw_foreground() noexcept {
  if (_dirty) {
    build_layer(
      _foreground_tiles.data(),
      _foreground_uvs.data(),
      _foreground_vertices,
      _foreground_indices
    );
    _dirty = false;
  }

  if (_foreground_vertices.empty()) [[unlikely]]
    return;

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*_foreground_atlas),
    _foreground_vertices.data(),
    static_cast<int>(_foreground_vertices.size()),
    _foreground_indices.data(),
    static_cast<int>(_foreground_indices.size())
  );
}

void tilemap::build_layer(const uint32_t* tiles, const uv* uvs, std::vector<SDL_Vertex>& vertices, std::vector<int32_t>& indices) noexcept {
  vertices.clear();
  indices.clear();

  const auto start_column = std::max(0, static_cast<int32_t>(_camera_x * _inv_size));
  const auto start_row = std::max(0, static_cast<int32_t>(_camera_y * _inv_size));
  const auto end_column = std::min(_width - 1, static_cast<int32_t>((_camera_x + _camera_w) * _inv_size) + 1);
  const auto end_row = std::min(_height - 1, static_cast<int32_t>((_camera_y + _camera_h) * _inv_size) + 1);

  if (start_column > end_column || start_row > end_row) [[unlikely]]
    return;

  constexpr SDL_FColor white{1.f, 1.f, 1.f, 1.f};

  auto row_offset = start_row * _width;
  auto dy = static_cast<float>(start_row) * _size - _camera_y;

  for (auto row = start_row; row <= end_row; ++row, row_offset += _width, dy += _size) {
    const auto snapped_y0 = std::roundf(dy);
    const auto snapped_y1 = std::roundf(dy + _size);

    for (auto column = start_column; column <= end_column; ++column) {
      const auto tile_id = tiles[row_offset + column];
      if (tile_id == 0) [[likely]]
        continue;

      const auto& entry = uvs[tile_id - 1];
      const auto dx = static_cast<float>(column) * _size - _camera_x;
      const auto snapped_x0 = std::roundf(dx);
      const auto snapped_x1 = std::roundf(dx + _size);
      const auto base = static_cast<int32_t>(vertices.size());

      vertices.emplace_back(SDL_Vertex{{snapped_x0, snapped_y0}, white, {entry.u0, entry.v0}});
      vertices.emplace_back(SDL_Vertex{{snapped_x1, snapped_y0}, white, {entry.u1, entry.v0}});
      vertices.emplace_back(SDL_Vertex{{snapped_x1, snapped_y1}, white, {entry.u1, entry.v1}});
      vertices.emplace_back(SDL_Vertex{{snapped_x0, snapped_y1}, white, {entry.u0, entry.v1}});

      indices.emplace_back(base);
      indices.emplace_back(base + 1);
      indices.emplace_back(base + 2);
      indices.emplace_back(base);
      indices.emplace_back(base + 2);
      indices.emplace_back(base + 3);
    }
  }
}
