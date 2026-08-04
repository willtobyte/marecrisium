---@meta

---@alias BodyType "dynamic"|"kinematic"|"static"
---@alias ScreenDirection "left"|"right"|"top"|"bottom"
---@alias MouseButton "left"|"middle"|"right"
---@alias FlipMode 0|1|2|3
---@alias Vector2 [number, number]
---@alias RGB [integer, integer, integer] Channels from 0 to 255.
---@alias ParticleRange [number, number]
---@alias AnimationFrame [number, number, number, number, number]|[number, number, number, number, number, number, number, number, number]

-- Keyboard

---Read-only keyboard state.
---@class Keyboard
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
---@field shift boolean Left Shift.
---@field ctrl boolean Left Ctrl.
---@field escape boolean
---@field space boolean
---@field enter boolean
---@field backspace boolean
---@field tab boolean

---@type Keyboard
keyboard = nil

-- Mouse

---Read-only position and button state; `shown` is read/write.
---@class Mouse
---@field x number World X coordinate (read-only).
---@field y number World Y coordinate (read-only).
---@field left boolean Left button state (read-only).
---@field middle boolean Middle button state (read-only).
---@field right boolean Right button state (read-only).
---@field shown boolean Cursor visibility (read/write).

---@type Mouse
mouse = nil

-- Gamepad

---Read-only gamepad state with a 0.1 deadzone. Sticks range from -1.0 to
---1.0; triggers range from 0.0 to 1.0.
---@class Gamepad
---@field connected boolean
---@field name string Empty when disconnected.
---@field left_x number
---@field left_y number
---@field right_x number
---@field right_y number
---@field trigger_left number
---@field trigger_right number
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

---Vibrate the gamepad. Intensities are clamped to 0.0-1.0.
---@param low number
---@param high number
---@param duration integer Milliseconds, from 0 to 4294967295.
---@return boolean
function Gamepad:rumble(low, high, duration) end

---Set the gamepad LED. Channels are clamped to 0.0-1.0.
---@param r number
---@param g number
---@param b number
---@return boolean
function Gamepad:led(r, g, b) end

---@type Gamepad
gamepad = nil

-- Cassette

---@alias CassetteValue boolean|number|string|table

---Persistent JSON-compatible Lua storage. Numbers must be finite. String values
---and table string keys must be valid UTF-8. Writes persist immediately; nil deletes.
---Nested writes persist. Proxies support `#`, `pairs`, and `ipairs`.
---@class Cassette
---@field [string] CassetteValue|nil
local Cassette = {}

---Delete all saved keys. The `purge` key is reserved.
function Cassette:purge() end

---@type Cassette
cassette = nil

-- Main script (`scripts/main.lua`)

---Startup configuration read once by the engine.
---@class MainConfig
---@field width integer Window width in pixels.
---@field height integer Window height in pixels.
---@field title string Window title.
---@field splash string Splash image name: `blobs/splashes/<name>.png`.
---@field scale number Render scale; logical viewport size is width/scale by height/scale.
---@field fullscreen? boolean Defaults to false.
---@field on_begin? fun() Runs once after engine initialization.

-- Particle config (`particles/<kind>.lua`)

---@class ParticleSpawnConfig
---@field x? ParticleRange Horizontal offset.
---@field y? ParticleRange Vertical offset.
---@field radius? ParticleRange Radial offset.
---@field angle? ParticleRange Angle in radians.
---@field scale? ParticleRange
---@field life? ParticleRange Lifetime in seconds.

---@class ParticleAxisConfig
---@field x? ParticleRange
---@field y? ParticleRange

---@class ParticleRotationConfig
---@field force? ParticleRange Angular acceleration in radians per second squared.
---@field velocity? ParticleRange Angular velocity in radians per second.

---Emitter configuration read when its kind is first loaded.
---@class ParticleConfig
---@field count integer Must be a positive multiple of 4.
---@field spawn? ParticleSpawnConfig
---@field velocity? ParticleAxisConfig
---@field gravity? ParticleAxisConfig
---@field rotation? ParticleRotationConfig

-- Stage (`stages/<name>.lua`)

---@class StageObject
---@field name string Pool name.
---@field kind string Loads `objects/<kind>.lua` and `blobs/objects/<kind>.png`.
---@field x? number Defaults to 0.
---@field y? number Defaults to 0.

---@class StageSound
---@field name string Pool name; loads `blobs/sounds/<name>.ogg`.
---@field loop? boolean Defaults to false.

---@class StageParticle
---@field name string Pool name.
---@field kind string Loads `particles/<kind>.lua` and `blobs/particles/<kind>.png`.
---@field x? number Defaults to 0.
---@field y? number Defaults to 0.
---@field active? boolean Whether dead particles respawn. Defaults to true.

---@class StageMinimap
---@field solid RGB
---@field passable RGB
---@field void RGB
---@field player RGB
---@field entity RGB

---Stage definition and callbacks owned by the script.
---@class Stage
---@field gravity? Vector2 Defaults to {0, 0}.
---@field substeps? integer Solver sub-steps per physics tick. Defaults to 1. Increase to 2 or 4 for stages with gravity or tight stacking.
---@field objects? StageObject[]
---@field sounds? StageSound[]
---@field particles? StageParticle[]
---@field foregrounds? string[] Shown in order; later entries draw on top.
---@field tilemap? string Loads `tilemaps/<name>.tilemap`.
---@field minimap? StageMinimap Requires a tilemap.
---@field on_enter? fun(self: Stage) Called when this stage becomes active.
---@field on_leave? fun(self: Stage) Called before this stage becomes inactive.
---@field on_text? fun(self: Stage, text: string) Receives committed UTF-8 text while active.
---@field on_loop? fun(self: Stage, delta: number) Called every active frame; delta is in seconds.
---@field on_camera? fun(self: Stage): number?, number? Returns the viewport world origin; nil preserves the previous coordinate.
---@field on_press? fun(self: Stage, x: number, y: number, button: MouseButton) Called when the physics query finds no shape under the cursor; x and y are world coordinates.
---@field on_release? fun(self: Stage, x: number, y: number, button: MouseButton) Called when the physics query finds no shape under the cursor; x and y are world coordinates.

-- Director

---@class Director
local Director = {}

---Queue navigation to an enrolled stage.
---@param name string Stage name.
function Director.navigate(name) end

---Destroy a cached stage. The stage must not be active or pending.
---@param name string Stage name.
function Director.destroy(name) end

---Create and cache a stage without navigation.
---The stage name must not be enrolled.
---@param name string Stage name.
function Director.enroll(name) end

---@type Director
director = nil

-- Foreground (`foregrounds/<name>.lua`)

---Mutable Lua snapshot; writes never affect the texture.
---@class ForegroundPixmap
---@field width integer Texture width.
---@field height integer Texture height.

---@class ForegroundConfig
---@field fonts? string[] Font families from `fonts/<name>.lua`.

---@class FontConfig
---@field glyphs string Single-byte glyphs in texture order.
---@field spacing? integer Defaults to 0.
---@field leading? integer Defaults to 0.
---@field scale? number Defaults to 1.

---@class GlyphEffect
---@field x_offset? number Defaults to 0.
---@field y_offset? number Defaults to 0.
---@field scale? number Defaults to 1.
---@field angle? number Degrees; defaults to 0.
---@field r? number Red channel, 0.0-1.0; defaults to 1.
---@field g? number Green channel, 0.0-1.0; defaults to 1.
---@field b? number Blue channel, 0.0-1.0; defaults to 1.
---@field alpha? number Opacity, 0.0-1.0; defaults to 1.

---@class Font
local Font = {}

---Draw up to 256 non-newline bytes. Effects use 1-based byte indices;
---newlines do not consume an index.
---@param text string
---@param x number
---@param y number
---@param effects? table<integer, GlyphEffect>
function Font:label(text, x, y, effects) end

---Script-owned configuration, callbacks, and custom state (read/write).
---@class ForegroundState: ForegroundConfig
---@field pixmap? ForegroundPixmap Available when the pixmap texture exists.
---@field on_appear? fun(self: ForegroundState) Called each time the foreground becomes visible.
---@field on_disappear? fun(self: ForegroundState) Called each time the foreground becomes hidden.
---@field on_loop? fun(self: ForegroundState, delta: number) Called every visible frame before painting; delta is in seconds.
---@field on_paint? fun(self: Foreground) Called every visible frame with the drawable state.
---@field [string] any Script fields and loaded fonts.

---Mutable foreground state passed directly to `on_paint`.
---@class Foreground: ForegroundState
local Foreground = {}

---Draw textured quads immediately from the foreground pixmap. `buffer` repeats
---`{x, y, width, height, angle_degrees, alpha_0_to_255}`.
---@param buffer number[]
---@param count integer Number of buffer elements; must be a positive multiple of 6.
function Foreground:draw(buffer, count) end

---Write-only foreground visibility. Assign true to show; false or nil to hide.
---Foregrounds draw in activation order, with the latest on top.
---@class Foregrounds
---@field [string] boolean|nil

---@type Foregrounds
foregrounds = nil

-- Viewport

---Mutable Lua snapshot of the viewport. Assignments do not affect rendering.
---@class Viewport
---@field width number Logical width.
---@field height number Logical height.
---@field scale number Render scale.

---@type Viewport
viewport = nil

-- Objects (`objects/<kind>.lua`)

---@class AnimationConfig
---@field default? string Initial clip. Without it, the first parsed clip is used.
---@field [string] AnimationClip|string

---Non-empty frame array with an optional sound.
---@class AnimationClip
---@field sound? string Plays `blobs/sounds/<name>.ogg` when selected through `object.animation`.
---@field [integer] AnimationFrame `{sx, sy, width, height, duration_ms}` with optional `{collider_x, collider_y, collider_width, collider_height}`.

---Spawn configuration and shared custom behavior. Engine configuration and
---callbacks are consumed internally and are unavailable through `Object`.
---@class ObjectPrototype
---@field body? BodyType Spawn-only; defaults to "kinematic".
---@field animation? AnimationConfig Spawn-only animation definitions.
---@field sleepable? boolean Spawn-only; defaults to false.
---@field on_spawn? fun(self: Object)
---@field on_loop? fun(self: Object, delta: number) Not called while dormant.
---@field on_sleep? fun(self: Object)
---@field on_wake? fun(self: Object)
---@field on_screen_exit? fun(self: Object, direction: ScreenDirection)
---@field on_screen_enter? fun(self: Object, direction: ScreenDirection)
---@field on_collision_begin? fun(self: Object, name: string, kind: string, normal_x?: number, normal_y?: number) Normal points from self to the other object; it is nil for sensor events.
---@field on_collision_end? fun(self: Object, name: string, kind: string)
---@field on_animation_end? fun(self: Object, clip: string) Called when a clip loops or is replaced.
---@field on_animation_begin? fun(self: Object, clip: string) Called after a selected clip starts and after each loop.
---@field on_press? fun(self: Object, x: number, y: number, button: MouseButton) Called for the topmost visible collider; the picker considers up to 16 overlaps.
---@field on_release? fun(self: Object, x: number, y: number, button: MouseButton) Called for the topmost visible collider; the picker considers up to 16 overlaps.
---@field on_hover? fun(self: Object) Called when the cursor enters the topmost visible collider.
---@field on_unhover? fun(self: Object) Called when the cursor leaves the topmost visible collider.
---@field [string] any Custom fields and methods shared by every object of this kind.

---Entity handle available as `self` and through `pool`. Calling
---`object:foo(...)` falls back to a non-reserved `on_foo(self, ...)` method.
---Custom writes update the shared prototype table for the object's kind.
---Spawn configuration and engine callbacks are not exposed.
---@class Object
---@field x number Transform X (read/write). Setting it teleports dynamic and static bodies.
---@field y number Transform Y (read/write). Setting it teleports dynamic and static bodies.
---@field center_x number Collider center X, or x when no collider exists (read-only).
---@field center_y number Collider center Y, or y when no collider exists (read-only).
---@field velocity_x number Linear velocity (read; write on dynamic bodies only).
---@field velocity_y number Linear velocity (read; write on dynamic bodies only).
---@field scale number Transform scale (read/write).
---@field angle number Rotation in degrees (read/write).
---@field alpha number Opacity, clamped to 0-255 (read/write).
---@field shown boolean Visibility (read/write).
---@field flip FlipMode Render mirroring (read/write).
---@field name string Instance name (read-only).
---@field kind string Prototype kind (read-only).
---@field z integer Render order (read/write); higher values draw on top.
---@field alive boolean Lifetime state (read-only); false after destruction.
---@field dormant boolean Sleep state (read-only).
---@field animation? string Current clip (read/write); unknown names are ignored.
---@field [string] any Non-reserved shared prototype field or dispatched method.

-- Sound

---@class Sound
---@field volume number Gain, clamped to 0.0-1.0 (read/write).
---@field pan number Stereo pan, clamped to -1.0-1.0 (read/write).
---@field loop boolean Looping state (read/write).
---@field playing boolean Playback state (read-only).
local Sound = {}

---Restart playback and run the `on_begin` callback.
function Sound:play() end

---Stop playback.
function Sound:stop() end

---Fade the volume.
---@param from number Start gain; a negative value uses the current gain.
---@param to number Target gain.
---@param ms integer Duration in milliseconds; negative values become 0.
function Sound:fade(from, to, ms) end

---Replace the playback-start callback.
---@param fn fun()
function Sound:on_begin(fn) end

---Replace the playback-end callback.
---@param fn fun()
function Sound:on_end(fn) end

-- Particle emitter

---Stage-scoped; do not retain after its stage is destroyed.
---@class Particle
---@field x number Emitter X (read/write).
---@field y number Emitter Y (read/write).
---@field active boolean Whether dead particles respawn (read/write).

-- World

---Stage-scoped physics API. Retained calls fail after the stage is destroyed.
---@class World
local World = {}

---Spawn an object. Returns nil if `on_spawn` destroys it.
---@param name string Unique pool name.
---@param kind string Object kind.
---@param x number
---@param y number
---@return Object?
function World.spawn(name, kind, x, y) end

---Destroy an object owned by this stage.
---@param object Object
function World.destroy(object) end

---Return the closest object hit before the first solid tile. The caller is ignored.
---@param caller Object
---@param x number Ray origin X.
---@param y number Ray origin Y.
---@param angle number Degrees.
---@param distance number
---@return Object?
function World.raycast(caller, x, y, angle, distance) end

---Return objects whose physics bodies overlap a circle, excluding the caller.
---@param caller Object
---@param x number
---@param y number
---@param radius number
---@return Object[]
function World.radar(caller, x, y, radius) end

---Count objects of a kind whose physics bodies overlap a rectangle.
---@param x number
---@param y number
---@param width number
---@param height number
---@param kind string
---@return integer
function World.count(x, y, width, height, kind) end

---Return objects of a kind whose physics bodies overlap a rectangle.
---@param x number
---@param y number
---@param width number
---@param height number
---@param kind string
---@return Object[]
function World.find(x, y, width, height, kind) end

---Available during callbacks of the active stage; nil at runtime otherwise.
---@type World
world = nil

-- Flip

---Lua constants by contract; the table itself is mutable.
---@class Flip
---@field none 0
---@field horizontal 1
---@field vertical 2
---@field both 3

---@type Flip
flip = nil

-- Minimap

---Stage-scoped; do not retain after its stage is destroyed.
---@class Minimap
---@field visible boolean Visibility (read/write).

-- Pool

---Mutable Lua table of handles. Replacing an entry does not destroy or rename
---the corresponding engine resource.
---@class Pool
---@field minimap? Minimap
---@field [string] Object|Sound|Particle|Minimap|nil

---Available during callbacks of the active stage; nil at runtime otherwise.
---@type Pool
pool = nil

-- Steam

---@class Achievement
local Achievement = {}

---Unlock an achievement. Returns false when Steam is unavailable or the call fails.
---@param id string Achievement API name.
---@return boolean
function Achievement:unlock(id) end

---@type Achievement
achievement = nil

---@class Friend
---@field id number Steam ID.
---@field name string Display name.

---@class User
---@field persona string Local display name, or an empty string without Steam.
---@field friends Friend[] Mutable friends snapshot.

---@type User
user = nil

-- Platform

---@class Platform
---@field name string Operating system name.
---@field cores integer Logical CPU count.
---@field memory integer RAM in MiB.

---@type Platform
platform = nil

---Clipboard text (read/write).
---@type string
clipboard = nil

---Ask the operating system to open a URL or URI.
---@param url string
---@return boolean
function openurl(url) end

-- Localization

---Translate a key with the first preferred OS locale and format it with
---`string.format`. Uses the key itself when the locale file does not exist.
---@param key string
---@vararg any
---@return string
function _(key, ...) end

-- Time

---Milliseconds since SDL initialization.
---@return number
function moment() end

-- Timer

---@class TimerHandle
---@field active boolean
local TimerHandle = {}

---Cancel this timer. Idempotent.
---@return TimerHandle self
function TimerHandle:cancel() end

---Pause this timer, preserving its remaining interval.
---@return TimerHandle self
function TimerHandle:pause() end

---Resume this timer.
---@return TimerHandle self
function TimerHandle:resume() end

---Stage-local timers advanced once per frame. Timers freeze while their stage is
---inactive and can only run while that same stage is active. Elapsed intervals
---of repeating timers replay without drift. Timers added by a callback begin on
---the next advance; nested updates are ignored.
---@class Timer
local Timer = {}

---Add a repeating timer.
---@param milliseconds number Positive and finite.
---@param callback fun()
---@return TimerHandle
function Timer:add(milliseconds, callback) end

---Add a timer that fires once.
---@param milliseconds number Positive and finite.
---@param callback fun()
---@return TimerHandle
function Timer:singleshot(milliseconds, callback) end

---Advance timers manually. The engine already calls this once per frame.
---@param delta number Seconds.
function Timer:update(delta) end

---Cancel every timer owned by the current stage.
function Timer:clear() end

---@type Timer
timer = nil

-- Controls (`require("helpers/controls")`)

---Computed input state. Assigning a key creates a Lua-side override.
---@class Controls
---@field left boolean A, D-pad left, or left stick left.
---@field right boolean D, D-pad right, or left stick right.
---@field up boolean W, D-pad up, or left stick up.
---@field down boolean S, D-pad down, or left stick down.
---@field minimap boolean Tab or gamepad Back.
