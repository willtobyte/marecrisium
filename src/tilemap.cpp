static bool vacant(const std::vector<uint32_t>& tiles) noexcept {
  for (const auto t : tiles)
    if (t != 0) [[likely]] return false;

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
  if (layer.tiles.empty()) [[unlikely]]
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

  static constexpr auto HEADER = size_t{20};
  static constexpr auto PERCELL =
    sizeof(uint32_t) +
    sizeof(uint32_t) +
    sizeof(uint8_t);

  uint32_t width{}, height{};
  float size{};
  std::memcpy(&width, bytes + 0, sizeof(width));
  std::memcpy(&height, bytes + 4, sizeof(height));
  std::memcpy(&size, bytes + 8, sizeof(size));

  _width = static_cast<int32_t>(width);
  _height = static_cast<int32_t>(height);
  _size = size;

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

  if (vacant(_background.tiles)) [[unlikely]]
    _background.tiles.clear();
  if (vacant(_foreground.tiles)) [[unlikely]]
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

          if (!valid) [[unlikely]]
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

  prepare(_background, name, "background", _size, _inverse);
  prepare(_foreground, name, "foreground", _size, _inverse);

  const auto tx = static_cast<size_t>(viewport.width * _inverse) + 2;
  const auto ty = static_cast<size_t>(viewport.height * _inverse) + 2;
  const auto capacity = tx * ty;

  if (_background.atlas) [[likely]]
    buffers(_background, capacity);
  if (_foreground.atlas) [[likely]]
    buffers(_foreground, capacity);
}

void tilemap::draw_background() {
  if (!_background.atlas) [[unlikely]]
    return;

  if (_snapshot != viewport) [[unlikely]]
    _dirty = true;

  if (_dirty) [[unlikely]]
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

  if (!_foreground.atlas) [[unlikely]] {
    _snapshot = viewport;
    _dirty = false;
  }
}

void tilemap::draw_foreground() {
  if (!_foreground.atlas) [[unlikely]]
    return;

  if (_dirty) [[unlikely]]
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
