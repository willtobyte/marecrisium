local abs = math.abs
local max = math.max
local min = math.min
local viewport = viewport

local camera = {}

local state = {
	x = 0,
	y = 0,
	velocity_x = 0,
	velocity_y = 0,
}

local config = {
	dead_zone_x = 16,
	dead_zone_y = 16,
	lookahead_x = 0,
	lookahead_y = 0,
	smoothing = 5,
	damping = 0.85,
	snap_threshold = 0.5,
	bounds = nil,
	offset_x = 0,
	offset_y = 0,
}

function camera.configure(options)
	for k, v in pairs(options) do
		config[k] = v
	end
end

function camera.reset(x, y)
	state.x = x or 0
	state.y = y or 0
	state.velocity_x = 0
	state.velocity_y = 0
end

function camera.set_bounds(min_x, min_y, max_x, max_y)
	config.bounds = { min_x = min_x, min_y = min_y, max_x = max_x, max_y = max_y }
end

function camera.update(target_x, target_y, delta)
	local dt = delta or (1 / 60)

	local tx = target_x + config.offset_x + config.lookahead_x - viewport.width * 0.5
	local ty = target_y + config.offset_y + config.lookahead_y - viewport.height * 0.5

	local bounds = config.bounds
	if bounds then
		tx = max(bounds.min_x, min(bounds.max_x, tx))
		ty = max(bounds.min_y, min(bounds.max_y, ty))
	end

	local dx = tx - state.x
	local dy = ty - state.y

	if abs(dx) < config.dead_zone_x then
		dx = 0
	end
	if abs(dy) < config.dead_zone_y then
		dy = 0
	end

	local s = config.smoothing
	local k = s * s
	local c = 2 * s * config.damping

	state.velocity_x = state.velocity_x + (dx * k - state.velocity_x * c) * dt
	state.velocity_y = state.velocity_y + (dy * k - state.velocity_y * c) * dt

	state.x = state.x + state.velocity_x * dt
	state.y = state.y + state.velocity_y * dt

	if bounds then
		if state.x <= bounds.min_x or state.x >= bounds.max_x then
			state.velocity_x = 0
		end
		if state.y <= bounds.min_y or state.y >= bounds.max_y then
			state.velocity_y = 0
		end
		state.x = max(bounds.min_x, min(bounds.max_x, state.x))
		state.y = max(bounds.min_y, min(bounds.max_y, state.y))
	end

	local snap = config.snap_threshold
	if abs(state.velocity_x) < snap and abs(dx) < snap then
		state.velocity_x = 0
	end
	if abs(state.velocity_y) < snap and abs(dy) < snap then
		state.velocity_y = 0
	end

	return state.x, state.y
end

return camera
