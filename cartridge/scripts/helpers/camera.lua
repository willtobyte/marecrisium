local abs = math.abs
local max = math.max
local min = math.min
local viewport = viewport

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

local k = config.smoothing * config.smoothing
local c = 2 * config.smoothing * config.damping

function camera.configure(options)
	for key, val in pairs(options) do
		config[key] = val
	end
	k = config.smoothing * config.smoothing
	c = 2 * config.smoothing * config.damping
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
	local tx = target.x + config.offset_x + config.lookahead_x - viewport.width * 0.5
	local ty = target.y + config.offset_y + config.lookahead_y - viewport.height * 0.5

	local bx1, by1, bx2, by2 = config.bounds_min_x, config.bounds_min_y, config.bounds_max_x, config.bounds_max_y
	if bx1 then
		tx = max(bx1, min(bx2, tx))
		ty = max(by1, min(by2, ty))
	end

	state.x = tx
	state.y = ty
	state.vx = 0
	state.vy = 0
end

function camera.update(target, delta)
	local dt = delta or (1 / 60)

	local tx = target.x + config.offset_x + config.lookahead_x - viewport.width * 0.5
	local ty = target.y + config.offset_y + config.lookahead_y - viewport.height * 0.5

	local bx1, by1, bx2, by2 = config.bounds_min_x, config.bounds_min_y, config.bounds_max_x, config.bounds_max_y
	if bx1 then
		tx = max(bx1, min(bx2, tx))
		ty = max(by1, min(by2, ty))
	end

	local dx = tx - state.x
	local dy = ty - state.y

	if abs(dx) < config.dead_zone_x then
		dx = 0
	end
	if abs(dy) < config.dead_zone_y then
		dy = 0
	end

	state.vx = state.vx + (dx * k - state.vx * c) * dt
	state.vy = state.vy + (dy * k - state.vy * c) * dt

	state.x = state.x + state.vx * dt
	state.y = state.y + state.vy * dt

	if bx1 then
		if state.x <= bx1 or state.x >= bx2 then
			state.vx = 0
		end
		if state.y <= by1 or state.y >= by2 then
			state.vy = 0
		end
		state.x = max(bx1, min(bx2, state.x))
		state.y = max(by1, min(by2, state.y))
	end

	local snap = config.snap_threshold
	if abs(state.vx) < snap and dx == 0 then
		state.vx = 0
	end
	if abs(state.vy) < snap and dy == 0 then
		state.vy = 0
	end
end

function camera.position()
	return state.x, state.y
end

return camera
