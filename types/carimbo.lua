---@meta

---@alias Vector2 [number, number]
---@alias ParticleRange [number, number]
---@alias AnimationFrame [number, number, number, number, number, number, number, number, number]|[number, number, number, number, number, number, number, number, number, number, number, number, number]
---@alias MouseButton "left"|"middle"|"right"

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
---@field xy fun(): number, number Returns world x and y. Does not create a table.
---@field snapshot fun(): number, number, boolean, boolean Returns world x, y, and left and right button states. Does not create a table.
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
---Dense sequences are stored as arrays; sparse and mixed tables preserve every key.
---Nested writes persist. Each root reuses live nested proxies. Proxies support `#`, `pairs`, and `ipairs`.
---@class Cassette
---@field [string] CassetteValue|nil
local Cassette = {}

---Delete all saved keys. Never store a `clear` key, the `clear` key is reserved.
function Cassette:clear() end

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
---@field on_begin fun() Runs once after engine initialization.

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

-- Scene (`scenes/<name>.lua`)

---@class SceneObject
---@field name string Pool name.
---@field kind string Loads `objects/<kind>.lua` and `blobs/objects/<kind>.png`.
---@field x? number Defaults to 0.
---@field y? number Defaults to 0.

---@class SceneSound
---@field name string Pool name. The decoded data is shared. Playback state belongs to the scene.

---Scene definition and callbacks owned by the script.
---@class Scene
---@field objects? SceneObject[] Declaration order sets each object's initial `z`.
---@field sounds? SceneSound[] At most 16 sounds.
---@field on_enter? fun(self: Scene) Called when this scene becomes active.
---@field on_leave? fun(self: Scene) Called before this scene becomes inactive.
---@field on_loop? fun(self: Scene, delta: number) Called every active frame; delta is in seconds.

-- Director

---@class Director
local Director = {}

---Queue navigation to an enrolled scene.
---@param name string Scene name.
function Director.navigate(name) end

---Destroy a cached scene. The scene must not be active or pending.
---@param name string Scene name.
function Director.destroy(name) end

---Create and cache a scene without navigation.
---The scene name must not be enrolled.
---@param name string Scene name.
function Director.enroll(name) end

---@type Director
director = nil

-- Overlay (`overlays/<scene-name>.lua`)

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

---@class Label
local Label = {}

---Create a windowed text label. The text never leaks the window; words wrap
---without splitting, overlong words split mid-word, and lines beyond the height are cut.
---@param font string Font family name from the overlay `fonts` list.
---@param x number
---@param y number
---@param w number
---@param h number
---@return Label
function Label.new(font, x, y, w, h) end

---Draw up to 256 bytes inside the window. Auto breaks do not consume an effect index.
---Call only inside `on_paint`.
---@param text string
---@param effects? table<integer, GlyphEffect> Indices must be from 1 to 256.
function Label:draw(text, effects) end

---Script-owned callbacks and custom state.
---@class Overlay
---@field fonts? string[] Font families loaded into `pool` by family name.
---@field on_appear? fun(self: Overlay) Called when the overlay becomes active.
---@field on_disappear? fun(self: Overlay) Called when the overlay becomes inactive.
---@field on_loop? fun(self: Overlay, delta: number) Called each frame before painting; delta is in seconds.
---@field on_paint? fun(self: Overlay) Called each frame.
---@field [string] any Script fields.
local Overlay = {}

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
---@field default? string Initial sequence. Without it, the first parsed sequence is used.
---@field [string] AnimationClip|string

---Non-empty frame array.
---Frame files use `index_duration_[once_]animation.png`. Duration `-1` holds the frame forever.
---@class AnimationClip
---@field loop? boolean Repeats by default. `false` stops on the last frame.
---@field [integer] AnimationFrame `{atlas_x, atlas_y, width, height, offset_x, offset_y, source_width, source_height, duration_ms[, collider_x, collider_y, collider_width, collider_height]}`. `duration_ms` is `-1` for a hold frame.

---Spawn configuration and shared custom behavior. The engine dispatches the
---reserved callbacks from references cached at load time. A write to a reserved
---callback through `Object` does not change the engine dispatch.
---@class ObjectPrototype
---@field animation? AnimationConfig Spawn-only animation definitions.
---@field on_spawn? fun(self: Object) Called once after the object is complete and available in `pool`.
---@field on_loop? fun(self: Object, delta: number) Called every active frame; delta is in seconds.
---@field [string] any Custom fields and methods shared by every object of this kind.

---Each object stores custom writes independently and reads missing fields from
---the shared prototype.
---@class Object
---@field x number Transform X (read/write).
---@field y number Transform Y (read/write).
---@field scale number Scale from the sprite center (read/write).
---@field angle number Rotation in degrees (read/write).
---@field alpha number Opacity, clamped to 0-255 (read/write).
---@field shown boolean Visibility (read/write).
---@field mirror 0|1|2|3 Render mirroring (read/write).
---@field name string Instance name (read-only).
---@field kind string Prototype kind (read-only).
---@field z integer Render order (read/write); defaults to the declaration index in `objects` and higher values draw on top.
---@field animation string Active animation name (read/write); assigning a sequence name switches to it from the first frame.
---@field [string] any Per-object field or dispatched prototype method.
local Object = {}

---@return number x World X of the current collider. Includes frame offsets and scale. Does not apply rotation or mirroring.
---@return number y World Y of the current collider. Includes frame offsets and scale. Does not apply rotation or mirroring.
---@return number width Scaled collider width. Zero when the frame has no collider.
---@return number height Scaled collider height. Zero when the frame has no collider.
function Object:collider() end

---Set the callback for each animation end.
---@param callback fun(self: Object, animation: string)
function Object:on_end(callback) end

-- Sound

---Scene-owned playback instance. The engine stops it after the scene's
---`on_leave` callback. Do not retain it after the scene is destroyed.
---@class Sound
---@field volume number Gain, clamped to 0.0-1.0 (read/write).
---@field pan number Stereo pan, clamped to -1.0-1.0 (read/write).
---@field playing boolean Playback state (read-only).
local Sound = {}

---Restart playback.
function Sound:play() end

---Stop playback.
function Sound:stop() end

---Set the callback for each natural playback end. Call this once per sound.
---`stop` and scene changes do not call it.
---@param callback fun(self: Sound)
function Sound:on_end(callback) end

---Fade the volume.
---@param from number Start gain; a negative value uses the current gain.
---@param to number Target gain.
---@param ms integer Duration in milliseconds; negative values become 0.
function Sound:fade(from, to, ms) end

-- Particle emitter

---Scene-scoped; do not retain after its scene is destroyed.
---@class ParticleEmitter
---@field x number Emitter X (read/write).
---@field y number Emitter Y (read/write).
---@field active boolean Whether dead particles respawn (read/write).

-- Mirror

---Lua constants by contract; the table itself is mutable.
---@class Mirror
---@field none 0
---@field horizontal 1
---@field vertical 2
---@field both 3

---@type Mirror
mirror = nil

-- Pool

---Mutable table shared by the active scene, its objects, and its overlay.
---Replacing a resource entry does not destroy or rename the engine resource.
---@class Pool
---@field [string] any

---Available during callbacks of the active scene, its objects, and its overlay.
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
---@field id integer Steam account ID, the low 32 bits of the SteamID64.
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

---The scene owns its timers. Timers are checked once per frame and pause with the scene.
---Late repeating timers run once and restart. Handle collection does not cancel timers. Handles expire with the scene.
---@class Timer
local Timer = {}

---Add a repeating timer.
---@param milliseconds number Must be positive and finite.
---@param callback fun()
---@return TimerHandle
function Timer:add(milliseconds, callback) end

---Add a timer that fires once.
---@param milliseconds number Must be positive and finite.
---@param callback fun()
---@return TimerHandle
function Timer:singleshot(milliseconds, callback) end

---Cancel every timer owned by the current scene.
function Timer:clear() end

---@type Timer
timer = nil

-- Controls (`require("helpers/controls")`)

---Computed input state. Assigning a key creates a Lua-side override.
---@class Controls
---@field left boolean A, left arrow, D-pad left, or left stick left.
---@field right boolean D, right arrow, D-pad right, or left stick right.
---@field up boolean W, up arrow, D-pad up, or left stick up.
---@field down boolean S, down arrow, D-pad down, or left stick down.
---@field action boolean Space or the south gamepad button.
---@field dismiss boolean Escape or the east gamepad button.
