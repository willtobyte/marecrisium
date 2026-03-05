---@meta

-- Nostalgia Engine API definitions
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
---@field on_begin fun() Called once after the engine is fully initialized.

--------------------------------------------------------------------------------
-- Stage (scripts returned by stages/<name>.lua)
--------------------------------------------------------------------------------

---@class Stage
---A stage script (`stages/<name>.lua`) returns a table that may contain
---these lifecycle callbacks plus an `objects` table defining the stage's entities.
local Stage = {}

---Called when the director navigates to this stage.
function Stage.on_enter() end

---Called when the director navigates away from this stage.
function Stage.on_leave() end

---Called every frame while this stage is active.
---@param self table The stage table itself.
---@param delta number Frame delta time in seconds.
function Stage.on_loop(self, delta) end

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

---Global stage director.
---@type Director
director = {}

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
-- Object (entity userdata, available as `self` and in `pool`)
--------------------------------------------------------------------------------

---@class Object
---@field x number Transform X position.
---@field y number Transform Y position.
---@field scale number Transform scale factor.
---@field angle number Rotation angle in degrees.
---@field alpha number Opacity (0-255).
---@field shown boolean Whether the object is visible.
---@field kind string The kind/type string of this object (read-only).
---@field animation string|nil Currently playing animation clip name. Assign to switch clips.
---@field [string] any Custom properties from the prototype table.
local Object = {}

---Called once when the object is created during stage construction.
---@param self Object
function Object.on_spawn(self) end

---Called every frame. Use for movement, AI, input handling, etc.
---@param self Object
---@param delta number Frame delta time in seconds.
function Object.on_loop(self, delta) end

---Called when a physics sensor overlap begins with another object.
---@param self Object
---@param other_name string Name of the other object.
---@param other_kind string Kind/type of the other object.
function Object.on_collision_begin(self, other_name, other_kind) end

---Called when a physics sensor overlap ends with another object.
---@param self Object
---@param other_name string Name of the other object.
---@param other_kind string Kind/type of the other object.
function Object.on_collision_end(self, other_name, other_kind) end

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
