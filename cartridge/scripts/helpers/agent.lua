local cos = math.cos
local sin = math.sin
local atan2 = math.atan2
local random = math.random
local fmod = math.fmod

local pi = math.pi
local two_pi = 2 * pi
local radians_to_degrees = 180 / pi
local degrees_to_radians = pi / 180

local PROBE_OFFSETS = { 45, -45, 90, -90, 135, -135, 180 }

local DEFAULTS = {
	detect_radius = 200,
	speed = 102,
	waypoint_reach = 10,
	path_interval = 20,
	probe_distance = 32,
	body_radius = 6,
	stall_threshold = 10,
	stall_min_dist_sq = 1,
	steer_blend = 0.35,
}

local function normalize_angle(a)
	a = fmod(a + pi, two_pi)
	if a < 0 then
		a = a + two_pi
	end
	return a - pi
end

local function lerp_angle(from, to, t)
	return from + normalize_angle(to - from) * t
end

local agent = {}
agent.__index = agent

function agent.new(config)
	config = config or {}
	local self = {}
	for k, v in pairs(DEFAULTS) do
		self[k] = config[k] or v
	end
	self._detect_radius_sq = self.detect_radius * self.detect_radius
	self._waypoint_reach_sq = self.waypoint_reach * self.waypoint_reach
	return setmetatable(self, agent)
end

function agent:init(object)
	object._path = {}
	object._wp = 1
	object._timer = random(1, self.path_interval)
	object._stall = 0
	object._angle = nil
	object._last_x = nil
	object._last_y = nil
end

function agent:stop(object)
	object._path = {}
	object._wp = 1
	object.vx = 0
	object.vy = 0
end

function agent:reset(object)
	object._timer = 0
	object._angle = nil
end

function agent:chase(object, target, world)
	object._timer = object._timer - 1

	if object._timer <= 0 then
		object._timer = self.path_interval

		local px, py = target.x, target.y
		local dx, dy = px - object.x, py - object.y

		local path
		if dx * dx + dy * dy <= self._detect_radius_sq then
			path = world.pathfind(object.x, object.y, px, py, self.body_radius)
		end

		if path and #path > 0 then
			object._path = path

			local best_wp = 2
			local best_dist = nil
			for i = 2, #path do
				local wx = path[i][1] - object.x
				local wy = path[i][2] - object.y
				local d = wx * wx + wy * wy
				if d < self._waypoint_reach_sq then
					best_wp = i + 1
				elseif best_dist == nil or d < best_dist then
					best_dist = d
					best_wp = i
				else
					break
				end
			end
			object._wp = best_wp
		else
			object._path = {}
			object._wp = 1
		end
	end

	local path = object._path
	local wp = object._wp
	local path_len = #path

	while wp <= path_len do
		local waypoint = path[wp]
		local dx = waypoint[1] - object.x
		local dy = waypoint[2] - object.y
		if dx * dx + dy * dy < self._waypoint_reach_sq then
			wp = wp + 1
			object._wp = wp
		else
			break
		end
	end

	local tx, ty

	if wp <= path_len then
		local waypoint = path[wp]
		tx = waypoint[1]
		ty = waypoint[2]
	else
		tx = target.x
		ty = target.y
	end

	local dx = tx - object.x
	local dy = ty - object.y
	local dist_sq = dx * dx + dy * dy

	if dist_sq <= 0 or dist_sq > self._detect_radius_sq then
		object.vx = 0
		object.vy = 0
		object._angle = nil
		return
	end

	if object._last_x then
		local ddx = object.x - object._last_x
		local ddy = object.y - object._last_y
		if ddx * ddx + ddy * ddy < self.stall_min_dist_sq then
			object._stall = object._stall + 1
			if object._stall >= self.stall_threshold then
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
	local adeg = desired * radians_to_degrees
	local hit = world.raycast(object, object.x, object.y, adeg, self.probe_distance)[1]

	if hit and hit.kind ~= target.kind then
		local chosen = nil
		for _, offset in ipairs(PROBE_OFFSETS) do
			local probe = world.raycast(object, object.x, object.y, adeg + offset, self.probe_distance)[1]
			if not probe or probe.kind == target.kind then
				chosen = desired + offset * degrees_to_radians
				break
			end
		end
		if chosen then
			desired = chosen
		else
			object.vx = 0
			object.vy = 0
			object._angle = nil
			object._timer = 0
			return
		end
	end

	local angle
	if object._angle then
		angle = lerp_angle(object._angle, desired, self.steer_blend)
	else
		angle = desired
	end
	object._angle = angle

	local vx = cos(angle) * self.speed
	local vy = sin(angle) * self.speed

	object.flip = vx < 0 and "horizontal" or vx > 0 and "none" or object.flip
	object.vx = vx
	object.vy = vy
end

function agent:in_range(object, target)
	local dx = target.x - object.x
	local dy = target.y - object.y
	return dx * dx + dy * dy <= self._detect_radius_sq
end

function agent:angle_to(object, target)
	return atan2(target.y - object.y, target.x - object.x)
end

return agent
