namespace {
constexpr auto axes = 2uz;
constexpr auto header = axes * sizeof(uint32_t) + sizeof(float) + sizeof(uint64_t);
constexpr auto cellsize = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
constexpr auto corners = 4uz;
constexpr auto cellspan = 1uz;
constexpr auto padding = int32_t{1};
constexpr auto overscan = 2uz;
constexpr auto empty = uint8_t{};
constexpr auto pending = uint8_t{2};
constexpr auto solid = uint8_t{1};
constexpr auto firsttile = uint32_t{1};
constexpr auto halfscale = .5f;
constexpr auto fullscale = 1.f;
[[nodiscard]] constexpr uint32_t little(const uint8_t* bytes) noexcept {
  uint32_t value;
  std::memcpy(&value, bytes, sizeof(value));
  return value;
}

[[nodiscard]] bool vacant(std::span<const uint8_t> bytes) noexcept {
  const auto* p = bytes.data();
  const auto* const end = p + bytes.size();
  while (p + sizeof(uint64_t) <= end) {
    uint64_t word;
    std::memcpy(&word, p, sizeof(word));
    if (word) [[likely]]
      return false;
    p += sizeof(word);
  }
  while (p < end)
    if (*p++) [[likely]]
      return false;
  return true;
}

void buffers(tilemap::layer& layer, size_t capacity) {
  constexpr auto indices = corners + corners / 2uz;
  const auto first = layer.indices.size() / indices;
  if (first >= capacity) [[likely]]
    return;

  layer.vertices.reserve(capacity * corners);
  layer.indices.resize(capacity * indices);

  auto* output = layer.indices.data() + first * indices;

  assert(output != nullptr && "tilemap index buffer must exist");
  [[assume(output != nullptr)]];

  for (auto i = first; i < capacity; ++i) {
    const auto base = static_cast<int32_t>(i * corners);
    output[0] = base;
    output[1] = base + 1;
    output[2] = base + 2;
    output[3] = base;
    output[4] = base + 2;
    output[5] = base + 3;
    output += indices;
  }
}

void prepare(tilemap::layer& layer, std::string_view name, std::string_view path, float size, float inverse) {
  if (layer.tiles.empty()) [[unlikely]]
    return;

  layer.atlas = depot->pixmap.get(std::format("tilemaps/{}/{}", name, path));
  SDL_SetTextureBlendMode(static_cast<SDL_Texture*>(*layer.atlas), SDL_BLENDMODE_NONE);

  const auto aw = static_cast<float>(layer.atlas->width());
  const auto ah = static_cast<float>(layer.atlas->height());
  const auto us = size / aw;
  const auto vs = size / ah;
  const auto tpr = static_cast<size_t>(aw * inverse);
  const auto tpc = static_cast<size_t>(ah * inverse);
  const auto count = tpr * tpc;
  const auto htu = halfscale / aw;
  const auto htv = halfscale / ah;

  assert(tpr > 0 && "tilemap atlas must have columns");
  [[assume(tpr > 0)]];
  assert(tpc > 0 && "tilemap atlas must have rows");
  [[assume(tpc > 0)]];

  layer.uvs.resize(count);
  for (size_t id = 0; id < count; ++id) {
    const auto cf = static_cast<float>(id % tpr);
    const auto rf = static_cast<float>(id / tpr);
    layer.uvs[id] = {
      cf * us + htu,
      rf * vs + htv,
      (cf + fullscale) * us - htu,
      (rf + fullscale) * vs - htv,
    };
  }
}

void render(const tilemap::layer& layer) {
  const auto vertices = layer.vertices.size();
  if (vertices == 0) [[unlikely]]
    return;

  constexpr auto indices = corners + corners / 2uz;
  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*layer.atlas),
    layer.vertices.data(),
    static_cast<int>(vertices),
    layer.indices.data(),
    static_cast<int>(vertices / corners * indices)
  );
}
}

tilemap::tilemap(std::string_view name, b2WorldId world) {
  const auto buffer = io::read(std::format("tilemaps/{}.tilemap", name));
  const auto* noalias bytes = buffer.data();
  const auto length = buffer.size();

  auto* cursor = bytes;
  _width = static_cast<int32_t>(little(cursor));
  cursor += sizeof(uint32_t);
  _height = static_cast<int32_t>(little(cursor));
  cursor += sizeof(uint32_t);
  _size = std::bit_cast<float>(little(cursor));

  assert(_size > float{} && "tilemap: invalid tile size");
  _inverse = fullscale / _size;

  const auto count = static_cast<size_t>(_width) * static_cast<size_t>(_height);

  assert(length == header + cellsize * count && "tilemap data size must be valid");
  [[assume(length == header + cellsize * count)]];

  auto offset = header;
  const auto tilebytes = count * sizeof(uint32_t);
  const auto load = [&](layer& current) {
    const std::span source{bytes + offset, tilebytes};
    offset += tilebytes;

    if (vacant(source)) [[unlikely]]
      return;

    current.tiles.resize(count);
    if constexpr (std::endian::native == std::endian::little) {
      std::memcpy(current.tiles.data(), source.data(), tilebytes);
    } else {
      for (size_t i{}; i < count; ++i)
        current.tiles[i] = little(source.data() + i * sizeof(uint32_t));
    }
  };

  load(_background);
  load(_foreground);

  _collision.resize(count);
  auto* noalias collision = _collision.data();
  const auto* noalias source = bytes + offset;
  for (size_t i = 0; i < count; ++i)
    collision[i] = source[i] == empty ? empty : pending;

  {
    assert(_width > int32_t{} && _height > int32_t{} && "tilemap dimensions must be positive");
    [[assume(_width > int32_t{} && _height > int32_t{})]];

    const auto columns = static_cast<size_t>(_width);
    const auto rows = static_cast<size_t>(_height);

    for (size_t row = 0; row < rows; ++row) {
      const auto ro = row * columns;

      for (size_t column = 0; column < columns; ++column) {
        const auto index = ro + column;
        if (collision[index] != pending) [[unlikely]]
          continue;

        auto rw = cellspan;
        while (column + rw < columns && collision[index + rw] == pending)
          ++rw;

        auto rh = cellspan;
        while (row + rh < rows) {
          const auto co = (row + rh) * columns + column;
          auto valid = true;

          for (size_t dx = 0; dx < rw; ++dx) {
            if (collision[co + dx] != pending) [[unlikely]] {
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
            collision[base + dx] = solid;
        }

        const auto half = _size * halfscale;
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

  const auto tx = static_cast<size_t>(viewport.width * _inverse) + overscan;
  const auto ty = static_cast<size_t>(viewport.height * _inverse) + overscan;
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

  render(_background);

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

  render(_foreground);
}

void tilemap::tessellate(layer& current) {
  const auto sc = std::max(int32_t{}, static_cast<int32_t>(viewport.x * _inverse));
  const auto sr = std::max(int32_t{}, static_cast<int32_t>(viewport.y * _inverse));
  const auto ec = std::min(_width - padding, static_cast<int32_t>((viewport.x + viewport.width) * _inverse) + padding);
  const auto er = std::min(_height - padding, static_cast<int32_t>((viewport.y + viewport.height) * _inverse) + padding);

  if (sc > ec || sr > er) [[unlikely]] {
    current.vertices.clear();
    return;
  }

  const auto capacity = static_cast<size_t>((ec - sc + padding) * (er - sr + padding));
  buffers(current, capacity);
  current.vertices.resize(capacity * corners);

  auto* vp = current.vertices.data();

  constexpr SDL_FColor white{fullscale, fullscale, fullscale, fullscale};

  auto ro = sr * _width;
  auto dy = static_cast<float>(sr) * _size - viewport.y;

  for (auto row = sr; row <= er; ++row, ro += _width, dy += _size) {
    const auto y1 = dy + _size;
    auto x0 = static_cast<float>(sc) * _size - viewport.x;

    for (auto column = sc; column <= ec; ++column, x0 += _size) {
      const auto ti = current.tiles[static_cast<size_t>(ro + column)];
      if (ti == uint32_t{}) [[unlikely]]
        continue;

      assert(static_cast<size_t>(ti - firsttile) < current.uvs.size() && "tile index out of bounds");
      const auto& uv = current.uvs[ti - firsttile];
      const auto x1 = x0 + _size;

      *vp++ = SDL_Vertex{{x0, dy}, white, {uv.u0, uv.v0}};
      *vp++ = SDL_Vertex{{x1, dy}, white, {uv.u1, uv.v0}};
      *vp++ = SDL_Vertex{{x1, y1}, white, {uv.u1, uv.v1}};
      *vp++ = SDL_Vertex{{x0, y1}, white, {uv.u0, uv.v1}};
    }
  }

  current.vertices.resize(static_cast<size_t>(vp - current.vertices.data()));
}
