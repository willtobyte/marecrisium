local cos = math.cos
local sin = math.sin
local atan2 = math.atan2
local random = math.random
local fmod = math.fmod

local PI = math.pi
local TWO_PI = 2 * PI
local RTD = 180 / PI
local DTR = PI / 180

local PROBE_OFFSETS = { 45, -45, 90, -90, 135, -135, 180 }

local DEFAULTS = {
	radius = 200,
	speed = 102,
	reach = 10,
	interval = 20,
	probe = 32,
	body_radius = 6,
	threshold = 10,
	deadzone = 1,
	blend = 0.35,
}

local function normalize_angle(angle)
	angle = fmod(angle + PI, TWO_PI)
	if angle < 0 then
		angle = angle + TWO_PI
	end
	return angle - PI
end

local function lerp_angle(from, to, blend)
	return from + normalize_angle(to - from) * blend
end

local agent = {}
agent.__index = agent

function agent.new(config)
	config = config or {}
	local self = {}
	for k, v in pairs(DEFAULTS) do
		self[k] = config[k] or v
	end
	self._radius_squared = self.radius * self.radius
	self._reach_squared = self.reach * self.reach
	return setmetatable(self, agent)
end

function agent:init(object)
	object._path = {}
	object._waypoint_index = 1
	object._timer = random(1, self.interval)
	object._stall = 0
	object._angle = nil
	object._last_x = nil
	object._last_y = nil
end

function agent:stop(object)
	object._path = {}
	object._waypoint_index = 1
	object.velocity_x = 0
	object.velocity_y = 0
end

function agent:reset(object)
	object._timer = 0
	object._angle = nil
end

function agent:chase(object, target, world)
	object._timer = object._timer - 1

	if object._timer <= 0 then
		object._timer = self.interval

		local px, py = target.x, target.y
		local dx, dy = px - object.x, py - object.y

		local path
		if dx * dx + dy * dy <= self._radius_squared then
			path = world.pathfind(object.x, object.y, px, py, self.body_radius)
		end

		if path and #path > 0 then
			object._path = path

			local best = 2
			local nearest = nil
			for i = 2, #path do
				local wx = path[i][1] - object.x
				local wy = path[i][2] - object.y
				local dsq = wx * wx + wy * wy
				if dsq < self._reach_squared then
					best = i + 1
				elseif nearest == nil or dsq < nearest then
					nearest = dsq
					best = i
				else
					break
				end
			end
			object._waypoint_index = best
		else
			object._path = {}
			object._waypoint_index = 1
		end
	end

	local path = object._path
	local wi = object._waypoint_index
	local length = #path

	while wi <= length do
		local waypoint = path[wi]
		local dx = waypoint[1] - object.x
		local dy = waypoint[2] - object.y
		if dx * dx + dy * dy < self._reach_squared then
			wi = wi + 1
			object._waypoint_index = wi
		else
			break
		end
	end

	local tx, ty

	if wi <= length then
		local waypoint = path[wi]
		tx = waypoint[1]
		ty = waypoint[2]
	else
		tx = target.x
		ty = target.y
	end

	local dx = tx - object.x
	local dy = ty - object.y
	local dsq = dx * dx + dy * dy

	if dsq <= 0 or dsq > self._radius_squared then
		object.velocity_x = 0
		object.velocity_y = 0
		object._angle = nil
		return
	end

	local pdx = target.x - object.x
	local pdy = target.y - object.y
	local pdsq = pdx * pdx + pdy * pdy
	local crsq = self._reach_squared * 9

	if pdsq > 0 and pdsq <= crsq then
		local desired = atan2(pdy, pdx)
		object._angle = desired
		object.velocity_x = cos(desired) * self.speed
		object.velocity_y = sin(desired) * self.speed
		return
	end

	if object._last_x then
		local mx = object.x - object._last_x
		local my = object.y - object._last_y
		if mx * mx + my * my < self.deadzone then
			object._stall = object._stall + 1
			if object._stall >= self.threshold then
				object._stall = 0
				object._timer = 0
			end
		else
			object._stall = 0
		end
	end
	object._last_x = object.x
	object._last_y = object.y

	local desired = atan2(dy, dx)
	local degrees = desired * RTD
	local hit = world.raycast(object, object.x, object.y, degrees, self.probe)[1]

	if hit and hit.kind ~= target.kind then
		local chosen = nil
		for _, offset in ipairs(PROBE_OFFSETS) do
			local candidate = world.raycast(object, object.x, object.y, degrees + offset, self.probe)[1]
			if not candidate or candidate.kind == target.kind then
				chosen = desired + offset * DTR
				break
			end
		end
		if chosen then
			desired = chosen
		else
			object.velocity_x = 0
			object.velocity_y = 0
			object._angle = nil
			object._timer = 0
			return
		end
	end

	local angle
	if object._angle then
		angle = lerp_angle(object._angle, desired, self.blend)
	else
		angle = desired
	end
	object._angle = angle

	local vx = cos(angle) * self.speed
	local vy = sin(angle) * self.speed

	object.flip = vx < 0 and flip.horizontal or vx > 0 and flip.none or object.flip
	object.velocity_x = vx
	object.velocity_y = vy
end

function agent:in_range(object, target)
	local dx = target.x - object.x
	local dy = target.y - object.y
	return dx * dx + dy * dy <= self._radius_squared
end

function agent:angle_to(object, target)
	return atan2(target.y - object.y, target.x - object.x)
end

return agent
