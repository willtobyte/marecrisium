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
	for key, value in pairs(options) do
		config[key] = value
	end
end

function camera.reset(x, y)
	state.x = x or 0
	state.y = y or 0
	state.velocity_x = 0
	state.velocity_y = 0
end

function camera.set_bounds(min_x, min_y, max_x, max_y)
	config.bounds = {
		min_x = min_x,
		min_y = min_y,
		max_x = max_x,
		max_y = max_y,
	}
end

local function clamp(value, min, max)
	if value < min then
		return min
	end
	if value > max then
		return max
	end
	return value
end

function camera.update(target_x, target_y, delta)
	local dt = delta or (1 / 60)

	target_x = target_x + config.offset_x + config.lookahead_x
	target_y = target_y + config.offset_y + config.lookahead_y

	local half_w = viewport.width * 0.5
	local half_h = viewport.height * 0.5

	local desired_x = target_x - half_w
	local desired_y = target_y - half_h

	if config.bounds then
		desired_x = clamp(desired_x, config.bounds.min_x, config.bounds.max_x)
		desired_y = clamp(desired_y, config.bounds.min_y, config.bounds.max_y)
	end

	local diff_x = desired_x - state.x
	local diff_y = desired_y - state.y

	if diff_x > -config.dead_zone_x and diff_x < config.dead_zone_x then
		diff_x = 0
	end
	if diff_y > -config.dead_zone_y and diff_y < config.dead_zone_y then
		diff_y = 0
	end

	local stiffness = config.smoothing * config.smoothing
	local acceleration_x = diff_x * stiffness - state.velocity_x * 2 * config.smoothing * config.damping
	local acceleration_y = diff_y * stiffness - state.velocity_y * 2 * config.smoothing * config.damping

	state.velocity_x = state.velocity_x + acceleration_x * dt
	state.velocity_y = state.velocity_y + acceleration_y * dt

	state.x = state.x + state.velocity_x * dt
	state.y = state.y + state.velocity_y * dt

	if config.bounds then
		state.x = clamp(state.x, config.bounds.min_x, config.bounds.max_x)
		state.y = clamp(state.y, config.bounds.min_y, config.bounds.max_y)

		if state.x == config.bounds.min_x or state.x == config.bounds.max_x then
			state.velocity_x = 0
		end
		if state.y == config.bounds.min_y or state.y == config.bounds.max_y then
			state.velocity_y = 0
		end
	end

	if math.abs(state.velocity_x) < config.snap_threshold and math.abs(diff_x) < config.snap_threshold then
		state.velocity_x = 0
	end
	if math.abs(state.velocity_y) < config.snap_threshold and math.abs(diff_y) < config.snap_threshold then
		state.velocity_y = 0
	end

	return state.x, state.y
end

return camera
