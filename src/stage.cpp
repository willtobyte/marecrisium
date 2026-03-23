#include "stage.hpp"



static void* to_userdata(entt::entity e) noexcept {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(e) + 1);
}

static entt::entity to_entity(const void* p) noexcept {
  return static_cast<entt::entity>(reinterpret_cast<uintptr_t>(p) - 1);
}

static b2Vec2 body_center(const transform& tf, const frame& fr, const body& bd) noexcept {
  return {tf.x + fr.cx + bd.cached_hx,
          tf.y + fr.cy + bd.cached_hy};
}

static void on_objectproxy_destroy(entt::registry& registry, entt::entity entity) {
  auto& proxy = registry.get<objectproxy>(entity);

  if (proxy.handle == LUA_NOREF)
    return;

  lua_rawgeti(L, LUA_REGISTRYINDEX, proxy.handle);
  auto* userdata = static_cast<objectproxy*>(lua_touserdata(L, -1));
  if (userdata) {
    release(L, userdata->on_animation_begin);
    release(L, userdata->on_animation_end);
    release(L, userdata->on_loop);
    release(L, userdata->prototype);
  }

  lua_pop(L, 1);
  release(L, proxy.handle);
}

static std::optional<std::pair<entt::entity, entt::entity>> resolve(b2ShapeId a, b2ShapeId b) noexcept {
  if (!b2Shape_IsValid(a) || !b2Shape_IsValid(b))
    return std::nullopt;

  const auto *da = b2Shape_GetUserData(a);
  const auto *db = b2Shape_GetUserData(b);
  if (!da || !db)
    return std::nullopt;

  return std::pair{to_entity(da), to_entity(db)};
}

static void dispatch_sensor_event(stage& self, b2ShapeId sensor_shape, b2ShapeId visitor_shape, const char* callback) {
  if (const auto pair = resolve(sensor_shape, visitor_shape))
    self.dispatch_collision(pair->first, pair->second, callback);
}

static void dispatch_contact_begin_event(stage& self, b2ShapeId shape_a, b2ShapeId shape_b, const b2Manifold& manifold) {
  const auto pair = resolve(shape_a, shape_b);
  if (!pair)
    return;

  const b2Vec2 flipped = {-manifold.normal.x, -manifold.normal.y};
  self.dispatch_collision(pair->first, pair->second, "on_collision_begin", &manifold.normal);
  self.dispatch_collision(pair->second, pair->first, "on_collision_begin", &flipped);
}

static void dispatch_contact_end_event(stage& self, b2ShapeId shape_a, b2ShapeId shape_b) {
  const auto pair = resolve(shape_a, shape_b);
  if (!pair)
    return;

  self.dispatch_collision(pair->first, pair->second, "on_collision_end");
  self.dispatch_collision(pair->second, pair->first, "on_collision_end");
}

static void on_object_destroy(entt::registry& registry, entt::entity entity) {
  auto& bo = registry.get<body>(entity);
  if (b2Body_IsValid(bo.id))
    b2DestroyBody(bo.id);
}

static int world_raycast(lua_State* state) {
  auto* self = upvalue<stage>(state);
  const auto* caller = check<objectproxy>(state, 1, "Object");
  const auto x = check<float>(state, 2);
  const auto y = check<float>(state, 3);
  const auto angle = check<float>(state, 4);
  const auto distance = check<float>(state, 5);
  return self->raycast(state, caller->entity, x, y, angle, distance);
}

static int world_radar(lua_State* state) {
  auto* self = upvalue<stage>(state);
  const auto* caller = check<objectproxy>(state, 1, "Object");
  const auto x = check<float>(state, 2);
  const auto y = check<float>(state, 3);
  const auto radius = check<float>(state, 4);
  return self->radar(state, caller->entity, x, y, radius);
}

static int world_at(lua_State* state) {
  auto* self = upvalue<stage>(state);
  const auto x = check<float>(state, 1);
  const auto y = check<float>(state, 2);
  return self->at(state, x, y);
}

static int world_pathfind(lua_State* state) {
  auto* self = upvalue<stage>(state);
  const auto x1 = check<float>(state, 1);
  const auto y1 = check<float>(state, 2);
  const auto x2 = check<float>(state, 3);
  const auto y2 = check<float>(state, 4);
  const auto r  = check<float>(state, 5);
  return self->pathfind(state, x1, y1, x2, y2, r);
}

static int world_spawn(lua_State* state) {
  auto* self = upvalue<stage>(state);
  const auto name = check<std::string_view>(state, 1);
  const auto kind = check<std::string_view>(state, 2);
  const auto x = check<float>(state, 3);
  const auto y = check<float>(state, 4);
  return self->spawn(state, name, kind, x, y);
}

static int world_destroy(lua_State* state) {
  auto* self = upvalue<stage>(state);
  return self->destroy(state);
}

stage::stage(std::string_view name)
    : _name(name) {
  _registry.on_destroy<objectproxy>().connect<&on_objectproxy_destroy>();
  _registry.on_destroy<body>().connect<&on_object_destroy>();

  _registry.ctx().emplace<stringpool*>(&_stringpool);


  const auto filename = std::format("stages/{}.lua", name);
  const auto buffer = io::read(filename);
  const auto label = std::format("@{}", filename);
  compile(L, buffer, label);

  lua_newtable(L);
  lua_newtable(L);
  lua_pushvalue(L, LUA_GLOBALSINDEX);
  lua_setfield(L, -2, "__index");
  lua_setmetatable(L, -2);
  _environment_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_newtable(L);
  _pool_reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
  lua_setfield(L, -2, "pool");
  lua_newtable(L);
  bind(L, "raycast", world_raycast, this);
  bind(L, "radar", world_radar, this);
  bind(L, "at", world_at, this);
  bind(L, "pathfind", world_pathfind, this);
  bind(L, "spawn", world_spawn, this);
  bind(L, "destroy", world_destroy, this);
  lua_setfield(L, -2, "world");
  lua_pop(L, 1);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _environment_reference);
  lua_setfenv(L, -2);

  pcall(L, 0, 1);

  b2Vec2 gravity{.0f, .0f};
  lua_getfield(L, -1, "gravity");
  if (lua_istable(L, -1)) {
    gravity.x = get<float>(L, -1, 1);
    gravity.y = get<float>(L, -1, 2);
  }
  lua_pop(L, 1);

  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = gravity;
  _world = b2CreateWorld(&def);

  const auto largest = std::max(viewport.width, viewport.height);
  _sleep_margin = largest * 1.f;
  _wake_margin  = largest * .5f;

  lua_getfield(L, -1, "objects");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      const auto object_name = std::string(get<std::string_view>(L, -1, "name"));
      const auto object_kind = std::string(get<std::string_view>(L, -1, "kind"));

      const auto ox = get<float>(L, -1, "x");
      const auto oy = get<float>(L, -1, "y");

      lua_pop(L, 1);

      std::ignore = spawn(L, object_name, object_kind, ox, oy);
      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "sounds");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));
    _sounds.reserve(static_cast<size_t>(count));

    for (auto i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      if (lua_istable(L, -1)) {
        const auto sound_name = std::string(get<std::string_view>(L, -1, "name"));

        const auto autoplay = get<bool>(L, -1, "autoplay");
        const auto loop = get<bool>(L, -1, "loop");

        auto& instance = depot->sound.get(std::format("sounds/{}", sound_name));
        pushuserdata(L, &instance, "Sound");

        lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
        lua_pushvalue(L, -2);
        lua_setfield(L, -2, sound_name.data());
        lua_pop(L, 1);

        _sounds.emplace_back(&instance);
        lua_pop(L, 1);

        if (loop)
          instance.set_loop(true);

        if (autoplay)
          instance.play();
      }

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  {
    const auto tilemap_name = get<std::string_view>(L, -1, "tilemap");
    if (!tilemap_name.empty())
      _tilemap = tilemap(tilemap_name, _world);
  }

  lua_getfield(L, -1, "overlay");
  if (lua_istable(L, -1)) {
    const auto widgets = get<std::string_view>(L, -1, "widgets");
    if (!widgets.empty())
      _overlay = std::string{widgets};

    const auto fg = get<std::string_view>(L, -1, "foreground");
    if (!fg.empty())
      _foreground = std::string{fg};
  }
  lua_pop(L, 1);

  lua_getfield(L, -1, "particles");
  if (lua_istable(L, -1)) {
    const auto count = static_cast<int>(lua_objlen(L, -1));

    for (int i = 1; i <= count; ++i) {
      lua_rawgeti(L, -1, i);

      const auto particle_name = std::string(get<std::string_view>(L, -1, "name"));
      const auto particle_kind = std::string(get<std::string_view>(L, -1, "kind"));

      const auto px = get<float>(L, -1, "x");
      const auto py = get<float>(L, -1, "y");
      const auto active = get<bool>(L, -1, "active", true);
      const auto sound_name = get<std::string_view>(L, -1, "sound");
      const auto particle_distance = get<float>(L, -1, "distance", 300.f);
      const auto particle_volume = get<float>(L, -1, "volume", 1.f);

      lua_pop(L, 1);

      auto& instance = _particlesystem.add(particle_name, particle_kind, px, py, active);

      if (!sound_name.empty()) {
        auto& fx = depot->sound.get(std::format("sounds/{}", sound_name));
        fx.set_loop(true);

        // if (std::ranges::find(_sounds, &fx) == _sounds.end())
        //   _sounds.emplace_back(&fx);

        instance.set_sound(&fx, particle_distance, particle_volume);
      }

      pushuserdata(L, &instance, "Particle");

      lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
      lua_pushvalue(L, -2);
      lua_setfield(L, -2, particle_name.data());
      lua_pop(L, 1);

      lua_pop(L, 1);
    }
  }
  lua_pop(L, 1);

  _reference = luaL_ref(L, LUA_REGISTRYINDEX);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  _on_loop = acquire(L, -1, "on_loop");
  _on_camera = acquire(L, -1, "on_camera");
  _on_tick = acquire(L, -1, "on_tick");
  lua_pop(L, 1);
}

stage::~stage() {
  release(L, _on_tick);
  release(L, _on_camera);
  release(L, _on_loop);
  release(L, _pool_reference);
  release(L, _environment_reference);
  release(L, _reference);
  b2DestroyWorld(_world);
}

auto stage::overlay() const noexcept -> const std::optional<std::string>& {
  return _overlay;
}

auto stage::foreground() const noexcept -> const std::optional<std::string>& {
  return _foreground;
}

void stage::on_enter() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  auto callback = acquire(L, -1, "on_enter");
  lua_pop(L, 1);
  invoke(L, callback, _reference);
  release(L, callback);
}

void stage::on_leave() {
  lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);
  auto callback = acquire(L, -1, "on_leave");
  lua_pop(L, 1);
  invoke(L, callback, _reference);
  release(L, callback);
}

void stage::on_tick(uint64_t tick) {
  invoke(L, _on_tick, _reference, tick);
}

void stage::update(float delta) {
  for (auto&& [entity, proxy, tf, an] :
       _registry.view<sleepable, objectproxy, transform, animation>().each()) {
    const auto& frame = an.clips[an.active].frames[an.current];
    const auto width  = frame.w * tf.scale;
    const auto height = frame.h * tf.scale;
    const auto screen_x = tf.x - viewport.x;
    const auto screen_y = tf.y - viewport.y;

    const auto offscreen =
      screen_x + width  < -_sleep_margin ||
      screen_x          >  viewport.width  + _sleep_margin ||
      screen_y + height < -_sleep_margin ||
      screen_y          >  viewport.height + _sleep_margin;

    const auto nearscreen =
      screen_x + width  >= -_wake_margin &&
      screen_x          <   viewport.width  + _wake_margin &&
      screen_y + height >= -_wake_margin &&
      screen_y          <   viewport.height + _wake_margin;

    if (nearscreen) {
      if (_registry.all_of<dormant>(entity)) {
        if (auto* bd = _registry.try_get<body>(entity);
            bd && b2Body_IsValid(bd->id)) {
          b2Body_Enable(bd->id);
        }
        _registry.remove<dormant>(entity);
        call(L, proxy.prototype, proxy.handle, "on_wake");
      }
    } else if (offscreen) {
      if (!_registry.all_of<dormant>(entity)) {
        _registry.emplace<dormant>(entity);

        if (auto* bd = _registry.try_get<body>(entity);
            bd && b2Body_IsValid(bd->id)) {
          bd->shape = b2_nullShapeId;
          bd->cached_hx = .0f;
          bd->cached_hy = .0f;
          b2Body_Disable(bd->id);
        }

        call(L, proxy.prototype, proxy.handle, "on_sleep");
      }
    }
  }

  invoke(L, _on_loop, _reference, delta);

  for (auto&& [entity, proxy] : _registry.view<objectproxy>(entt::exclude<dormant>).each()) {
    if (proxy.prototype == LUA_NOREF || proxy.handle == LUA_NOREF)
      continue;

    invoke(L, proxy.on_loop, proxy.handle, delta);

    auto* a = _registry.try_get<animation>(entity);
    if (!a || !a->playing || a->clip_count == 0)
      continue;

    const auto& c = a->clips[a->active];
    const auto& fr = c.frames[a->current];
    if (c.count == 0 || fr.duration < .0f)
      continue;

    a->elapsed += delta;

    if (a->elapsed >= fr.duration) {
      a->elapsed -= fr.duration;
      ++a->current;

      if (a->current >= c.count) {
        a->current = 0;

        invoke(L, proxy.on_animation_end, proxy.handle, _stringpool.get(c.name));
        invoke(L, proxy.on_animation_begin, proxy.handle, _stringpool.get(c.name));
      }
    }
  }

  _accumulator += delta;

  while (_accumulator >= _timestep) {
    for (auto&& [en, bd, an, tf] :
         _registry.view<body, animation, transform>(entt::exclude<dormant>).each()) {
      const auto& frame = an.clips[an.active].frames[an.current];
      const auto hx = frame.cw * .5f;
      const auto hy = frame.ch * .5f;

      if (!b2Shape_IsValid(bd.shape)) {
        const auto polygon = b2MakeBox(hx, hy);

        auto sdef = b2DefaultShapeDef();
        sdef.userData = to_userdata(en);
        sdef.enableContactEvents = true;
        sdef.enableSensorEvents = true;

        if (bd.type == body_type::dynamic) {
          sdef.density = 1.f;
          sdef.material.friction = .0f;
        }

        bd.shape = b2CreatePolygonShape(bd.id, &sdef, &polygon);
        bd.cached_hx = hx;
        bd.cached_hy = hy;

        const auto center = body_center(tf, frame, bd);

        if (bd.type == body_type::kinematic)
          b2Body_SetTargetTransform(bd.id, {center, b2Rot_identity}, _timestep);
        else
          b2Body_SetTransform(bd.id, center, b2Rot_identity);

        continue;
      }

      if (hx != bd.cached_hx || hy != bd.cached_hy) {
        const auto polygon = b2MakeBox(hx, hy);
        b2Shape_SetPolygon(bd.shape, &polygon);
        bd.cached_hx = hx;
        bd.cached_hy = hy;
      }

      if (bd.type == body_type::kinematic) {
        const auto center = body_center(tf, frame, bd);
        b2Body_SetTargetTransform(bd.id, {center, b2Rot_identity}, _timestep);
      }
    }

    b2World_Step(_world, _timestep, _substeps);

    const auto body_events = b2World_GetBodyEvents(_world);

    for (const auto& event : std::span(body_events.moveEvents, static_cast<size_t>(body_events.moveCount))) {
      const auto entity = to_entity(event.userData);

      if (!_registry.valid(entity)) [[unlikely]]
        continue;

      const auto* bd = _registry.try_get<body>(entity);
      if (!bd || bd->type != body_type::dynamic)
        continue;

      const auto* an = _registry.try_get<animation>(entity);
      if (!an || !an->playing || an->clip_count == 0)
        continue;

      auto& tf = _registry.get<transform>(entity);
      const auto& frame = an->clips[an->active].frames[an->current];
      const auto position = event.transform.p;
      tf.x = position.x - frame.cx * tf.scale - bd->cached_hx;
      tf.y = position.y - frame.cy * tf.scale - bd->cached_hy;

      _registry.get<renderable>(entity).z = static_cast<int>(tf.y + frame.h * tf.scale);
    }

    static constexpr std::string_view directions[] = {"left", "right", "top", "bottom"};

    const auto viewport_right  = viewport.x + viewport.width;
    const auto viewport_bottom = viewport.y + viewport.height;

    for (auto&& [entity, bd] : _registry.view<const body>(entt::exclude<dormant>).each()) {
      if (!b2Body_IsValid(bd.id))
        continue;

      if (bd.type == body_type::dynamic) {
        const auto capacity = b2Body_GetContactCapacity(bd.id);
        bool on_ground = false;
        entt::entity ride_target = entt::null;

        if (capacity > 0) {
          std::array<b2ContactData, 16> contacts{};
          const auto count = b2Body_GetContactData(bd.id, contacts.data(), static_cast<int>(contacts.size()));
          const auto* user = b2Shape_GetUserData(bd.shape);

          for (const auto& contact : std::span(contacts.data(), static_cast<size_t>(count))) {
            const auto& manifold = contact.manifold;
            const auto is_shape_a = b2Shape_GetUserData(contact.shapeIdA) == user;
            const auto normal_y = is_shape_a ? manifold.normal.y : -manifold.normal.y;
            if (normal_y > .5f) {
              on_ground = true;

              const auto other_shape = is_shape_a ? contact.shapeIdB : contact.shapeIdA;
              const auto other_body = b2Shape_GetBody(other_shape);
              if (b2Body_GetType(other_body) == b2_kinematicBody) {
                const auto* other_data = b2Shape_GetUserData(other_shape);
                if (other_data)
                  ride_target = to_entity(other_data);
              }

              break;
            }
          }
        }

        if (on_ground) {
          if (!_registry.all_of<grounded>(entity))
            _registry.emplace<grounded>(entity);
        } else {
          _registry.remove<grounded>(entity);
        }

        _registry.emplace_or_replace<riding>(entity, ride_target);
      }

      if (auto* sb = _registry.try_get<boundary>(entity);
          sb && b2Shape_IsValid(bd.shape)) {
        const auto* proxy = _registry.try_get<objectproxy>(entity);
        if (proxy) {
          const auto aabb = b2Shape_GetAABB(bd.shape);

          uint8_t current = 0;
          if (aabb.upperBound.x < viewport.x)
            current |= boundary::left;
          if (aabb.lowerBound.x > viewport_right)
            current |= boundary::right;
          if (aabb.upperBound.y < viewport.y)
            current |= boundary::top;
          if (aabb.lowerBound.y > viewport_bottom)
            current |= boundary::bottom;

          const auto exited  = static_cast<uint8_t>(current & ~sb->previous);
          const auto entered = static_cast<uint8_t>(sb->previous & ~current);

          for (uint8_t bit = 0; bit < 4; ++bit) {
            const auto mask = static_cast<uint8_t>(1u << bit);
            if (exited & mask)
              call(L, proxy->prototype, proxy->handle, "on_screen_exit", directions[bit]);
            if (entered & mask)
              call(L, proxy->prototype, proxy->handle, "on_screen_enter", directions[bit]);
          }

          sb->previous = current;
        }
      }
    }

    const auto sensor_events = b2World_GetSensorEvents(_world);

    for (const auto& event : std::span(sensor_events.beginEvents, static_cast<size_t>(sensor_events.beginCount)))
      dispatch_sensor_event(*this, event.sensorShapeId, event.visitorShapeId, "on_collision_begin");

    for (const auto& event : std::span(sensor_events.endEvents, static_cast<size_t>(sensor_events.endCount)))
      dispatch_sensor_event(*this, event.sensorShapeId, event.visitorShapeId, "on_collision_end");

    const auto contact_events = b2World_GetContactEvents(_world);

    for (const auto& event : std::span(contact_events.beginEvents, static_cast<size_t>(contact_events.beginCount)))
      dispatch_contact_begin_event(*this, event.shapeIdA, event.shapeIdB, event.manifold);

    for (const auto& event : std::span(contact_events.endEvents, static_cast<size_t>(contact_events.endCount)))
      dispatch_contact_end_event(*this, event.shapeIdA, event.shapeIdB);

    _accumulator -= _timestep;
  }

  for (auto* sound : _sounds) sound->poll();

  _particlesystem.update(delta);

  _registry.sort<renderable>([](const renderable& lhs, const renderable& rhs) noexcept {
    return lhs.z < rhs.z;
  }, entt::insertion_sort{});
}

void stage::draw() {
  viewport.x = .0f;
  viewport.y = .0f;

  if (_on_camera != LUA_NOREF) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, _on_camera);
    lua_rawgeti(L, LUA_REGISTRYINDEX, _reference);

    pcall(L, 1, 2);

    if (lua_isnumber(L, -2))
      viewport.x = std::roundf(static_cast<float>(lua_tonumber(L, -2)));
    if (lua_isnumber(L, -1))
      viewport.y = std::roundf(static_cast<float>(lua_tonumber(L, -1)));
    lua_pop(L, 2);
  }

  _tilemap.draw_background();

  for (auto entity : _registry.view<renderable>(entt::exclude<dormant>)) {
    const auto& [a, tf] = _registry.get<animation, transform>(entity);

    if (!tf.shown || !a.playing || !a.pixmap || a.clip_count == 0) [[unlikely]]
      continue;

    const auto& c = a.clips[a.active];
    if (c.count == 0)
      continue;

    const auto& fr = c.frames[a.current];

    const auto dw = fr.w * tf.scale;
    const auto dh = fr.h * tf.scale;
    const auto sx = tf.x - viewport.x;
    const auto sy = tf.y - viewport.y;

    if (sx + dw < .0f || sx > viewport.width ||
        sy + dh < .0f || sy > viewport.height)
      continue;

    a.pixmap->draw(
      fr.x, fr.y, fr.w, fr.h,
      sx, sy, dw, dh,
      static_cast<double>(tf.angle),
      static_cast<uint8_t>(std::clamp(tf.alpha, .0f, 255.f)),
      tf.flip
    );
  }

  _particlesystem.draw();

  _tilemap.draw_foreground();

#ifdef DEBUG
  SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

  const b2AABB aabb = {{viewport.x, viewport.y}, {viewport.x + viewport.width, viewport.y + viewport.height}};
  const b2QueryFilter filter = b2DefaultQueryFilter();
  const float offset[] = {viewport.x, viewport.y};

  b2World_OverlapAABB(_world, aabb, filter, [](b2ShapeId shape, void* userdata) -> bool {
    const auto* offset = static_cast<const float*>(userdata);

    const auto shape_aabb = b2Shape_GetAABB(shape);
    const auto lx = shape_aabb.lowerBound.x - offset[0];
    const auto ly = shape_aabb.lowerBound.y - offset[1];
    const auto ux = shape_aabb.upperBound.x - offset[0];
    const auto uy = shape_aabb.upperBound.y - offset[1];

    if (ux < .0f || lx > viewport.width || uy < .0f || ly > viewport.height)
      return true;

    const auto bid = b2Shape_GetBody(shape);
    const auto xf = b2Body_GetTransform(bid);
    const auto polygon = b2Shape_GetPolygon(shape);

    for (int i = 0; i < polygon.count; ++i) {
      const auto a = b2TransformPoint(xf, polygon.vertices[i]);
      const auto b = b2TransformPoint(xf, polygon.vertices[(i + 1) % polygon.count]);

      SDL_RenderLine(renderer, a.x - offset[0], a.y - offset[1], b.x - offset[0], b.y - offset[1]);
    }

    return true;
  }, const_cast<float*>(offset));

  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
#endif
}


void stage::dispatch_collision(entt::entity entity, entt::entity other, const char* callback, const b2Vec2* normal) {
  if (!_registry.valid(entity) || !_registry.valid(other)) return;
  if (!_registry.all_of<objectproxy>(entity) ||
      !_registry.all_of<objectproxy>(other)) return;

  const auto& self = _registry.get<objectproxy>(entity);
  const auto& target = _registry.get<objectproxy>(other);

  const auto* name = _stringpool.get(target.name);
  const auto* kind = _stringpool.get(target.kind);

  if (normal)
    call(L, self.prototype, self.handle, callback, name, kind, normal->x, normal->y);
  else
    call(L, self.prototype, self.handle, callback, name, kind);
}


int stage::radar(lua_State* state, entt::entity caller, float x, float y, float radius) {
  struct context {
    std::array<entt::entity, 32>* buffer;
    uint8_t count;
  };

  context ctx{&_radar_hits, 0};

  const b2Vec2 center{x, y};
  const auto proxy = b2MakeProxy(&center, 1, radius);
  const auto filter = b2DefaultQueryFilter();

  b2World_OverlapShape(
    _world, &proxy, filter,
    [](b2ShapeId shape, void* userdata) -> bool {
      auto* ctx = static_cast<context*>(userdata);
      if (ctx->count >= 64) [[unlikely]]
        return false;
      (*ctx->buffer)[ctx->count++] = to_entity(b2Shape_GetUserData(shape));
      return true;
    },
    &ctx
  );

  lua_newtable(state);
  int index = 1;

  const auto result = std::span(_radar_hits.data(), ctx.count);
  for (const auto entity : result) {
    if (entity == caller)
      continue;

    if (!_registry.valid(entity) || !_registry.all_of<objectproxy>(entity))
      continue;

    const auto& proxy_obj = _registry.get<objectproxy>(entity);
    if (proxy_obj.handle == LUA_NOREF)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy_obj.handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

int stage::raycast(lua_State* state, entt::entity caller, float x, float y, float angle, float distance) {
  struct context {
    std::array<raycast_hit, 32>* buffer;
    uint8_t count;
  };

  context ctx{&_raycast_hits, 0};

  const auto radians = to_radians(angle);
  auto sine = .0f, cosine = .0f;
  sincos(radians, sine, cosine);
  const b2Vec2 origin{x, y};
  const b2Vec2 translation{cosine * distance, sine * distance};
  const auto filter = b2DefaultQueryFilter();

  b2World_CastRay(
    _world,
    origin,
    translation,
    filter,
    [](b2ShapeId shape, b2Vec2, b2Vec2, float fraction, void* userdata) -> float {
      auto* ctx = static_cast<context*>(userdata);
      if (ctx->count >= 64) [[unlikely]]
        return -1.f;
      (*ctx->buffer)[ctx->count++] = {to_entity(b2Shape_GetUserData(shape)), fraction};
      return 1.f;
    },
    &ctx
  );

  const auto result = std::span(_raycast_hits.data(), ctx.count);
  std::ranges::sort(result, {}, &raycast_hit::fraction);

  lua_newtable(state);
  int index = 1;

  for (const auto& [entity, fraction] : result) {
    if (entity == entt::null)
      continue;

    if (entity == caller)
      continue;

    if (!_registry.valid(entity) || !_registry.all_of<objectproxy>(entity))
      continue;

    const auto& proxy = _registry.get<objectproxy>(entity);
    if (proxy.handle == LUA_NOREF)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy.handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

int stage::pathfind(lua_State* state, float x1, float y1, float x2, float y2, float radius) noexcept {
  return _tilemap.pathfind(state, x1, y1, x2, y2, radius);
}

int stage::spawn(lua_State* state, std::string_view name, std::string_view kind, float x, float y) {
  const auto entity = _registry.create();
  _registry.emplace<renderable>(entity);
  auto& tf = _registry.emplace<transform>(entity);
  tf.x = x;
  tf.y = y;
  const auto& proxy = _registry.emplace<objectproxy>(entity, _registry, entity, name, kind, _environment_reference);
  const auto prototype = proxy.prototype;
  const auto handle = proxy.handle;

  static_cast<void>(_stringpool.insert(name));
  static_cast<void>(_stringpool.insert(kind));

  lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
  lua_getfield(L, -1, "animation");

  if (lua_istable(L, -1)) {
    auto& a = _registry.emplace<animation>(entity);
    a.pixmap = &depot->pixmap.get(std::format("objects/{}", kind));

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
      if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        continue;
      }

      if (a.clip_count >= a.clips.size()) {
        lua_pop(L, 2);
        break;
      }

      const std::string_view clip_name = lua_tostring(L, -2);
      const auto cid = _stringpool.insert(clip_name);

      auto& c = a.clips[a.clip_count];
      c.name = cid;
      c.count = 0;

      {
        const auto frame_count = static_cast<int>(lua_objlen(L, -1));

        for (int f = 1; f <= frame_count && c.count < c.frames.size(); ++f) {
          lua_rawgeti(L, -1, f);

          if (lua_istable(L, -1)) {
            auto& fr = c.frames[c.count];

            fr.x = get<float>(L, -1, 1);
            fr.y = get<float>(L, -1, 2);
            fr.w = get<float>(L, -1, 3);
            fr.h = get<float>(L, -1, 4);
            fr.duration = get<float>(L, -1, 5) / 1000.f;

            lua_rawgeti(L, -1, 6);
            if (!lua_isnil(L, -1)) {
              fr.cx = static_cast<float>(lua_tonumber(L, -1));
              lua_pop(L, 1);
              fr.cy = get<float>(L, -1, 7);
              fr.cw = get<float>(L, -1, 8);
              fr.ch = get<float>(L, -1, 9);
              fr.collidable = true;
            } else {
              lua_pop(L, 1);
            }

            ++c.count;
          }

          lua_pop(L, 1);
        }
      }

      {
        const auto sound_name = get<std::string_view>(L, -1, "sound");
        if (!sound_name.empty())
          c.fx = &depot->sound.get(std::format("sounds/{}", sound_name));
      }

      ++a.clip_count;
      lua_pop(L, 1);
    }

    if (a.clip_count > 0) {
      a.playing = true;

      const auto default_clip = get<std::string_view>(L, -1, "default");
      if (!default_clip.empty()) {
        const auto hash = entt::hashed_string{default_clip.data()}.value();
        for (uint8_t i = 0; i < a.clip_count; ++i) {
          if (a.clips[i].name == hash) {
            a.active = i;
            break;
          }
        }
      }
    }

    const auto& fr = a.clips[a.active].frames[0];
    _registry.get<renderable>(entity).z = static_cast<int>(tf.y + fr.h * tf.scale);

    bool collidable = false;
    for (uint8_t ci = 0; ci < a.clip_count && !collidable; ++ci)
      for (uint8_t fi = 0; fi < a.clips[ci].count && !collidable; ++fi)
        collidable = a.clips[ci].frames[fi].collidable;

    if (collidable) {
      lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
      const auto str = get<std::string_view>(L, -1, "body");
      lua_pop(L, 1);

      const auto type = str == "dynamic" ? body_type::dynamic
                    : str == "static" ? body_type::stationary
                                    : body_type::kinematic;

      b2BodyDef bdef = b2DefaultBodyDef();
      bdef.userData = to_userdata(entity);

      bdef.type = type == body_type::dynamic ? b2_dynamicBody
               : type == body_type::stationary ? b2_staticBody
                                             : b2_kinematicBody;

      if (type == body_type::dynamic) {
        bdef.isBullet = true;
        bdef.fixedRotation = true;
      }

      const auto id = b2CreateBody(_world, &bdef);
      _registry.emplace<body>(entity, id, b2_nullShapeId, .0f, .0f, type);
      _registry.emplace<boundary>(entity);
    }
  }

  lua_pop(L, 2);

  {
    lua_rawgeti(L, LUA_REGISTRYINDEX, prototype);
    const auto wants_sleepable = get<bool>(L, -1, "sleepable");
    lua_pop(L, 1);

    if (wants_sleepable)
      _registry.emplace<sleepable>(entity);
  }

  call(L, prototype, handle, "on_spawn");

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
  lua_rawgeti(L, LUA_REGISTRYINDEX, handle);
  lua_setfield(L, -2, name.data());
  lua_pop(L, 1);

  if (_registry.all_of<sleepable, animation>(entity)) {
    const auto& an = _registry.get<animation>(entity);
    const auto& fr = an.clips[an.active].frames[an.current];
    const auto  w  = fr.w * tf.scale;
    const auto  h  = fr.h * tf.scale;
    const auto  sx = tf.x - viewport.x;
    const auto  sy = tf.y - viewport.y;

    const auto offscreen =
      sx + w < -_sleep_margin ||
      sx     >  viewport.width  + _sleep_margin ||
      sy + h < -_sleep_margin ||
      sy     >  viewport.height + _sleep_margin;

    if (offscreen) {
      _registry.emplace<dormant>(entity);

      if (auto* bd = _registry.try_get<body>(entity);
          bd && b2Body_IsValid(bd->id)) {
        bd->shape     = b2_nullShapeId;
        bd->cached_hx = .0f;
        bd->cached_hy = .0f;
        b2Body_Disable(bd->id);
      }
    }
  }

  lua_rawgeti(state, LUA_REGISTRYINDEX, handle);
  return 1;
}

int stage::destroy(lua_State* state) {
  auto* proxy = check<objectproxy>(state, 1, "Object");
  if (!_registry.valid(proxy->entity))
    return 0;

  const auto* object_name = _stringpool.get(proxy->name);

  lua_rawgeti(L, LUA_REGISTRYINDEX, _pool_reference);
  lua_pushnil(L);
  lua_setfield(L, -2, object_name);
  lua_pop(L, 1);

  _registry.destroy(proxy->entity);
  return 0;
}

uint8_t stage::at(float x, float y, entt::entity* buffer, uint8_t capacity) const noexcept {
  constexpr auto HALF = .5f;
  const b2AABB aabb = {{x - HALF, y - HALF}, {x + HALF, y + HALF}};
  const auto filter = b2DefaultQueryFilter();

  struct context {
    entt::entity* hits;
    uint8_t capacity;
    uint8_t count;
  };

  context ctx{buffer, capacity, 0};

  b2World_OverlapAABB(
    _world, aabb, filter,
    [](b2ShapeId shape, void* userdata) -> bool {
      auto* ctx = static_cast<context*>(userdata);
      if (ctx->count >= ctx->capacity) [[unlikely]]
        return false;
      ctx->hits[ctx->count++] = to_entity(b2Shape_GetUserData(shape));
      return true;
    },
    &ctx);

  return ctx.count;
}

int stage::at(lua_State* state, float x, float y) {
  std::array<entt::entity, 32> buffer;
  const auto count = at(x, y, buffer.data(), static_cast<uint8_t>(buffer.size()));

  lua_newtable(state);
  int index = 1;

  for (uint8_t i = 0; i < count; ++i) {
    const auto entity = buffer[i];
    if (!_registry.valid(entity) || !_registry.all_of<objectproxy>(entity))
      continue;

    const auto& proxy = _registry.get<objectproxy>(entity);
    if (proxy.handle == LUA_NOREF)
      continue;

    lua_rawgeti(state, LUA_REGISTRYINDEX, proxy.handle);
    lua_rawseti(state, -2, index++);
  }

  return 1;
}

entt::entity stage::find_topmost(std::span<const entt::entity> hits) const noexcept {
  if (hits.empty()) [[unlikely]]
    return entt::null;

  entt::entity topmost = entt::null;

  for (auto&& [entity, a, tf] : _registry.view<animation, transform>(entt::exclude<dormant>).each()) {
    if (!tf.shown) [[unlikely]]
      continue;

    for (const auto hit : hits) {
      if (hit == entity) {
        topmost = entity;
        break;
      }
    }
  }

  return topmost;
}
