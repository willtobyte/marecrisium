local floor = math.floor
local min = math.min

local camera = {}

local cx, cy = 0, 0

local config = {
	speed = 3,
	offset_x = 0,
	offset_y = 0,
}

function camera.snap(target)
	cx = target.x + config.offset_x - viewport.width * 0.5
	cy = target.y + config.offset_y - viewport.height * 0.5
end

function camera.update(target, delta)
	delta = delta or (1 / 60)

	local target_x = target.x + config.offset_x - viewport.width * 0.5
	local target_y = target.y + config.offset_y - viewport.height * 0.5

	local t = min(1, config.speed * delta)
	cx = cx + (target_x - cx) * t
	cy = cy + (target_y - cy) * t
end

function camera.position()
	return floor(cx), floor(cy)
end

return camera
