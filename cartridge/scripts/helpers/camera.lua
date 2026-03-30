local max = math.max
local min = math.min

local camera = {}

local cx, cy = 0, 0

local config = {
	speed = 5,
	offset_x = 0,
	offset_y = 0,
	bounds_min_x = nil,
	bounds_min_y = nil,
	bounds_max_x = nil,
	bounds_max_y = nil,
}

function camera.configure(options)
	for key, val in pairs(options) do
		config[key] = val
	end
end

function camera.reset(x, y)
	cx = x or 0
	cy = y or 0
end

function camera.set_bounds(min_x, min_y, max_x, max_y)
	config.bounds_min_x = min_x
	config.bounds_min_y = min_y
	config.bounds_max_x = max_x
	config.bounds_max_y = max_y
end

local function clamp(x, y)
	if config.bounds_min_x then
		x = max(config.bounds_min_x, min(config.bounds_max_x, x))
		y = max(config.bounds_min_y, min(config.bounds_max_y, y))
	end
	return x, y
end

function camera.snap(target)
	local target_x = target.x + config.offset_x - viewport.width * 0.5
	local target_y = target.y + config.offset_y - viewport.height * 0.5
	cx, cy = clamp(target_x, target_y)
end

function camera.update(target, delta)
	delta = delta or (1 / 60)

	local target_x = target.x + config.offset_x - viewport.width * 0.5
	local target_y = target.y + config.offset_y - viewport.height * 0.5
	target_x, target_y = clamp(target_x, target_y)

	local t = min(1, config.speed * delta)
	cx = cx + (target_x - cx) * t
	cy = cy + (target_y - cy) * t
	cx, cy = clamp(cx, cy)
end

function camera.position()
	return cx, cy
end

return camera
