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
constexpr std::array pattern{int32_t{0}, int32_t{1}, int32_t{2}, int32_t{0}, int32_t{2}, int32_t{3}};

[[nodiscard]] constexpr uint32_t little(const uint8_t* bytes) noexcept {
  auto value = uint32_t{};
  constexpr auto bits = std::numeric_limits<uint8_t>::digits;
  for (size_t i{}; i < sizeof(value); ++i)
    value |= static_cast<uint32_t>(bytes[i]) << (i * bits);

  return value;
}

[[nodiscard]] bool vacant(std::span<const uint8_t> bytes) noexcept {
  while (bytes.size() >= sizeof(uint64_t)) {
    uint64_t word;
    std::memcpy(&word, bytes.data(), sizeof(word));
    if (word != uint64_t{}) [[likely]]
      return false;

    bytes = bytes.subspan(sizeof(word));
  }

  for (const auto byte : bytes)
    if (byte != empty) [[likely]]
      return false;

  return true;
}

void buffers(tilemap::layer& layer, size_t capacity) {
  const auto first = layer.indices.size() / pattern.size();
  if (first >= capacity) [[likely]]
    return;

  layer.vertices.reserve(capacity * corners);
  layer.indices.resize(capacity * pattern.size());

  auto* output = layer.indices.data() + first * pattern.size();

  [[assume(output != nullptr)]];

  for (auto i = first; i < capacity; ++i) {
    const auto base = static_cast<int32_t>(i * corners);
    for (const auto offset : pattern)
      *output++ = base + offset;
  }
}

void prepare(tilemap::layer& layer, std::string_view name, std::string_view path, float size, float inverse) {
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
  const auto htu = halfscale / aw;
  const auto htv = halfscale / ah;

  [[assume(tpr > 0)]];
  [[assume(tpc > 0)]];

  layer.uvs.resize(count);
  for (size_t id = 0; id < count; ++id) {
    const auto column = static_cast<float>(id % tpr);
    const auto rowid = id / tpr;
    const auto row = static_cast<float>(rowid);
    layer.uvs[id] = {
      column * us + htu,
      row * vs + htv,
      (column + fullscale) * us - htu,
      (row + fullscale) * vs - htv,
    };
  }
}

void render(const tilemap::layer& layer) {
  const auto vertices = layer.vertices.size();
  if (vertices == 0) [[unlikely]]
    return;

  SDL_RenderGeometry(
    renderer,
    static_cast<SDL_Texture*>(*layer.atlas),
    layer.vertices.data(),
    static_cast<int>(vertices),
    layer.indices.data(),
    static_cast<int>(vertices / corners * pattern.size())
  );
}
}

tilemap::tilemap(std::string_view name, b2WorldId world) {
  const auto blob = io::read(std::format("tilemaps/{}.bmap", name));
  const auto* noalias bytes = blob.data();
  const auto length = blob.size();

  auto* cursor = bytes;
  _width = static_cast<int32_t>(little(cursor));
  cursor += sizeof(uint32_t);
  _height = static_cast<int32_t>(little(cursor));
  cursor += sizeof(uint32_t);
  _size = std::bit_cast<float>(little(cursor));

  assert(_size > float{} && "tilemap: invalid tile size");
  _inverse = fullscale / _size;

  const auto n = static_cast<size_t>(_width) * static_cast<size_t>(_height);

  [[assume(length == header + cellsize * n)]];

  auto offset = header;
  const auto tilebytes = n * sizeof(uint32_t);
  const auto load = [&](layer& layer) {
    const std::span source{bytes + offset, tilebytes};
    offset += tilebytes;

    if (vacant(source)) [[unlikely]]
      return;

    layer.tiles.resize(n);
    if constexpr (std::endian::native == std::endian::little) {
      std::memcpy(layer.tiles.data(), source.data(), tilebytes);
    } else {
      for (size_t i{}; i < n; ++i)
        layer.tiles[i] = little(source.data() + i * sizeof(uint32_t));
    }
  };

  load(_background);
  load(_foreground);

  _collision.resize(n);
  auto* noalias collision = _collision.data();
  const auto* noalias source = bytes + offset;
  for (size_t i = 0; i < n; ++i)
    collision[i] = source[i] == empty ? empty : pending;

  {
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

void tilemap::tessellate(layer& layer) {
  const auto sc = std::max(int32_t{}, static_cast<int32_t>(viewport.x * _inverse));
  const auto sr = std::max(int32_t{}, static_cast<int32_t>(viewport.y * _inverse));
  const auto ec = std::min(_width - padding, static_cast<int32_t>((viewport.x + viewport.width) * _inverse) + padding);
  const auto er = std::min(_height - padding, static_cast<int32_t>((viewport.y + viewport.height) * _inverse) + padding);

  if (sc > ec || sr > er) [[unlikely]] {
    layer.vertices.clear();
    return;
  }

  const auto capacity = static_cast<size_t>((ec - sc + padding) * (er - sr + padding));
  buffers(layer, capacity);
  layer.vertices.resize(capacity * corners);

  auto* vp = layer.vertices.data();

  constexpr SDL_FColor white{fullscale, fullscale, fullscale, fullscale};

  auto ro = sr * _width;
  auto dy = static_cast<float>(sr) * _size - viewport.y;

  for (auto row = sr; row <= er; ++row, ro += _width, dy += _size) {
    const auto y1 = dy + _size;

    for (auto column = sc; column <= ec; ++column) {
      const auto ti = layer.tiles[static_cast<size_t>(ro + column)];
      if (ti == uint32_t{}) [[unlikely]]
        continue;

      assert(static_cast<size_t>(ti - firsttile) < layer.uvs.size() && "tile index out of bounds");
      const auto& uv = layer.uvs[ti - firsttile];
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
