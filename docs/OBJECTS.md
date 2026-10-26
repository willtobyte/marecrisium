# Objects

Object source files are in `assets/objects`. The generator creates the texture atlas and Lua animation data.

## Create an object

Create an object directory with a `frames` subdirectory:

```text
assets/objects/radio/
├── colliders/
│   ├── 00_100_normal.png
│   ├── 01_250_normal.png
│   └── 00_150_active.png
└── frames/
    ├── 00_100_normal.png
    ├── 01_250_normal.png
    └── 00_150_active.png
```

Use this format for each frame name:

```text
<order>_<duration>_<animation>.png
```

- `order`: Frame position. Start at `00` for each animation.
- `duration`: Frame duration in milliseconds. Each frame can have a different duration. Use `-1` to hold the frame forever. The hold frame must be last.
- `animation`: Animation name. Use `normal` for the initial animation and set `default = "normal"` in `object.lua.j2`. Never name an animation `default`: that key holds the initial-animation marker and a clip with the same name erases it.

## Add mouse colliders

Add a mask in `colliders` only for frames that need mouse collision. Use the frame's file name and image size. Use an RGBA PNG. Make the collider area fully opaque white. Keep the background fully transparent.

The generator adds the smallest rectangle around the opaque area to the frame data as `collider_x, collider_y, collider_width, collider_height`. Coordinates start at the top-left corner of the cropped frame. Frames without masks have no collider data. The generator does not add masks to the atlas or copy them to `cartridge`.

Call `self:collider()` to get the current frame's collider rectangle:

- Returns world `x`, `y`, `width`, and `height` without creating a table.
- Includes frame offsets and scale. Excludes rotation and mirroring.
- Returns zero width and height for a frame without a collider.

The engine does not call mouse handlers. Use the Lua interaction module in the scene `on_loop` to check mouse collisions and call object handlers.

## Add object behavior

Add `object.lua.j2` to the object directory only when it needs behavior. Objects with only animations need no `object.lua.j2`. Use `assets/objects/template.lua.j2` as the base.

For an object that must breathe, extend the breathing trait:

```jinja2
{% extends "traits/breathing.lua.j2" %}
```

## Handle an animation end

Call `on_end` to set an object's animation-end callback. In `object.lua.j2`, use `on_spawn`:

```lua
on_spawn = function(self)
	self:on_end(function(object, animation)
		print("Completed " .. animation)
	end)
end,
```

In a scene, use its object pool:

```lua
on_enter = function()
	pool.objectname:on_end(function(object, animation)
		print("Completed " .. animation)
	end)
end,
```

The callback receives the object and animation name. It runs at the end of each repeating cycle. Each `on_end` call replaces the previous callback. Call `on_end(nil)` to remove the callback.

## Call an object handler

When the engine loads an object kind, it also sets each `on_<name>` field of `object.lua.j2` as `<name>`. In `object.lua.j2`:

```lua
on_hit = function(self, damage)
	self.animation = "hurt"
end,
```

In a scene or another object:

```lua
pool.objectname:hit(10)
```

The call has the same cost as a call to `on_hit`. A `<name>` field in `object.lua.j2` is replaced. Only fields that exist at load get a second name. Top-level keys of `object.lua.j2` must be strings.

## Generate the objects

Run the generator from the repository root:

```shell
uv run assets/tools/objects.py
```

The command generates all objects with a `frames` directory on each run. It writes these files for each object:

```text
cartridge/blobs/objects/<name>.png
cartridge/objects/<name>.lua
```

Do not edit these generated files.

## Update an object

1. Add, replace, rename, or remove source files in the object's `frames` directory.
2. Apply the same changes to each frame's mask in `colliders`, if present.
3. Update `object.lua.j2` when behavior changes.
4. Run the generator again to update `cartridge`.
