local sqrt = math.sqrt
local cos = math.cos
local sin = math.sin
local atan2 = math.atan2
local random = math.random

local pi = math.pi
local radians_to_degrees = 180 / pi
local degrees_to_radians = pi / 180

local DETECT_RADIUS = 200
local SPEED = 55
local WAYPOINT_REACH = 10
local PATH_INTERVAL = 20
local PROBE_DISTANCE = 32
local BODY_RADIUS = 6
local STALL_THRESHOLD = 10
local STALL_MIN_DIST_SQ = 1

local DETECT_RADIUS_SQUARED = DETECT_RADIUS * DETECT_RADIUS

local PROBE_OFFSETS = { 45, -45, 90, -90, 135, -135, 180 }

return {
	body = "dynamic",

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},

	on_spawn = function(self)
		self.animation = "idle"
		self._path = {}
		self._wp = 1
		self._timer = random(1, PATH_INTERVAL)
		self._stall = 0
		self._last_x = nil
		self._last_y = nil
	end,

	on_loop = function(self, delta)
		local player = pool.player
		if not player or not player.alive then
			return
		end

		self._timer = self._timer - 1

		if self._timer <= 0 then
			self._timer = PATH_INTERVAL

			local px, py = player.x, player.y
			local dx, dy = px - self.x, py - self.y

			if dx * dx + dy * dy <= DETECT_RADIUS_SQUARED then
				local path = world.pathfind(self.x, self.y, px, py, BODY_RADIUS)
				if path and #path > 0 then
					self._path = path
					self._wp = 2
				else
					self._path = {}
					self._wp = 1
				end
			else
				self._path = {}
				self._wp = 1
			end
		end

		local wp = self._wp
		if wp > #self._path then
			self.vx = 0
			self.vy = 0
			return
		end

		local target = self._path[wp]
		local dx = target[1] - self.x
		local dy = target[2] - self.y

		if sqrt(dx * dx + dy * dy) < WAYPOINT_REACH then
			self._wp = wp + 1
			self.vx = 0
			self.vy = 0
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

		local angle = atan2(dy, dx)
		local adeg = angle * radians_to_degrees
		local hit = world.raycast(self, self.x, self.y, adeg, PROBE_DISTANCE)[1]

		if hit and hit.kind ~= "player" then
			local chosen = nil
			for _, offset in ipairs(PROBE_OFFSETS) do
				local probe = world.raycast(self, self.x, self.y, adeg + offset, PROBE_DISTANCE)[1]
				if not probe or probe.kind == "player" then
					chosen = angle + offset * degrees_to_radians
					break
				end
			end
			if chosen then
				angle = chosen
			else
				self.vx = 0
				self.vy = 0
				self._timer = 0
				return
			end
		end

		local vx = cos(angle) * SPEED
		local vy = sin(angle) * SPEED

		self.flip = vx < 0 and "horizontal" or vx > 0 and "none" or self.flip
		self.vx = vx
		self.vy = vy
	end,

	on_collision_begin = function(self, name, kind)
		if kind == "player" then
			pool.player:damage()
		end
	end,
}
