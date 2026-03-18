local cos = math.cos
local sin = math.sin
local atan2 = math.atan2
local random = math.random
local fmod = math.fmod

local pi = math.pi
local two_pi = 2 * pi
local radians_to_degrees = 180 / pi
local degrees_to_radians = pi / 180

local DETECT_RADIUS = 200
local SPEED = 102
local WAYPOINT_REACH = 10
local WAYPOINT_REACH_SQ = WAYPOINT_REACH * WAYPOINT_REACH
local PATH_INTERVAL = 20
local PROBE_DISTANCE = 32
local BODY_RADIUS = 6
local STALL_THRESHOLD = 10
local STALL_MIN_DIST_SQ = 1
local STEER_BLEND = 0.35

local DETECT_RADIUS_SQUARED = DETECT_RADIUS * DETECT_RADIUS

local PROBE_OFFSETS = { 45, -45, 90, -90, 135, -135, 180 }

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

local function clear_path(self)
	self._path = {}
	self._wp = 1
end

return {
	body = "dynamic",
	sleepable = true,

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},

	on_spawn = function(self)
		self.animation = "idle"
		clear_path(self)
		self._timer = random(1, PATH_INTERVAL)
		self._stall = 0
		self._touching_player = false
		self._angle = nil
	end,

	on_loop = function(self, delta)
		local player = pool.player
		if not player or not player.alive then
			return
		end

		if self._touching_player then
			self.vx = 0
			self.vy = 0
			return
		end

		self._timer = self._timer - 1

		if self._timer <= 0 then
			self._timer = PATH_INTERVAL

			local px, py = player.x, player.y
			local dx, dy = px - self.x, py - self.y

			local path
			if dx * dx + dy * dy <= DETECT_RADIUS_SQUARED then
				path = world.pathfind(self.x, self.y, px, py, BODY_RADIUS)
			end

			if path and #path > 0 then
				self._path = path

				local best_wp = 2
				local best_dist = nil
				for i = 2, #path do
					local wx = path[i][1] - self.x
					local wy = path[i][2] - self.y
					local d = wx * wx + wy * wy
					if d < WAYPOINT_REACH_SQ then
						best_wp = i + 1
					elseif best_dist == nil or d < best_dist then
						best_dist = d
						best_wp = i
					else
						break
					end
				end
				self._wp = best_wp
			else
				clear_path(self)
			end
		end

		local path = self._path
		local wp = self._wp
		local path_len = #path

		while wp <= path_len do
			local target = path[wp]
			local dx = target[1] - self.x
			local dy = target[2] - self.y
			if dx * dx + dy * dy < WAYPOINT_REACH_SQ then
				wp = wp + 1
				self._wp = wp
			else
				break
			end
		end

		local tx, ty

		if wp <= path_len then
			local target = path[wp]
			tx = target[1]
			ty = target[2]
		else
			tx = player.x
			ty = player.y
		end

		local dx = tx - self.x
		local dy = ty - self.y
		local dist_sq = dx * dx + dy * dy

		if dist_sq <= 0 or dist_sq > DETECT_RADIUS_SQUARED then
			self.vx = 0
			self.vy = 0
			self._angle = nil
			return
		end

		if self._last_x then
			local ddx = self.x - self._last_x
			local ddy = self.y - self._last_y
			if ddx * ddx + ddy * ddy < STALL_MIN_DIST_SQ then
				self._stall = self._stall + 1
				if self._stall >= STALL_THRESHOLD then
					self._stall = 0
					self._timer = 0
				end
			else
				self._stall = 0
			end
		end
		self._last_x = self.x
		self._last_y = self.y

		local desired = atan2(dy, dx)
		local adeg = desired * radians_to_degrees
		local hit = world.raycast(self, self.x, self.y, adeg, PROBE_DISTANCE)[1]

		if hit and hit.kind ~= "player" then
			local chosen = nil
			for _, offset in ipairs(PROBE_OFFSETS) do
				local probe = world.raycast(self, self.x, self.y, adeg + offset, PROBE_DISTANCE)[1]
				if not probe or probe.kind == "player" then
					chosen = desired + offset * degrees_to_radians
					break
				end
			end
			if chosen then
				desired = chosen
			else
				self.vx = 0
				self.vy = 0
				self._angle = nil
				self._timer = 0
				return
			end
		end

		local angle
		if self._angle then
			angle = lerp_angle(self._angle, desired, STEER_BLEND)
		else
			angle = desired
		end
		self._angle = angle

		local vx = cos(angle) * SPEED
		local vy = sin(angle) * SPEED

		self.flip = vx < 0 and "horizontal" or vx > 0 and "none" or self.flip
		self.vx = vx
		self.vy = vy
	end,

	on_collision_begin = function(self, name, kind)
		if kind == "player" then
			self._touching_player = true
			self.vx = 0
			self.vy = 0
			clear_path(self)
			pool.player:damage()
		end
	end,

	on_collision_end = function(self, name, kind)
		if kind == "player" then
			self._touching_player = false
			self._timer = 0
			self._angle = nil
		end
	end,
}
