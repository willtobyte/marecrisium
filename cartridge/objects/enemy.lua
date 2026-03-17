local sqrt = math.sqrt
local random = math.random

local DETECT_RADIUS = 200
local SPEED = 55
local WAYPOINT_REACH = 10
local PATH_INTERVAL = 20

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
	end,

	on_loop = function(self, delta)
		local player = pool.player
		if not player or not player.alive then
			return
		end

		self._timer = self._timer - 1

		if self._timer <= 0 then
			self._timer = PATH_INTERVAL

			local px = player.x
			local py = player.y
			local dx = px - self.x
			local dy = py - self.y

			if dx * dx + dy * dy <= DETECT_RADIUS * DETECT_RADIUS then
				local path = world.pathfind(self.x, self.y, px, py)
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

		local path = self._path
		local wp = self._wp

		if not path or wp > #path then
			self.vx = 0
			self.vy = 0
			return
		end

		local target = path[wp]
		local tx = target[1]
		local ty = target[2]
		local dx = tx - self.x
		local dy = ty - self.y
		local dist = sqrt(dx * dx + dy * dy)

		if dist < WAYPOINT_REACH then
			self._wp = wp + 1
			self.vx = 0
			self.vy = 0
			return
		end

		local inv = SPEED / dist
		local vx = dx * inv
		local vy = dy * inv

		if vx < 0 then
			self.flip = "horizontal"
		elseif vx > 0 then
			self.flip = "none"
		end

		self.vx = vx
		self.vy = vy
	end,

	on_collision_begin = function(self, name, kind)
		if kind ~= "player" then
			return
		end
		pool.player:damage()
	end,
}
