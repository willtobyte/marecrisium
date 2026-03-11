---@meta

-- Carimbo Engine API definitions
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
---@field x number Mouse X position in render coordinates (read-only).
---@field y number Mouse Y position in render coordinates (read-only).
---@field xy number, number Mouse X and Y as two return values (read-only). Usage: `local x, y = mouse.xy`
---@field button integer Currently pressed button: 1=left, 2=middle, 3=right, 0=none (read-only).
---@field shown boolean Whether the cursor is visible (read/write).

---Global mouse state.
---@type Mouse
mouse = {}

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

---Global gamepad state.
---@type Gamepad
gamepad = {}

--------------------------------------------------------------------------------
-- Cassette (persistent key-value store)
--------------------------------------------------------------------------------

---@class Cassette
---Persistent save-data store. Read/write arbitrary keys.
---
---Supported value types: `boolean`, `number`, `string`.
---Assign `nil` to delete a key. All writes persist immediately.
---
---Usage:
---```lua
---cassette.score = 100
---local s = cassette.score
---cassette.score = nil -- delete
---cassette:clear()     -- delete all
---```
---@field [string] boolean|number|string|nil
local Cassette = {}

---Clear all saved data.
function Cassette:clear() end

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
---@field scale number Render scale factor.
---@field fullscreen boolean Whether to start in fullscreen mode.
---@field ticks integer|nil Fixed tick rate (ticks per second). Default is 0 (disabled). Set to e.g. 10 for 10 ticks/second.
---@field sentry? string Sentry DSN for crash reporting. Only active in non-debug builds. Omit or set empty to disable.
---@field on_begin fun() Called once after the engine is fully initialized.

--------------------------------------------------------------------------------
-- Stage (scripts returned by stages/<name>.lua)
--------------------------------------------------------------------------------

---@class StageObject
---@field name string Unique instance name for this object. Accessible via `pool.<name>`.
---@field kind string Object type. Maps to `objects/<kind>.lua` (prototype) and `blobs/objects/<kind>.png` (spritesheet).

---@class Stage
---A stage script (`stages/<name>.lua`) returns a table that may contain
---these fields, lifecycle callbacks, and entity/sound declarations.
---@field gravity number[]|nil World gravity as {gx, gy}. Default is {0, 0} (no gravity). Set to e.g. {0, 980} for a platformer.
---@field objects StageObject[]|nil Objects to spawn when the stage is created.
---@field sounds string[]|nil Sound names to preload. Each `"foo"` loads `sounds/foo` and is accessible as `pool.foo`.
---@field tilemap string|nil Tilemap name. Loads `tilemaps/<name>.lua` and exposes a `tilemap` global in the stage environment.
local Stage = {}

---Called when the director navigates to this stage.
function Stage.on_enter() end

---Called when the director navigates away from this stage.
function Stage.on_leave() end

---Called at fixed tick rate while this stage is active.
---Only fires when `ticks` is set in MainConfig.
---@param self table The stage table itself.
---@param tick number Monotonically increasing tick counter (starts at 1).
function Stage.on_tick(self, tick) end

---Called every frame while this stage is active.
---@param self table The stage table itself.
---@param delta number Frame delta time in seconds.
function Stage.on_loop(self, delta) end

---Called when a click occurs but no collidable object is hit.
---@param x number Click X position in world coordinates.
---@param y number Click Y position in world coordinates.
---@param button "left"|"middle"|"right" Which mouse button was released.
function Stage.on_click(x, y, button) end

---Called every frame before rendering.
---Return camera_x, camera_y to offset object and tilemap rendering.
---The engine automatically applies the returned camera to the tilemap.
---@param self table The stage table itself.
---@return number|nil camera_x Camera X offset for rendering.
---@return number|nil camera_y Camera Y offset for rendering.
function Stage.on_paint(self) end

--------------------------------------------------------------------------------
-- Director (stage navigation)
--------------------------------------------------------------------------------

---@class Director
local Director = {}

---Navigate to a named stage. Creates it if not preloaded.
---Triggers `on_leave` on the current stage.
---@param name string Stage name (matches `stages/<name>.lua`).
function Director.navigate(name) end

---Destroy a named stage. Cannot destroy the current stage.
---@param name string Stage name.
function Director.destroy(name) end

---Preload a stage without navigating to it.
---@param name string Stage name.
function Director.preload(name) end

---Destroy all stages and clear all resource pools.
function Director.flush() end

---Activate an overlay. Creates it if not loaded. Pass nil to deactivate.
---@param name string|nil Overlay name (matches `overlays/<name>.lua`).
function Director.overlay(name) end

---Global stage director.
---@type Director
director = {}

--------------------------------------------------------------------------------
-- Overlay (HUD / on-screen text)
--------------------------------------------------------------------------------

---@class OverlayConfig
---@field fonts string[] Font families to preload from `overlay/fonts/`.

---Per-glyph visual effect applied to individual characters in a label.
---Each field is optional; omitted fields use their default (no effect).
---Newline characters do not count as glyphs for indexing purposes.
---
---Usage:
---```lua
---overlay:label("pixel", "hello", 10, 20, {
---  [1] = { r = 1, g = 0, b = 0 },           -- 'h' in red
---  [3] = { yoffset = -2, scale = 1.5 },      -- first 'l' raised and scaled
---  [5] = { alpha = 0.5 },                     -- 'o' semi-transparent
---})
---```
---@class GlyphEffect
---@field xoffset? number Horizontal pixel offset for this glyph. Default 0.
---@field yoffset? number Vertical pixel offset for this glyph. Default 0.
---@field scale? number Scale factor for this glyph. Default 1.
---@field angle? number Rotation angle in degrees for this glyph, around its center. Default 0.
---@field r? number Red channel for this glyph (0.0-1.0). Default 1.
---@field g? number Green channel for this glyph (0.0-1.0). Default 1.
---@field b? number Blue channel for this glyph (0.0-1.0). Default 1.
---@field alpha? number Opacity for this glyph (0.0-1.0). Default 1.

---@class Overlay
local Overlay = {}

---Called every frame while this overlay is active (logic only, no rendering).
---@param self Overlay The overlay table itself.
---@param delta number Frame delta time in seconds.
function Overlay.on_loop(self, delta) end

---Called every frame to render overlay elements. Use overlay:label() here.
---@param self Overlay The overlay table itself.
function Overlay.on_paint(self) end

---Draw a text label at the given position using a preloaded font.
---Optionally accepts a table of per-glyph effects keyed by 1-based visible
---character index (newlines are not counted). Each entry is a GlyphEffect
---table that overrides position, scale, and color for that specific glyph.
---Indices without an entry render normally (white, no offset, scale 1).
---@param font string Font family name (must be declared in the overlay's fonts list).
---@param text string The text to render.
---@param x number X position in logical pixels.
---@param y number Y position in logical pixels.
---@param effects? table<integer, GlyphEffect> Per-glyph effects keyed by 1-based visible character index.
function Overlay:label(font, text, x, y, effects) end

---Global overlay instance (nil when no overlay is active).
---@type Overlay|nil
overlay = nil

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
---@field cullable? boolean Whether this object can become dormant when off-screen. Only applies to non-dynamic bodies. Default is false.
---@field animation table<string, number[][]> Animation clips. Each clip is an array of frames: {sx, sy, sw, sh, duration_ms [, cx, cy, cw, ch]}.
---@field on_spawn? fun(self: Object) Called once when the object is created.
---@field on_loop? fun(self: Object, delta: number) Called every frame.
---@field on_collision_begin? fun(self: Object, name: string, kind: string, normal_x?: number, normal_y?: number) Called on physics contact begin. `name` and `kind` refer to the other object involved in the collision.
---@field on_collision_end? fun(self: Object, name: string, kind: string) Called on physics contact end. `name` and `kind` refer to the other object involved in the collision.
---@field on_screen_exit? fun(self: Object, direction: "left"|"right"|"top"|"bottom") Called when the object moves fully outside a screen edge.
---@field on_screen_enter? fun(self: Object, direction: "left"|"right"|"top"|"bottom") Called when the object returns inside a screen edge.
---@field on_animation_end? fun(self: Object, clip_name: string) Called when an animation clip finishes or is replaced.
---@field on_animation_begin? fun(self: Object, clip_name: string) Called when a new animation clip starts playing.
---@field on_click? fun(self: Object, x: number, y: number, button: "left"|"middle"|"right") Called when the object is clicked.
---@field on_hover? fun(self: Object) Called when the mouse cursor enters the object's hitbox.
---@field on_unhover? fun(self: Object) Called when the mouse cursor leaves the object's hitbox.
---@field on_sleep? fun(self: Object) Called when the object becomes dormant (off-screen).
---@field on_wake? fun(self: Object) Called when the object wakes from dormancy.
---@field [string] any Custom properties accessible via the object instance.

--------------------------------------------------------------------------------
-- Object (entity userdata, available as `self` and in `pool`)
--------------------------------------------------------------------------------

---@class Object
---@field x number Transform X position. For dynamic and static bodies, setting this teleports the body.
---@field y number Transform Y position. For dynamic and static bodies, setting this teleports the body.
---@field position number[] Write-only. Set position as {x, y} with a single body teleport. More efficient than setting x and y separately.
---@field vx number Linear velocity X component. Readable on all body types; writable only on dynamic bodies.
---@field vy number Linear velocity Y component. Readable on all body types; writable only on dynamic bodies.
---@field scale number Transform scale factor.
---@field angle number Rotation angle in degrees.
---@field alpha number Opacity (0-255).
---@field shown boolean Whether the object is visible.
---@field flip "none"|"horizontal"|"vertical"|"both" Flip mode for rendering.
---@field name string The object's name (read-only).
---@field kind string The kind/type string of this object (read-only).
---@field alive boolean Whether the object is still alive (read-only).
---@field cullable boolean Whether this object participates in off-screen dormancy (read-only, set via prototype).
---@field dormant boolean Whether this object is currently dormant/sleeping because it is off-screen (read-only).
---@field grounded boolean Whether this dynamic body is touching a surface below it (read-only). Always false for non-dynamic bodies.
---@field riding string|nil Name of the kinematic object this dynamic body is standing on (read-only). Nil when not riding anything.
---@field animation string|nil Currently playing animation clip name. Assign to switch clips.
---@field [string] any Custom properties from the prototype table.
local Object = {}

---Destroy this object. After calling, `alive` returns false and all property access returns nil.
function Object:die() end

---Called once when the object is created during stage construction.
---@param self Object
function Object.on_spawn(self) end

---Called every frame. Use for movement, AI, input handling, etc.
---@param self Object
---@param delta number Frame delta time in seconds.
function Object.on_loop(self, delta) end

---Called when a physics contact begins with another object.
---For contact events, the collision normal is provided. The normal points from self toward the other object.
---A normal_y > 0.5 typically indicates the other object is below (ground contact).
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

---Called when the object moves fully outside a screen edge.
---@param self Object
---@param direction "left"|"right"|"top"|"bottom" Which edge was crossed.
function Object.on_screen_exit(self, direction) end

---Called when the object returns inside a screen edge it had previously exited.
---@param self Object
---@param direction "left"|"right"|"top"|"bottom" Which edge was crossed.
function Object.on_screen_enter(self, direction) end

---Called when an animation clip finishes (loops or is replaced).
---@param self Object
---@param clip_name string Name of the clip that ended.
function Object.on_animation_end(self, clip_name) end

---Called when a new animation clip starts playing.
---@param self Object
---@param clip_name string Name of the clip that started.
function Object.on_animation_begin(self, clip_name) end

---Called when the object is clicked (mouse button released over its hitbox).
---Only triggered for collidable objects. The topmost object receives the click.
---@param self Object
---@param x number Click X position in world coordinates.
---@param y number Click Y position in world coordinates.
---@param button "left"|"middle"|"right" Which mouse button was released.
function Object.on_click(self, x, y, button) end

---Called when the mouse cursor enters the object's hitbox.
---Only triggered for collidable objects.
---@param self Object
function Object.on_hover(self) end

---Called when the mouse cursor leaves the object's hitbox.
---Only triggered for collidable objects.
---@param self Object
function Object.on_unhover(self) end

---Called when the object goes dormant (fully off-screen with margin).
---Only triggered for objects with `cullable = true` in their prototype.
---While dormant, the object receives no updates, physics, rendering, or other callbacks.
---@param self Object
function Object.on_sleep(self) end

---Called when the object wakes up (returns to screen after being dormant).
---Only triggered for objects with `cullable = true` in their prototype.
---@param self Object
function Object.on_wake(self) end

--------------------------------------------------------------------------------
-- Sound (audio handle userdata, available in `pool`)
--------------------------------------------------------------------------------

---@class Sound
---@field volume number Current volume, 0.0 to 1.0.
---@field loop boolean Whether looping is enabled.
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
-- World (physics, available per-stage)
--------------------------------------------------------------------------------

---@class World
local World = {}

---Cast a ray and return all hit objects sorted by distance.
---@param caller Object The object casting the ray (excluded from results).
---@param x number Ray origin X.
---@param y number Ray origin Y.
---@param angle number Ray angle in degrees.
---@param distance number Maximum ray distance.
---@return Object[] hits Array of hit objects.
function World.raycast(caller, x, y, angle, distance) end

---Physics world (available inside stage scripts).
---@type World
world = {}

--------------------------------------------------------------------------------
-- Pool (named collection of objects and sounds, available per-stage)
--------------------------------------------------------------------------------

---@class Pool
---Access objects and sounds by name.
---@field [string] Object|Sound

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
---@field locale string Preferred locale, e.g. "pt-BR" or "en" (read-only).
---@field clipboard string System clipboard text (read/write).

---Global platform information.
---@type Platform
platform = {}

--------------------------------------------------------------------------------
-- Open URL
--------------------------------------------------------------------------------

---Open a URL in the system's default browser or handler.
---@param url string The URL to open.
---@return boolean success Whether the operation succeeded.
function openurl(url) end

--------------------------------------------------------------------------------
-- WebSocket (WebSocket connection)
--------------------------------------------------------------------------------

---@class WebSocket
---A WebSocket connection. Supports pub/sub messaging with JSON payloads.
local WebSocket = {}

---Subscribe to a topic. The callback receives decoded JSON data as a Lua table
---whenever a message arrives on this topic.
---@param topic string The topic name to subscribe to.
---@param callback fun(data: table) Called with the decoded message data.
---@return Subscription subscription Handle to publish or unsubscribe.
function WebSocket:subscribe(topic, callback) end

---@class Subscription
---A subscription to a topic on a WebSocket connection.
---Automatically unsubscribes on garbage collection.
local Subscription = {}

---Publish a table as JSON to this subscription's topic.
---@param data table The data to send (encoded as JSON).
function Subscription:publish(data) end

---Unsubscribe from this topic. Also called automatically on garbage collection.
function Subscription:unsubscribe() end

---The topic name of this subscription (read-only).
---@type string
Subscription.topic = ""

---Create a WebSocket connection to the given URL.
---Only one connection can exist at a time; calling again replaces the previous one.
---@param url string The WebSocket URL (wss:// or ws://).
---@return WebSocket
function WebSocket.new(url) end

--------------------------------------------------------------------------------
-- Moment (time)
--------------------------------------------------------------------------------

---Returns the current time in milliseconds since engine initialization.
---Useful for absolute timestamps, countdowns, and sub-tick timing.
---@return number milliseconds
function moment() end

--------------------------------------------------------------------------------
-- Ticker (Lua-side tick-based timer system)
--------------------------------------------------------------------------------

---@class Ticker
---Pure Lua timer system driven by the engine's fixed tick rate.
---Require via `require("helpers/ticker")`.
---
---Usage:
---```lua
---local ticker = require("helpers/ticker")
---
----- One-shot: fire after 30 ticks
---ticker.after(30, function() print("done") end)
---
----- Repeating: fire every 10 ticks
---local timer = ticker.every(10, function() print("tick") end)
---ticker.cancel(timer)
---
----- Wrap a stage to auto-advance and auto-clear on leave
---ticker.wrap(stage)
---```
local Ticker = {}

---Schedule a one-shot callback after a number of ticks.
---@param ticks integer Number of ticks to wait.
---@param callback fun() Function to call when ticks elapse.
---@return table timer Timer handle for cancellation.
function Ticker.after(ticks, callback) end

---Schedule a repeating callback every N ticks.
---@param ticks integer Interval in ticks.
---@param callback fun() Function to call each interval.
---@return table timer Timer handle for cancellation.
function Ticker.every(ticks, callback) end

---Cancel a timer by its handle.
---@param timer table The handle returned by `after` or `every`.
function Ticker.cancel(timer) end

---Cancel all active timers.
function Ticker.clear() end

---Decorate a stage table to auto-advance timers on `on_tick`
---and auto-clear on `on_leave`. Chains with existing callbacks.
---@param stage Stage The stage table to wrap.
---@return Stage stage The same table, modified in place.
function Ticker.wrap(stage) end

--------------------------------------------------------------------------------
-- Controls (unified keyboard + gamepad abstraction)
--------------------------------------------------------------------------------

---@class Controls
---Unified input abstraction that merges keyboard and gamepad into
---semantic game actions. Require via `require("helpers/controls")`.
---
---Directional inputs combine arrow keys, d-pad, and left stick.
---Action buttons combine keyboard keys and gamepad face buttons.
---
---Usage:
---```lua
---local controls = require("helpers/controls")
---if controls.left then ... end
---if controls.jump then ... end
---```
---@field left boolean Arrow left, d-pad left, or left stick left.
---@field right boolean Arrow right, d-pad right, or left stick right.
---@field up boolean Arrow up, d-pad up, or left stick up.
---@field down boolean Arrow down, d-pad down, or left stick down.
---@field jump boolean Keyboard Space or gamepad south (A / Cross).
---@field attack boolean Keyboard Z or gamepad west (X / Square).
---@field start boolean Keyboard Enter or gamepad Start.
local Controls = {}
