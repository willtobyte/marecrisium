#include "stage.hpp"

stage::stage(std::string_view name) {
  b2WorldDef def = b2DefaultWorldDef();
  def.gravity = {.0f, .0f};
  _world = b2CreateWorld(&def);
}

stage::~stage() {
}

void stage::update(float delta) {
  const auto now = SDL_GetTicks();

  _accumulator += delta;

  while (_accumulator >= FIXED_TIMESTEP) {
    b2World_Step(_world, FIXED_TIMESTEP, WORLD_SUBSTEPS);
    _accumulator -= FIXED_TIMESTEP;
  }
}

void stage::draw() const {

  #ifdef DEVELOPMENT
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);

    const b2AABB aabb = {{0, 0}, {viewport.width, viewport.height}};
    const b2QueryFilter filter = b2DefaultQueryFilter();

    b2World_OverlapAABB(_world, aabb, filter, [](b2ShapeId shape, void*) -> bool {
      const auto box = b2Shape_GetAABB(shape);
      const SDL_FRect r{
        box.lowerBound.x,
        box.lowerBound.y,
        box.upperBound.x - box.lowerBound.x,
        box.upperBound.y - box.lowerBound.y
      };

      SDL_RenderRect(renderer, &r);

      return true;
    }, nullptr);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
  #endif
}
