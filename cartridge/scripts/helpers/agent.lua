local cos = math.cos
local sin = math.sin
local atan2 = math.atan2
local random = math.random
local fmod = math.fmod
local min = math.min

local PI = math.pi
local TWO_PI = 2 * PI
local RTD = 180 / PI
local DTR = PI / 180

local PROBE_OFFSETS = { 45, -45, 90, -90, 135, -135, 180 }
local RETRY_FAST = 5

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
	blockers = { tree = true },
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

	local blockers = {}
	for kind in pairs(self.blockers) do
		blockers[kind] = true
	end
	self._blockers = blockers

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
	object._direct = false
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

	local origin_x = object.center_x
	local origin_y = object.center_y
	local target_x = target.center_x
	local target_y = target.center_y

	if object._timer <= 0 then
		object._timer = self.interval

		local horizontal = target_x - origin_x
		local vertical = target_y - origin_y

		local path
		if horizontal * horizontal + vertical * vertical <= self._radius_squared then
			path = world.pathfind(origin_x, origin_y, target_x, target_y, self.body_radius)
		end

		if path and #path > 0 then
			object._path = path
			object._direct = false

			local best = 2
			local nearest = nil
			for index = 2, #path do
				local waypoint_horizontal = path[index][1] - origin_x
				local waypoint_vertical = path[index][2] - origin_y
				local distance_squared = waypoint_horizontal * waypoint_horizontal
					+ waypoint_vertical * waypoint_vertical
				if distance_squared < self._reach_squared then
					best = index + 1
				elseif nearest == nil or distance_squared < nearest then
					nearest = distance_squared
					best = index
				else
					break
				end
			end
			object._waypoint_index = best
		else
			object._path = {}
			object._waypoint_index = 1
			object._direct = horizontal * horizontal + vertical * vertical <= self._radius_squared
			object._timer = min(object._timer, RETRY_FAST)
		end
	end

	local path = object._path
	local waypoint_index = object._waypoint_index
	local length = #path

	while waypoint_index <= length do
		local waypoint = path[waypoint_index]
		local horizontal = waypoint[1] - origin_x
		local vertical = waypoint[2] - origin_y
		if horizontal * horizontal + vertical * vertical < self._reach_squared then
			waypoint_index = waypoint_index + 1
			object._waypoint_index = waypoint_index
		else
			break
		end
	end

	local destination_x, destination_y

	if waypoint_index <= length then
		local waypoint = path[waypoint_index]
		destination_x = waypoint[1]
		destination_y = waypoint[2]
	else
		destination_x = target_x
		destination_y = target_y
	end

	local horizontal = destination_x - origin_x
	local vertical = destination_y - origin_y
	local distance_squared = horizontal * horizontal + vertical * vertical

	if distance_squared <= 0 or distance_squared > self._radius_squared then
		object.velocity_x = 0
		object.velocity_y = 0
		object._angle = nil
		return
	end

	local pursuit_horizontal = target_x - origin_x
	local pursuit_vertical = target_y - origin_y
	local pursuit_squared = pursuit_horizontal * pursuit_horizontal + pursuit_vertical * pursuit_vertical
	local close_range_squared = self._reach_squared * 9

	if pursuit_squared > 0 and (pursuit_squared <= close_range_squared or object._direct) then
		local desired = atan2(pursuit_vertical, pursuit_horizontal)
		object._angle = desired
		object.flip = pursuit_horizontal < 0 and flip.horizontal or pursuit_horizontal > 0 and flip.none or object.flip
		object.velocity_x = cos(desired) * self.speed
		object.velocity_y = sin(desired) * self.speed
		return
	end

	if object._last_x then
		local moved_horizontal = origin_x - object._last_x
		local moved_vertical = origin_y - object._last_y
		if moved_horizontal * moved_horizontal + moved_vertical * moved_vertical < self.deadzone then
			object._stall = object._stall + 1
			if object._stall >= self.threshold then
				object._stall = 0
				object._timer = 0
			end
		else
			object._stall = 0
		end
	end
	object._last_x = origin_x
	object._last_y = origin_y

	local desired = atan2(vertical, horizontal)
	local degrees = desired * RTD
	local blockers = self._blockers
	local hit = world.raycast(object, origin_x, origin_y, degrees, self.probe)[1]

	if hit and blockers[hit.kind] then
		local chosen = nil
		for _, offset in ipairs(PROBE_OFFSETS) do
			local candidate = world.raycast(object, origin_x, origin_y, degrees + offset, self.probe)[1]
			if not candidate or not blockers[candidate.kind] then
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

	local velocity_x = cos(angle) * self.speed
	local velocity_y = sin(angle) * self.speed

	object.flip = velocity_x < 0 and flip.horizontal or velocity_x > 0 and flip.none or object.flip
	object.velocity_x = velocity_x
	object.velocity_y = velocity_y
end

function agent:in_range(object, target)
	local horizontal = target.center_x - object.center_x
	local vertical = target.center_y - object.center_y
	return horizontal * horizontal + vertical * vertical <= self._radius_squared
end

function agent:angle_to(object, target)
	return atan2(target.center_y - object.center_y, target.center_x - object.center_x)
end

return agent
