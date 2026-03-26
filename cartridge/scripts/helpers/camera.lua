local abs = math.abs
local max = math.max
local min = math.min

local camera = {}

local state = {
	x = 0,
	y = 0,
	vx = 0,
	vy = 0,
}

local config = {
	dead_zone_x = 16,
	dead_zone_y = 16,
	lookahead_x = 0,
	lookahead_y = 0,
	smoothing = 5,
	damping = 0.85,
	snap_threshold = 0.5,
	offset_x = 0,
	offset_y = 0,
	bounds_min_x = nil,
	bounds_min_y = nil,
	bounds_max_x = nil,
	bounds_max_y = nil,
}

local stiffness = config.smoothing * config.smoothing
local damping_coefficient = 2 * config.smoothing * config.damping

function camera.configure(options)
	for key, val in pairs(options) do
		config[key] = val
	end
	stiffness = config.smoothing * config.smoothing
	damping_coefficient = 2 * config.smoothing * config.damping
end

function camera.reset(x, y)
	state.x = x or 0
	state.y = y or 0
	state.vx = 0
	state.vy = 0
end

function camera.set_bounds(min_x, min_y, max_x, max_y)
	config.bounds_min_x = min_x
	config.bounds_min_y = min_y
	config.bounds_max_x = max_x
	config.bounds_max_y = max_y
end

function camera.snap(target)
	local target_x = target.x + config.offset_x + config.lookahead_x - viewport.width * 0.5
	local target_y = target.y + config.offset_y + config.lookahead_y - viewport.height * 0.5

	local bounds_x_min, bounds_y_min, bounds_x_max, bounds_y_max =
		config.bounds_min_x, config.bounds_min_y, config.bounds_max_x, config.bounds_max_y
	if bounds_x_min then
		target_x = max(bounds_x_min, min(bounds_x_max, target_x))
		target_y = max(bounds_y_min, min(bounds_y_max, target_y))
	end

	state.x = target_x
	state.y = target_y
	state.vx = 0
	state.vy = 0
end

function camera.update(target, delta)
	delta = delta or (1 / 60)

	local target_x = target.x + config.offset_x + config.lookahead_x - viewport.width * 0.5
	local target_y = target.y + config.offset_y + config.lookahead_y - viewport.height * 0.5

	local bounds_x_min, bounds_y_min, bounds_x_max, bounds_y_max =
		config.bounds_min_x, config.bounds_min_y, config.bounds_max_x, config.bounds_max_y
	if bounds_x_min then
		target_x = max(bounds_x_min, min(bounds_x_max, target_x))
		target_y = max(bounds_y_min, min(bounds_y_max, target_y))
	end

	local delta_x = target_x - state.x
	local delta_y = target_y - state.y

	if abs(delta_x) < config.dead_zone_x then
		delta_x = 0
	end
	if abs(delta_y) < config.dead_zone_y then
		delta_y = 0
	end

	state.vx = state.vx + (delta_x * stiffness - state.vx * damping_coefficient) * delta
	state.vy = state.vy + (delta_y * stiffness - state.vy * damping_coefficient) * delta

	state.x = state.x + state.vx * delta
	state.y = state.y + state.vy * delta

	if bounds_x_min then
		if state.x <= bounds_x_min or state.x >= bounds_x_max then
			state.vx = 0
		end
		if state.y <= bounds_y_min or state.y >= bounds_y_max then
			state.vy = 0
		end
		state.x = max(bounds_x_min, min(bounds_x_max, state.x))
		state.y = max(bounds_y_min, min(bounds_y_max, state.y))
	end

	local snap = config.snap_threshold
	if abs(state.vx) < snap and delta_x == 0 then
		state.vx = 0
	end
	if abs(state.vy) < snap and delta_y == 0 then
		state.vy = 0
	end
end

function camera.position()
	return state.x, state.y
end

return camera
