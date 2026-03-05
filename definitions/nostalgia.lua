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
