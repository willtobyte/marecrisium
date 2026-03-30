local cos = math.cos
local sin = math.sin
local atan2 = math.atan2
local random = math.random
local fmod = math.fmod

local PI = math.pi
local TWO_PI = 2 * PI
local radians_to_degrees = 180 / PI
local degrees_to_radians = PI / 180

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
	self._detect_radius_sq = self.detect_radius * self.detect_radius
	self._waypoint_reach_sq = self.waypoint_reach * self.waypoint_reach
	return setmetatable(self, agent)
end

function agent:init(object)
	object._path = {}
	object._waypoint_index = 1
	object._timer = random(1, self.path_interval)
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
		object._timer = self.path_interval

		local player_x, player_y = target.x, target.y
		local delta_x, delta_y = player_x - object.x, player_y - object.y

		local path
		if delta_x * delta_x + delta_y * delta_y <= self._detect_radius_sq then
			path = world.pathfind(object.x, object.y, player_x, player_y, self.body_radius)
		end

		if path and #path > 0 then
			object._path = path

			local best_waypoint = 2
			local best_distance = nil
			for i = 2, #path do
				local waypoint_x = path[i][1] - object.x
				local waypoint_y = path[i][2] - object.y
				local distance_squared = waypoint_x * waypoint_x + waypoint_y * waypoint_y
				if distance_squared < self._waypoint_reach_sq then
					best_waypoint = i + 1
				elseif best_distance == nil or distance_squared < best_distance then
					best_distance = distance_squared
					best_waypoint = i
				else
					break
				end
			end
			object._waypoint_index = best_waypoint
		else
			object._path = {}
			object._waypoint_index = 1
		end
	end

	local path = object._path
	local waypoint_index = object._waypoint_index
	local path_length = #path

	while waypoint_index <= path_length do
		local waypoint = path[waypoint_index]
		local delta_x = waypoint[1] - object.x
		local delta_y = waypoint[2] - object.y
		if delta_x * delta_x + delta_y * delta_y < self._waypoint_reach_sq then
			waypoint_index = waypoint_index + 1
			object._waypoint_index = waypoint_index
		else
			break
		end
	end

	local target_x, target_y

	if waypoint_index <= path_length then
		local waypoint = path[waypoint_index]
		target_x = waypoint[1]
		target_y = waypoint[2]
	else
		target_x = target.x
		target_y = target.y
	end

	local delta_x = target_x - object.x
	local delta_y = target_y - object.y
	local distance_squared = delta_x * delta_x + delta_y * delta_y

	if distance_squared <= 0 or distance_squared > self._detect_radius_sq then
		object.velocity_x = 0
		object.velocity_y = 0
		object._angle = nil
		return
	end

	local player_dx = target.x - object.x
	local player_dy = target.y - object.y
	local player_dist_sq = player_dx * player_dx + player_dy * player_dy
	local close_range_sq = self._waypoint_reach_sq * 9

	if player_dist_sq > 0 and player_dist_sq <= close_range_sq then
		local desired = atan2(player_dy, player_dx)
		object._angle = desired
		object.velocity_x = cos(desired) * self.speed
		object.velocity_y = sin(desired) * self.speed
		return
	end

	if object._last_x then
		local movement_x = object.x - object._last_x
		local movement_y = object.y - object._last_y
		if movement_x * movement_x + movement_y * movement_y < self.stall_min_dist_sq then
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

	local desired = atan2(delta_y, delta_x)
	local angle_degrees = desired * radians_to_degrees
	local hit = world.raycast(object, object.x, object.y, angle_degrees, self.probe_distance)[1]

	if hit and hit.kind ~= target.kind then
		local chosen = nil
		for _, offset in ipairs(PROBE_OFFSETS) do
			local probe = world.raycast(object, object.x, object.y, angle_degrees + offset, self.probe_distance)[1]
			if not probe or probe.kind == target.kind then
				chosen = desired + offset * degrees_to_radians
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
		angle = lerp_angle(object._angle, desired, self.steer_blend)
	else
		angle = desired
	end
	object._angle = angle

	local velocity_x = cos(angle) * self.speed
	local velocity_y = sin(angle) * self.speed

	object.flip = velocity_x < 0 and "horizontal" or velocity_x > 0 and "none" or object.flip
	object.velocity_x = velocity_x
	object.velocity_y = velocity_y
end

function agent:in_range(object, target)
	local delta_x = target.x - object.x
	local delta_y = target.y - object.y
	return delta_x * delta_x + delta_y * delta_y <= self._detect_radius_sq
end

function agent:angle_to(object, target)
	return atan2(target.y - object.y, target.x - object.x)
end

return agent
