# Objects

Object source files are in `assets/objects`.

The generator creates the texture atlas and the Lua animation data.

## Create an object

Create a directory for the object. Add a `frames` directory inside it.

```text
assets/objects/radio/
├── colliders/
│   ├── 00_100_default.png
│   ├── 01_250_default.png
│   └── 00_150_active.png
└── frames/
    ├── 00_100_default.png
    ├── 01_250_default.png
    └── 00_150_active.png
```

Use this format for each frame name:

```text
<order>_<duration>_<animation>.png
```

- `order` is the frame position in the animation.
- `duration` is the frame duration in milliseconds.
- `animation` is the animation name.

Use `default` as the initial animation name. Start the order at `00` for each animation. Each frame can have a different duration.

Add one collider mask for each frame. Use the same file name and image size. Draw an opaque area where the collider must exist. Keep all other pixels transparent.

The collider PNG must use RGBA with a fully opaque white collider and a fully transparent background.

The generator writes the smallest rectangle that contains the opaque area to the Lua animation data. It does not add collider masks to the atlas or copy them to `cartridge`.

## Add object behavior

The object does not need a patch if it only has animations.

Add `patch.lua.j2` to the object directory when the object needs behavior. Extend the breathing trait when the selected object must breathe:

```jinja2
{% extends "traits/breathing.lua.j2" %}
```

Use `assets/objects/template.lua.j2` as the base for other object behavior.

## Handle an animation end

Call `on_end` on an object to set its animation-end callback. An object patch can set the callback in `on_spawn`:

```lua
on_spawn = function(self)
	self:on_end(function(object, animation)
		print("Completed " .. animation)
	end)
end,
```

A scene can also set the callback through its object pool:

```lua
on_enter = function()
	pool.objectname:on_end(function(object, animation)
		print("Completed " .. animation)
	end)
end,
```

The callback receives the object and the animation name. It runs at the end of each cycle for a repeating animation. It runs one time for a non-repeating animation.

A new call to `on_end` replaces the callback that was set before.

## Generate the objects

Run the generator from the repository root:

```shell
uv run assets/objects/generate.py
```

The command generates all objects that contain a `frames` directory. It writes these files for each object:

```text
cartridge/blobs/objects/<name>.png
cartridge/objects/<name>.lua
```

Do not edit files in `cartridge/objects` or `cartridge/blobs/objects`. Change the source frames or `patch.lua.j2`. Run the generator again.

## Update an object

Add, replace, rename, or remove files in the object `frames` directory. Make the same change in the `colliders` directory. Update `patch.lua.j2` when the behavior changes. Run the generator again to update the files in `cartridge`.
