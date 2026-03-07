#include "tilemap.hpp"

tilemap::tilemap(std::string_view name, pixmappool& pixmaps, b2WorldId world) {
  const auto filename = std::format("tilemaps/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto *data = reinterpret_cast<const char *>(buffer.data());
  const auto size = buffer.size();
  const auto label = std::format("@{}", filename);

  if (luaL_loadbuffer(L, data, size, label.c_str()) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  if (lua_pcall(L, 0, 1, 0) != 0) [[unlikely]] {
    std::string error = lua_tostring(L, -1);
    lua_pop(L, 1);
    throw std::runtime_error(error);
  }

  lua_getfield(L, -1, "tileset");
  const std::string_view tileset_name = lua_tostring(L, -1);
  _tileset = &pixmaps.get(std::format("tilemaps/{}", tileset_name));
  lua_pop(L, 1);

  lua_getfield(L, -1, "tile");
  _tile = static_cast<int>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "columns");
  _columns = static_cast<int>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  lua_getfield(L, -1, "rows");
  _rows = static_cast<int>(lua_tonumber(L, -1));
  lua_pop(L, 1);

  _tileset_columns = _tileset->width() / _tile;
  const auto tileset_rows = _tileset->height() / _tile;
  const auto total_tiles = static_cast<size_t>(_tileset_columns) * static_cast<size_t>(tileset_rows);

  {
    const auto atlas_width = static_cast<float>(_tileset->width());
    const auto atlas_height = static_cast<float>(_tileset->height());
    const auto u_scale = static_cast<float>(_tile) / atlas_width;
    const auto v_scale = static_cast<float>(_tile) / atlas_height;

    _uv_table.resize(total_tiles);

    for (size_t id = 0; id < total_tiles; ++id) {
      const auto tile_column = static_cast<int>(id % static_cast<size_t>(_tileset_columns));
      const auto tile_row = static_cast<int>(id / static_cast<size_t>(_tileset_columns));

      auto& uv = _uv_table[id];
      uv[0] = static_cast<float>(tile_column) * u_scale;
      uv[1] = static_cast<float>(tile_row) * v_scale;
      uv[2] = uv[0] + u_scale;
      uv[3] = uv[1] + v_scale;
    }
  }

  const auto count = static_cast<size_t>(_columns) * static_cast<size_t>(_rows);

  lua_getfield(L, -1, "background");
  if (lua_istable(L, -1)) {
    std::vector<uint16_t> tiles(count);
    for (size_t i = 0; i < count; ++i) {
      lua_rawgeti(L, -1, static_cast<int>(i + 1));
      tiles[i] = static_cast<uint16_t>(lua_tonumber(L, -1));
      lua_pop(L, 1);
    }
    build_layer(tiles, _background_vertices, _background_indices);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "foreground");
  if (lua_istable(L, -1)) {
    std::vector<uint16_t> tiles(count);
    for (size_t i = 0; i < count; ++i) {
      lua_rawgeti(L, -1, static_cast<int>(i + 1));
      tiles[i] = static_cast<uint16_t>(lua_tonumber(L, -1));
      lua_pop(L, 1);
    }
    build_layer(tiles, _foreground_vertices, _foreground_indices);
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "collision");
  if (lua_istable(L, -1)) {
    std::vector<uint8_t> collision(count);
    for (size_t i = 0; i < count; ++i) {
      lua_rawgeti(L, -1, static_cast<int>(i + 1));
      collision[i] = static_cast<uint8_t>(lua_tonumber(L, -1));
      lua_pop(L, 1);
    }
    build_collision(collision, world);
  }
  lua_pop(L, 1);

  lua_pop(L, 1);
}

tilemap::~tilemap() {
  if (b2Body_IsValid(_collision_body))
    b2DestroyBody(_collision_body);
}

void tilemap::build_layer(
  const std::vector<uint16_t>& tiles,
  std::vector<SDL_Vertex>& vertices,
  std::vector<int>& indices
) const {
  const auto tile_size = static_cast<float>(_tile);
  constexpr SDL_FColor white{1.f, 1.f, 1.f, 1.f};

  const auto columns = static_cast<size_t>(_columns);
  const auto rows = static_cast<size_t>(_rows);

  size_t solid_count = 0;
  for (size_t i = 0; i < tiles.size(); ++i) {
    if (tiles[i] != 0)
      ++solid_count;
  }

  vertices.reserve(solid_count * 4);
  indices.reserve(solid_count * 6);

  for (size_t row = 0; row < rows; ++row) {
    const auto row_offset = row * columns;
    const auto dy = static_cast<float>(row) * tile_size;

    for (size_t column = 0; column < columns; ++column) {
      const auto tile_id = tiles[row_offset + column];
      if (tile_id == 0) [[likely]]
        continue;

      const auto& uv = _uv_table[tile_id - 1];
      const auto dx = static_cast<float>(column) * tile_size;

      const auto base = static_cast<int>(vertices.size());

      vertices.push_back({{dx, dy}, white, {uv[0], uv[1]}});
      vertices.push_back({{dx + tile_size, dy}, white, {uv[2], uv[1]}});
      vertices.push_back({{dx + tile_size, dy + tile_size}, white, {uv[2], uv[3]}});
      vertices.push_back({{dx, dy + tile_size}, white, {uv[0], uv[3]}});

      indices.push_back(base);
      indices.push_back(base + 1);
      indices.push_back(base + 2);
      indices.push_back(base);
      indices.push_back(base + 2);
      indices.push_back(base + 3);
    }
  }
}

void tilemap::build_collision(const std::vector<uint8_t>& collision, b2WorldId world) {
  const auto half = static_cast<float>(_tile) * .5f;
  const auto tile_size = static_cast<float>(_tile);
  const auto columns = static_cast<size_t>(_columns);
  const auto rows = static_cast<size_t>(_rows);
  const auto total = columns * rows;

  std::vector<uint8_t> visited(total);

  b2BodyDef body_definition = b2DefaultBodyDef();
  body_definition.type = b2_staticBody;
  _collision_body = b2CreateBody(world, &body_definition);

  for (size_t row = 0; row < rows; ++row) {
    const auto row_offset = row * columns;

    for (size_t column = 0; column < columns; ++column) {
      const auto index = row_offset + column;
      if (collision[index] == 0 || visited[index]) [[likely]]
        continue;

      auto run_width = size_t{1};
      while (column + run_width < columns &&
             collision[index + run_width] != 0 &&
             !visited[index + run_width]) {
        ++run_width;
      }

      auto run_height = size_t{1};
      while (row + run_height < rows) {
        const auto check_offset = (row + run_height) * columns + column;

        auto valid = true;
        for (size_t dx = 0; dx < run_width; ++dx) {
          if (collision[check_offset + dx] == 0 || visited[check_offset + dx]) {
            valid = false;
            break;
          }
        }

        if (!valid) break;
        ++run_height;
      }

      for (size_t dy = 0; dy < run_height; ++dy) {
        std::memset(visited.data() + (row + dy) * columns + column, 1, run_width);
      }

      const auto box_hx = static_cast<float>(run_width) * half;
      const auto box_hy = static_cast<float>(run_height) * half;

      const auto center_x = static_cast<float>(column) * tile_size + box_hx;
      const auto center_y = static_cast<float>(row) * tile_size + box_hy;

      const auto polygon = b2MakeOffsetBox(box_hx, box_hy, {center_x, center_y}, b2MakeRot(.0f));

      auto shape_definition = b2DefaultShapeDef();
      shape_definition.enableContactEvents = true;

      _collision_shapes.push_back(
        b2CreatePolygonShape(_collision_body, &shape_definition, &polygon));
    }
  }
}

void tilemap::draw_background(const camera& camera) const noexcept {
  draw_layer(_tileset, _background_vertices, _background_indices, camera);
}

void tilemap::draw_foreground(const camera& camera) const noexcept {
  draw_layer(_tileset, _foreground_vertices, _foreground_indices, camera);
}

void tilemap::draw_layer(
  const pixmap* tileset,
  const std::vector<SDL_Vertex>& vertices,
  const std::vector<int>& indices,
  const camera& camera
) noexcept {
  if (vertices.empty()) [[unlikely]]
    return;

  const auto sx = viewport.width / camera.w;
  const auto sy = viewport.height / camera.h;

  static std::vector<SDL_Vertex> buffer;
  buffer.resize(vertices.size());

  for (auto i = 0uz; i < vertices.size(); ++i) {
    buffer[i] = vertices[i];
    buffer[i].position.x = (vertices[i].position.x - camera.x) * sx;
    buffer[i].position.y = (vertices[i].position.y - camera.y) * sy;
  }

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*tileset),
    buffer.data(),
    static_cast<int>(buffer.size()),
    indices.data(),
    static_cast<int>(indices.size())
  );
}
