---@meta

-- Mare Crisium API definitions
-- Auto-generated EmmyLua annotations for the Lua Language Server

--------------------------------------------------------------------------------
-- Keyboard
--------------------------------------------------------------------------------

---@class Keyboard
---Read-only. Access any field to check if a key is pressed.
---
---Supported keys:
--- Letters: a-z
--- Digits: 0-9
--- Arrows: up, down, left, right
--- Modifiers: shift, ctrl
--- Special: escape, space, enter, backspace, tab
---
---Usage: `if keyboard.w then ... end`
---@field a boolean
---@field b boolean
---@field c boolean
---@field d boolean
---@field e boolean
---@field f boolean
---@field g boolean
---@field h boolean
---@field i boolean
---@field j boolean
---@field k boolean
---@field l boolean
---@field m boolean
---@field n boolean
---@field o boolean
---@field p boolean
---@field q boolean
---@field r boolean
---@field s boolean
---@field t boolean
---@field u boolean
---@field v boolean
---@field w boolean
---@field x boolean
---@field y boolean
---@field z boolean
---@field ["0"] boolean
---@field ["1"] boolean
---@field ["2"] boolean
---@field ["3"] boolean
---@field ["4"] boolean
---@field ["5"] boolean
---@field ["6"] boolean
---@field ["7"] boolean
---@field ["8"] boolean
---@field ["9"] boolean
---@field up boolean
---@field down boolean
---@field left boolean
---@field right boolean
---@field shift boolean
---@field ctrl boolean
---@field escape boolean
---@field space boolean
---@field enter boolean
---@field backspace boolean
---@field tab boolean

---Global keyboard state.
---@type Keyboard
keyboard = {}

--------------------------------------------------------------------------------
-- Mouse
--------------------------------------------------------------------------------

---@class Mouse
---@field x number Mouse X position in world coordinates (logical position plus viewport offset, read-only).
---@field y number Mouse Y position in world coordinates (logical position plus viewport offset, read-only).
---@field button integer Currently pressed mouse button: 1 = left, 2 = middle, 3 = right, 0 = none (read-only).
---@field shown boolean Whether the mouse cursor is visible (read/write).
local Mouse = {}

---Global mouse state.
---@type Mouse
mouse = {}

--------------------------------------------------------------------------------
-- Text (typed input)
--------------------------------------------------------------------------------

---@class Text
local Text = {}

---Register the callback invoked whenever the user types text.
---Fires for committed UTF-8 input (respects keyboard layout and IME), not raw key codes.
---Calling this again replaces the previously registered callback.
---@param callback fun(text: string) Receives the UTF-8 text that was input.
function Text.on(callback) end

---Unregister the text callback and stop text input.
---Call this when the callback's owner, such as a stage, is no longer active.
function Text.off() end

---Global typed-text input.
---@type Text
text = {}

--------------------------------------------------------------------------------
-- Gamepad
--------------------------------------------------------------------------------

---@class Gamepad
---@field connected boolean Whether a gamepad is connected (read-only).
---@field name string Gamepad name, empty string if not connected (read-only).
---Axes (number, -1.0 to 1.0 with 0.1 deadzone):
---@field left_x number Left stick X axis.
---@field left_y number Left stick Y axis.
---@field right_x number Right stick X axis.
---@field right_y number Right stick Y axis.
---@field trigger_left number Left trigger.
---@field trigger_right number Right trigger.
---Buttons (boolean):
---@field south boolean A / Cross.
---@field east boolean B / Circle.
---@field west boolean X / Square.
---@field north boolean Y / Triangle.
---@field back boolean Back / Select.
---@field guide boolean Guide / Home.
---@field start boolean Start / Options.
---@field shoulder_left boolean Left bumper.
---@field shoulder_right boolean Right bumper.
---@field stick_left boolean Left stick press.
---@field stick_right boolean Right stick press.
---@field up boolean D-pad up.
---@field down boolean D-pad down.
---@field left boolean D-pad left.
---@field right boolean D-pad right.
local Gamepad = {}

---Vibrate the gamepad.
---@param low number Low-frequency motor intensity (0.0 to 1.0).
---@param high number High-frequency motor intensity (0.0 to 1.0).
---@param duration integer Duration in milliseconds.
---@return boolean success
function Gamepad:rumble(low, high, duration) end

---Set the gamepad LED color.
---@param r number Red intensity (0.0 to 1.0).
---@param g number Green intensity (0.0 to 1.0).
---@param b number Blue intensity (0.0 to 1.0).
---@return boolean success
function Gamepad:led(r, g, b) end

---Global gamepad state.
---@type Gamepad
gamepad = {}

--------------------------------------------------------------------------------
-- Cassette (persistent key-value store)
--------------------------------------------------------------------------------

---@class Cassette
---Persistent save-data store. Read/write arbitrary keys.
---
---Supported value types: `boolean`, `number`, `string`, `table`.
---Assign `nil` to delete a key. Accepted writes persist immediately.
---Tables are serialized as JSON and stored as JSONB in SQLite.
---Each root-key read loads the latest persisted value. Tables are returned
---as reactive proxies: mutating any nested field automatically re-persists
---the root key. Writes through a stale proxy are ignored after the same
---root key changes.
---
---Proxied tables support indexed access, `#`, `pairs()`, and `ipairs()`.
---
---Usage:
---```lua
---cassette.score = 100
---local s = cassette.score
---cassette.inventory = { "sword", "shield" }
---local count = #cassette.inventory
---for index, item in ipairs(cassette.inventory) do print(index, item) end
---cassette.progress = { level = 3, stars = 2 }
---cassette.progress.stars = 5       -- persists automatically
---cassette.score = nil              -- delete
---cassette:purge()                  -- delete all
---```
---@field [string] boolean|number|string|table|nil
local Cassette = {}

---Delete all saved data and clear the in-memory cache.
function Cassette:purge() end

---Global persistent storage.
---@type Cassette
cassette = {}

--------------------------------------------------------------------------------
-- Main Script (scripts/main.lua return table)
--------------------------------------------------------------------------------

---@class MainConfig
---@field width number Viewport width in logical pixels.
---@field height number Viewport height in logical pixels.
---@field title string Window title.
---@field splash string Splash screen image name. Resolves to `blobs/splashes/<name>.png`. Displayed full-screen on the first frame while the game loads.
---@field scale number Render scale factor.
---@field fullscreen boolean Whether to start in fullscreen mode.
---@field ticks integer|nil Fixed tick rate (ticks per second). Default is 0 (disabled). Set to e.g. 10 for 10 ticks/second.
---@field on_begin fun() Called once after the engine is fully initialized.

--------------------------------------------------------------------------------
-- Stage (scripts returned by stages/<name>.lua)
--------------------------------------------------------------------------------

---@class StageObject
---@field name string Unique instance name for this object. Accessible via `pool.<name>`.
---@field kind string Object type. Maps to `objects/<kind>.lua` (prototype) and `blobs/objects/<kind>.png` (spritesheet).
---@field x? number Initial X position. Default 0.
---@field y? number Initial Y position. Default 0.

---@class StageSound
---@field name string Sound name. Loads `sounds/<name>` and is accessible as `pool.<name>`.
---@field autoplay? boolean Whether to start playing immediately. Default false.
---@field loop? boolean Whether to enable looping. Default false.

---@class StageParticle
---@field name string Unique instance name for this particle emitter. Accessible via `pool.<name>`.
---@field kind string Particle type. Maps to `particles/<kind>.lua` (config) and `blobs/particles/<kind>.png` (texture).
---@field x? number Initial X position. Default 0.
---@field y? number Initial Y position. Default 0.
---@field active? boolean Whether particles spawn immediately. Default true.

---List of foreground names (each matching `foregrounds/<name>.lua`) to show
---when the stage becomes active. Order is z-order: index 1 is the back layer,
---last index is on top. All listed foregrounds are pre-loaded and made visible
---on stage entry; they can be toggled at runtime via `foregrounds.<name> = true|false`.
---On stage leave, every active foreground receives `on_disappear`.

---@class StageMinimap
---Minimap color configuration. Each field is an RGB triplet `{r, g, b}` (0-255).
---@field solid number[] Color for solid (collision) tiles.
---@field passable number[] Color for passable (free) tiles.
---@field void number[] Color for out-of-map areas.
---@field player number[] Color for the player marker.
---@field entity number[] Color for entity markers.

---@class Stage
---A stage script (`stages/<name>.lua`) returns a table that may contain
---these fields, lifecycle callbacks, and entity/sound declarations.
---@field gravity number[]|nil World gravity as {gx, gy}. Default is {0, 0} (no gravity).
---@field objects StageObject[]|nil Objects to spawn when the stage is created.
---@field sounds StageSound[]|nil Sounds to preload. Each entry is `{ name = "foo", autoplay = true }`. Loads `sounds/<name>` and is accessible as `pool.<name>`.
---@field particles StageParticle[]|nil Particle emitters to create. Each entry spawns a particle system accessible as `pool.<name>`.
---@field foregrounds string[]|nil Foregrounds shown when this stage is active, in z-order (index 1 is back). All listed foregrounds are pre-loaded; toggle individually at runtime via `foregrounds.<name> = true|false`.
---@field tilemap string|nil Tilemap name. Loads the binary navmap `tilemaps/<name>.bmap` produced by `assets/tilemaps/generate.py` (background, foreground, collision, components, JPS+ jump tables).
---@field minimap StageMinimap|nil Minimap color palette. Only used when `tilemap` is also set.
local Stage = {}

---Called when the director navigates to this stage.
---@param self table The stage table itself.
function Stage.on_enter(self) end

---Called when the director navigates away from this stage.
---@param self table The stage table itself.
function Stage.on_leave(self) end

---Called at fixed tick rate while this stage is active.
---Only fires when `ticks` is set in MainConfig.
---@param self table The stage table itself.
---@param tick number Monotonically increasing tick counter (starts at 1).
function Stage.on_tick(self, tick) end

---Called every frame while this stage is active.
---@param self table The stage table itself.
---@param delta number Frame delta time in seconds.
function Stage.on_loop(self, delta) end

---Called every frame before rendering.
---Return camera_x, camera_y to offset object and tilemap rendering.
---The engine automatically applies the returned camera to the tilemap.
---@param self table The stage table itself.
---@return number|nil camera_x Camera X offset for rendering.
---@return number|nil camera_y Camera Y offset for rendering.
function Stage.on_camera(self) end

---Called when a mouse button is pressed while the cursor is over no object (miss fallback).
---@param self table The stage table itself.
---@param x number Cursor X in world coordinates.
---@param y number Cursor Y in world coordinates.
---@param button "left"|"middle"|"right" The mouse button pressed.
function Stage.on_press(self, x, y, button) end

---Called when a mouse button is released while the cursor is over no object (miss fallback).
---@param self table The stage table itself.
---@param x number Cursor X in world coordinates.
---@param y number Cursor Y in world coordinates.
---@param button "left"|"middle"|"right" The mouse button released.
function Stage.on_release(self, x, y, button) end

--------------------------------------------------------------------------------
-- Director (stage navigation)
--------------------------------------------------------------------------------

---@class Director
local Director = {}

---Navigate to a named stage. Creates it if not enrolled.
---Triggers `on_leave` on the current stage.
---@param name string Stage name (matches `stages/<name>.lua`).
function Director.navigate(name) end

---Destroy a named stage. Cannot destroy the current stage.
---@param name string Stage name.
function Director.destroy(name) end

---Enroll a stage without navigating to it.
---@param name string Stage name.
function Director.enroll(name) end

---Global stage director.
---@type Director
director = {}

--------------------------------------------------------------------------------
-- Foreground (a single visible layer; many can be active simultaneously)
--------------------------------------------------------------------------------

---@class ForegroundSprite
---@field x number X position in logical pixels. Default 0.
---@field y number Y position in logical pixels. Default 0.
---@field width number Pixmap native width in pixels (read-only).
---@field height number Pixmap native height in pixels (read-only).
---@field angle number Rotation angle in degrees. Default 0.
---@field alpha number Opacity (0-255). Default 255.

---@class ForegroundConfig
---@field pixmaps string[] Pixmap names to preload from `blobs/foregrounds/<name>.png`.
---@field fonts? string[] Font families to preload from `fonts/<name>.lua`. Each declared font is exposed on the foreground table by family name (e.g. `self.pixel`, `self.small`) as a `Font` instance.

---Per-glyph visual effect applied to individual characters in a label.
---Each field is optional; omitted fields use their default (no effect).
---Newline characters do not count as glyphs for indexing purposes.
---
---Usage:
---```lua
---self.pixel:label("hello", 10, 20, {
---  [1] = { r = 1, g = 0, b = 0 },           -- 'h' in red
---  [3] = { y_offset = -2, scale = 1.5 },      -- first 'l' raised and scaled
---  [5] = { alpha = 0.5 },                     -- 'o' semi-transparent
---})
---```
---@class GlyphEffect
---@field x_offset? number Horizontal pixel offset for this glyph. Default 0.
---@field y_offset? number Vertical pixel offset for this glyph. Default 0.
---@field scale? number Scale factor for this glyph. Default 1.
---@field angle? number Rotation angle in degrees for this glyph, around its center. Default 0.
---@field r? number Red channel for this glyph (0.0-1.0). Default 1.
---@field g? number Green channel for this glyph (0.0-1.0). Default 1.
---@field b? number Blue channel for this glyph (0.0-1.0). Default 1.
---@field alpha? number Opacity for this glyph (0.0-1.0). Default 1.

---@class Font
---A font instance bound to a foreground. The same family across foregrounds
---resolves to the same underlying texture and metrics (cached in the engine),
---so declaring a font in multiple foregrounds is cheap.
local Font = {}

---Draw a text label at the given position using this font.
---Optionally accepts a table of per-glyph effects keyed by 1-based visible
---character index (newlines are not counted). Each entry is a GlyphEffect
---table that overrides position, scale, color, and rotation for that specific
---glyph. Indices without an entry render normally (white, no offset, scale 1).
---Each call submits one batched draw to the GPU.
---@param text string The text to render.
---@param x number X position in logical pixels.
---@param y number Y position in logical pixels.
---@param effects? table<integer, GlyphEffect> Per-glyph effects keyed by 1-based visible character index.
function Font:label(text, x, y, effects) end

---@class Foreground
local Foreground = {}

---Called when the foreground becomes visible. Fires every time the foreground
---is activated via `foregrounds.<name> = true` or implicitly by entering a
---stage that lists it, including subsequent activations after it was hidden.
---Foregrounds are cached, so the same instance receives `on_appear` again
---when it is shown after being hidden.
---@param self Foreground The foreground table itself.
function Foreground.on_appear(self) end

---Called when the foreground stops being visible. Fires every time the
---foreground is hidden via `foregrounds.<name> = false` (or `nil`), or when
---the active stage transitions away. The instance stays cached and may
---receive `on_appear` again later.
---@param self Foreground The foreground table itself.
function Foreground.on_disappear(self) end

---Called every frame while this foreground is active (logic only, no rendering).
---Sprites declared in `pixmaps` are accessible as `self.<name>` and have
---mutable `x`, `y`, `width`, `height`, `alpha`, `angle` properties.
---@param self Foreground The foreground table itself.
---@param delta number Frame delta time in seconds.
function Foreground.on_loop(self, delta) end

---Called every frame to render foreground elements. Use `self:draw()` for
---batched sprite quads from the pixmap, and `self.<font>:label(...)` for text
---using any font declared in this foreground's `fonts` list. Each call enqueues
---a draw that is flushed after `on_paint` returns.
---@param self Foreground The foreground table itself.
function Foreground.on_paint(self) end

---Submit a batch of quads for rendering using the foreground's pixmap texture.
---`buffer` is a flat array of numbers in groups of 6: `{x, y, w, h, angle, alpha, ...}`.
---`count` is the total number of elements (must be a positive multiple of 6).
---Each group of 6 defines one quad: destination x/y, width/height, rotation angle in
---degrees, and alpha (0-255). Quads with alpha 0 are skipped. Call once per frame
---inside `on_paint`.
---@param buffer number[] Flat array of quad data: repeating {x, y, w, h, angle, alpha}.
---@param count integer Total number of elements in buffer (multiple of 6).
function Foreground:draw(buffer, count) end

---Global foreground instance (nil when no foreground is active).
--------------------------------------------------------------------------------
-- Foregrounds (the global container of active foreground layers)
--------------------------------------------------------------------------------

---@class Foregrounds
---Container for any number of simultaneously visible foregrounds. Assign
---`true` to a foreground name to show it (loading and caching the instance
---on first use); assign `false` or `nil` to hide it (the cached instance is
---preserved for later reuse). Multiple foregrounds can be active at the same
---time; they are drawn in the z-order defined by the active stage's
---`foregrounds` list, with later entries on top.
---@field [string] boolean Write-only. `foregrounds.<name> = true` shows the foreground; `false`/`nil` hides it.
local Foregrounds = {}

---Global foregrounds container. Always present.
---
---Usage:
---```lua
---foregrounds.inventory = true    -- show on top of whatever is already visible
---foregrounds.dialog    = true    -- show as well; both stay visible
---foregrounds.inventory = false   -- hide just inventory; dialog remains
---foregrounds.inventory = nil     -- same as false
---```
---@type Foregrounds
foregrounds = {}

--------------------------------------------------------------------------------
-- Viewport
--------------------------------------------------------------------------------

---@class Viewport
---@field width number Viewport width in logical units (read-only).
---@field height number Viewport height in logical units (read-only).
---@field scale number Render scale factor (read-only).

---Global viewport dimensions.
---@type Viewport
viewport = {}

--------------------------------------------------------------------------------
-- ObjectPrototype (table returned by objects/<kind>.lua)
--------------------------------------------------------------------------------

---@class ObjectPrototype
---@field body? "dynamic"|"kinematic"|"static" Physics body type. Default is "kinematic".
---@field animation table<string, AnimationClip> Animation clips. Each clip is named and contains frames plus an optional sound.

---@class AnimationClip
---An animation clip is an array of frames with an optional sound effect.
---Each frame is: {source_x, source_y, source_width, source_height, duration_ms [, collider_x, collider_y, collider_width, collider_height]}.
---@field sound? string Optional sound name. Loads `blobs/sounds/<name>.opus` and plays automatically when this clip is activated via `self.animation = "clip_name"`.
---@field [integer] number[] Frame data: {source_x, source_y, source_width, source_height, duration_ms [, collider_x, collider_y, collider_width, collider_height]}.
---@field sleepable? boolean If true, the object sleeps (pauses all callbacks) when off-screen by more than 32 px. Default false.
---@field on_spawn? fun(self: Object) Called once when the object is created.
---@field on_loop? fun(self: Object, delta: number) Called every frame. Not called while the object is dormant.
---@field on_sleep? fun(self: Object) Called when the object goes off-screen and enters sleep (only on sleepable objects).
---@field on_wake? fun(self: Object) Called when the object returns on-screen and wakes up (only on sleepable objects).
---@field on_screen_exit? fun(self: Object, direction: "left"|"right"|"top"|"bottom") Called when the object's physics body exits the viewport on the given side.
---@field on_screen_enter? fun(self: Object, direction: "left"|"right"|"top"|"bottom") Called when the object's physics body re-enters the viewport from the given side.
---@field on_collision_begin? fun(self: Object, name: string, kind: string, normal_x?: number, normal_y?: number) Called on physics contact begin. `name` and `kind` refer to the other object involved in the collision.
---@field on_collision_end? fun(self: Object, name: string, kind: string) Called on physics contact end. `name` and `kind` refer to the other object involved in the collision.
---@field on_animation_end? fun(self: Object, clip_name: string) Called when an animation clip finishes or is replaced.
---@field on_animation_begin? fun(self: Object, clip_name: string) Called when a new animation clip starts playing.
---@field on_press? fun(self: Object, x: number, y: number, button: "left"|"middle"|"right") Called when a mouse button is pressed over the topmost visible object under the cursor. `x`/`y` are world coordinates.
---@field on_release? fun(self: Object, x: number, y: number, button: "left"|"middle"|"right") Called when a mouse button is released over the topmost visible object under the cursor. `x`/`y` are world coordinates.
---@field on_hover? fun(self: Object) Called the first frame the cursor moves over the object.
---@field on_unhover? fun(self: Object) Called the first frame the cursor leaves the object.
---@field [string] any Custom properties accessible via the object instance.

--------------------------------------------------------------------------------
-- Object (entity userdata, available as `self` and in `pool`)
--------------------------------------------------------------------------------

---@class Object
---@field x number Transform X position. For dynamic and static bodies, setting this teleports the body.
---@field y number Transform Y position. For dynamic and static bodies, setting this teleports the body.
---@field center_x number World X coordinate of the body's hitbox center, derived from the current animation frame's collider (read-only).
---@field center_y number World Y coordinate of the body's hitbox center, derived from the current animation frame's collider (read-only).
---@field velocity_x number Linear velocity X component. Readable on all body types; writable only on dynamic bodies.
---@field velocity_y number Linear velocity Y component. Readable on all body types; writable only on dynamic bodies.
---@field scale number Transform scale factor.
---@field angle number Rotation angle in degrees.
---@field alpha number Opacity (0-255).
---@field shown boolean Whether the object is visible.
---@field flip integer Flip mode for rendering. Use `flip.none`, `flip.horizontal`, `flip.vertical`, or `flip.both`.
---@field name string The object's name (read-only).
---@field kind string The kind/type string of this object (read-only).
---@field z integer Render Z-order layer. Higher draws on top (read/write).
---@field alive boolean Whether the object is still alive (read-only).
---@field dormant boolean Whether this object is currently sleeping (read-only). Always false for non-sleepable objects.
---@field animation string|nil Currently playing animation clip name. Assign to switch clips.
---@field [string] any Custom properties from the prototype table.
---
---Method dispatch: calling `pool.<name>:<method>(...)` resolves to
---`on_<method>(self, ...)` in the target object's prototype. The engine
---automatically prepends the `on_` prefix when looking up methods.
---For example, `pool.player:damage()` invokes `on_damage(self)` defined
---in the player's prototype (`objects/player.lua`).
local Object = {}

---Called once when the object is created during stage construction.
---@param self Object
function Object.on_spawn(self) end

---Called every frame. Use for movement, AI, input handling, etc.
---@param self Object
---@param delta number Frame delta time in seconds.
function Object.on_loop(self, delta) end

---Called when a physics contact begins with another object.
---For contact events, the collision normal is provided. The normal points from self toward the other object.
---@param self Object
---@param name string Name of the other object involved in the collision.
---@param kind string Kind/type of the other object involved in the collision.
---@param normal_x? number X component of the contact normal (nil for sensor events).
---@param normal_y? number Y component of the contact normal (nil for sensor events).
function Object.on_collision_begin(self, name, kind, normal_x, normal_y) end

---Called when a physics contact ends with another object.
---@param self Object
---@param name string Name of the other object involved in the collision.
---@param kind string Kind/type of the other object involved in the collision.
function Object.on_collision_end(self, name, kind) end

---Called when an animation clip finishes (loops or is replaced).
---@param self Object
---@param clip_name string Name of the clip that ended.
function Object.on_animation_end(self, clip_name) end

---Called when a new animation clip starts playing.
---@param self Object
---@param clip_name string Name of the clip that started.
function Object.on_animation_begin(self, clip_name) end

---Called when a mouse button is pressed and this object is the topmost visible object under the cursor.
---@param self Object
---@param x number Cursor X in world coordinates.
---@param y number Cursor Y in world coordinates.
---@param button "left"|"middle"|"right" The mouse button pressed.
function Object.on_press(self, x, y, button) end

---Called when a mouse button is released and this object is the topmost visible object under the cursor.
---@param self Object
---@param x number Cursor X in world coordinates.
---@param y number Cursor Y in world coordinates.
---@param button "left"|"middle"|"right" The mouse button released.
function Object.on_release(self, x, y, button) end

---Called the first frame the cursor moves over this object.
---@param self Object
function Object.on_hover(self) end

---Called the first frame the cursor leaves this object.
---@param self Object
function Object.on_unhover(self) end

--------------------------------------------------------------------------------
-- Sound (audio handle userdata, available in `pool`)
--------------------------------------------------------------------------------

---@class Sound
---@field volume number Current volume, 0.0 to 1.0.
---@field pan number Current stereo pan, -1.0 (left) to 1.0 (right).
---@field loop boolean Whether looping is enabled.
---@field playing boolean Whether the sound is currently playing (read-only).
local Sound = {}

---Start playback from the beginning.
function Sound:play() end

---Stop playback.
function Sound:stop() end

---Fade the sound volume over time.
---@param from number Starting volume (0.0 to 1.0, or -1 to use current volume).
---@param to number Target volume (0.0 to 1.0).
---@param ms integer Fade duration in milliseconds.
function Sound:fade(from, to, ms) end

---Register a callback for when playback starts.
---@param fn fun()
function Sound:on_begin(fn) end

---Register a callback for when playback ends.
---@param fn fun()
function Sound:on_end(fn) end

--------------------------------------------------------------------------------
-- Particle (particle emitter userdata, available in `pool`)
--------------------------------------------------------------------------------

---@class Particle
---@field x number Emitter X position (read/write).
---@field y number Emitter Y position (read/write).
---@field active boolean Whether dead particles respawn (read/write).
local Particle = {}

--------------------------------------------------------------------------------
-- World (physics, available per-stage)
--------------------------------------------------------------------------------

---@class World
local World = {}

---Spawn a new object at runtime and return it.
---@param name string Unique name for the object (used in pool).
---@param kind string Object kind (loads objects/<kind>.lua prototype).
---@param x number Initial X position.
---@param y number Initial Y position.
---@return Object object The newly created object.
function World.spawn(name, kind, x, y) end

---Destroy an object, removing it from the world and pool.
---@param object Object The object to destroy.
function World.destroy(object) end

---Cast a ray and return all hits sorted by distance.
---Tilemap solid tiles have no user data and are skipped; only Object entities are returned.
---Cast a ray from (x, y) in the given direction up to ``distance``. The ray
---stops at the first solid tilemap tile (line of sight is blocked by walls).
---Returns the entities hit along the way, sorted by distance.
---@param caller Object The object casting the ray (excluded from results).
---@param x number Ray origin X.
---@param y number Ray origin Y.
---@param angle number Ray angle in degrees.
---@param distance number Maximum ray distance.
---@return Object[] hits Sorted array of hit objects (in front of the first wall, if any).
function World.raycast(caller, x, y, angle, distance) end

---Return all objects within a circular area, excluding the caller.
---@param caller Object The object performing the scan (excluded from results).
---@param x number Center X.
---@param y number Center Y.
---@param radius number Circle radius.
---@return Object[] hits Array of objects within the area.
function World.radar(caller, x, y, radius) end

---Count objects of a given kind whose physics body overlaps the given rectangle.
---@param x number Left edge of the query region in world coordinates.
---@param y number Top edge of the query region in world coordinates.
---@param w number Width of the query region.
---@param h number Height of the query region.
---@param kind string Object kind to filter by (e.g. "enemy").
---@return integer count Number of matching objects.
function World.count(x, y, w, h, kind) end

---Find objects of a given kind whose physics body overlaps the given rectangle.
---@param x number Left edge of the query region in world coordinates.
---@param y number Top edge of the query region in world coordinates.
---@param w number Width of the query region.
---@param h number Height of the query region.
---@param kind string Object kind to filter by (e.g. "enemy").
---@return Object[] objects Array of matching objects.
function World.find(x, y, w, h, kind) end

---Physics world (available inside stage scripts).
---@type World
world = {}

--------------------------------------------------------------------------------
-- Flip (rendering mirror constants)
--------------------------------------------------------------------------------

---@class Flip
---@field none integer No flip (0).
---@field horizontal integer Flip horizontally (1).
---@field vertical integer Flip vertically (2).
---@field both integer Flip both axes (3).

---Global flip mode constants.
---@type Flip
flip = {}

--------------------------------------------------------------------------------
-- Minimap (toggle-able tile overview, available in `pool`)
--------------------------------------------------------------------------------

---@class Minimap
---A 33x33 pixel minimap centered on the player, showing nearby tiles and entities.
---Toggle visibility with M key or gamepad back button.
---@field visible boolean Whether the minimap is currently shown (read/write).
local Minimap = {}

--------------------------------------------------------------------------------
-- Pool (named collection of objects and sounds, available per-stage)
--------------------------------------------------------------------------------

---@class Pool
---Access objects, sounds, particle emitters, and minimap by name.
---@field minimap Minimap|nil The stage minimap (present when the stage has a tilemap and minimap config).
---@field [string] Object|Sound|Particle

---Resource pool (available inside stage scripts).
---@type Pool
pool = {}

--------------------------------------------------------------------------------
-- Achievement (Steam achievements)
--------------------------------------------------------------------------------

---@class Achievement
local Achievement = {}

---Unlock a Steam achievement by its API name.
---No-op if Steam is unavailable or the achievement is already unlocked.
---@param id string The achievement API name (e.g., "ACH_FIRST_BLOOD").
---@return boolean success Whether the unlock succeeded.
function Achievement:unlock(id) end

---Global achievement interface.
---@type Achievement
achievement = {}

--------------------------------------------------------------------------------
-- User (Steam user info)
--------------------------------------------------------------------------------

---@class Friend
---@field id number The friend's Steam ID (read-only).
---@field name string The friend's display name (read-only).

---@class User
local User = {}

---The local user's Steam display name. Empty string if Steam is unavailable.
---@type string
User.persona = ""

---List of Steam friends. Empty table if Steam is unavailable.
---@type Friend[]
User.friends = {}

---Global user interface.
---@type User
user = {}

--------------------------------------------------------------------------------
-- Platform (system information)
--------------------------------------------------------------------------------

---@class Platform
---@field name string Operating system name, e.g. "macOS", "Windows", "Linux" (read-only).
---@field cores integer Number of logical CPU cores (read-only).
---@field memory integer System RAM in megabytes (read-only).
---@field clipboard string System clipboard text (read/write).

---Global platform information.
---@type Platform
platform = {}

--------------------------------------------------------------------------------
-- Open URL
--------------------------------------------------------------------------------

---Open a URL in the system's default browser or handler.
---@param url string The URL to open.
function openurl(url) end

--------------------------------------------------------------------------------
-- Localization
--------------------------------------------------------------------------------

---Looks up key in the loaded locale file (locales/<lang>.lua, 2-letter code from SDL).
---Tries each preferred OS locale in order; first existing file wins.
---If no locale file is found (or the key is missing), the key itself is used,
---which is treated as the original English text written in the source.
---Extra arguments are applied via string.format on the resolved string,
---supporting %s, %d, %1$s positional specifiers, %% literals, etc.
---Format errors are silently ignored and the resolved template is returned as-is.
---@param key string Template string (may contain string.format placeholders)
---@vararg any Values to interpolate
---@return string
function _(key, ...) end

--------------------------------------------------------------------------------
-- Moment (time)
--------------------------------------------------------------------------------

---Returns the current time in milliseconds since engine initialization.
---Useful for absolute timestamps, countdowns, and sub-tick timing.
---@return number milliseconds
function moment() end

--------------------------------------------------------------------------------
-- Controls (unified keyboard + gamepad abstraction)
--------------------------------------------------------------------------------

---@class Controls
---Unified input abstraction that merges keyboard and gamepad into
---semantic game actions. Require via `require("helpers/controls")`.
---
---Directional inputs combine arrow keys, d-pad, and left stick.
---
---Usage:
---```lua
---local controls = require("helpers/controls")
---if controls.left then ... end
---```
---@field left boolean Arrow left, d-pad left, or left stick left.
---@field right boolean Arrow right, d-pad right, or left stick right.
---@field up boolean Arrow up, d-pad up, or left stick up.
---@field down boolean Arrow down, d-pad down, or left stick down.
---@field minimap boolean Gamepad back or M key.
local Controls = {}

--------------------------------------------------------------------------------
-- Random (xorshift128 replacement for math.random / math.randomseed)
--------------------------------------------------------------------------------

---The engine replaces Lua's built-in `math.random` and `math.randomseed` with
---a fast xorshift128 PRNG. The RNG state is **not** automatically seeded at
---startup, so `math.randomseed` **must** be called before the first use of
---`math.random`; otherwise the sequence is deterministic (all-zero state).
---Calling `math.randomseed` again at any point fully replaces the internal
---state, and all subsequent `math.random` calls use the new seed.
---
---Usage:
---```lua
---math.randomseed(moment())      -- seed once at startup
---local f = math.random()         -- float in [0, 1)
---local n = math.random(6)        -- integer in [1, 6]
---local m = math.random(10, 20)   -- integer in [10, 20]
---```

---Seed the xorshift128 PRNG. Must be called before any `math.random` call.
---Each call fully replaces the internal state; subsequent `math.random` calls
---produce a new sequence determined entirely by this seed.
---@param seed integer Seed value (truncated to 32 bits internally).
function math.randomseed(seed) end

---Generate a pseudo-random number using the xorshift128 PRNG.
---
---With no arguments, returns a float in [0, 1).
---With one argument `n`, returns an integer in [1, n].
---With two arguments, returns an integer in [minimum, maximum].
---@overload fun(): number
---@overload fun(n: integer): integer
---@param minimum integer Lower bound (inclusive).
---@param maximum integer Upper bound (inclusive).
---@return integer
function math.random(minimum, maximum) end

--------------------------------------------------------------------------------
-- Scheduler (coroutine-based task scheduler)
--------------------------------------------------------------------------------

---@class Scheduler
---Pure Lua coroutine scheduler driven by the engine's fixed tick rate.
---Require via `require("helpers/scheduler")`.
---
---Usage:
---```lua
---local scheduler = require("helpers/scheduler")
---
---local stop = scheduler.run(function(wait)
---    while self.alive do
---        self.direction = 1
---        wait(20)
---        self.direction = -1
---        wait(20)
---    end
---end)
---
----- Cancel early (e.g. when the entity dies). Idempotent: safe to
----- call again after the routine has already finished.
---stop()
---
---return scheduler.wrap({
---    ...
---})
---```
local Scheduler = {}

---Launch a function as a managed coroutine.
---`f` receives a `wait` function; call `wait(n)` to pause N ticks.
---Begins executing on the next scheduler advance.
---Returns a `stop` closure that cancels the routine: once called, the
---coroutine is never resumed again and is removed on the next advance.
---@param fn fun(wait: fun(ticks: integer)) The function to run.
---@return fun() stop Cancel closure; idempotent; safe after completion.
function Scheduler.run(fn) end

---Advance all ready coroutines. Called internally by `wrap`.
---@param current_tick integer The current tick counter.
function Scheduler.advance(current_tick) end

---Cancel all active coroutines.
function Scheduler.clear() end

---Decorate a stage to auto-advance on `on_tick` and auto-clear on `on_leave`.
---Chains with existing callbacks.
---@param stage Stage The stage table to wrap.
---@return Stage stage The same table, modified in place.
function Scheduler.wrap(stage) end
