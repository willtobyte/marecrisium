local cos = math.cos
local sin = math.sin

local SPEED = 260
local TTL = 1.2

return {
	body = "dynamic",

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 4, 8, 8, 8 },
		},
	},

	on_spawn = function(self)
		self._lifetime = TTL
		self._alive = true
	end,

	on_loop = function(self, delta)
		if not self._alive then
			return
		end

		self._lifetime = self._lifetime - delta
		if self._lifetime <= 0 then
			self._alive = false
			world.destroy(self)
			return
		end

		local angle = self._angle or 0
		self.velocity_x = cos(angle) * SPEED
		self.velocity_y = sin(angle) * SPEED
	end,

	on_collision_begin = function(self, name, kind)
		if not self._alive then
			return
		end

		if kind == "player" then
			pool.player:damage()
			self._alive = false
			world.destroy(self)
		elseif kind ~= "demon" then
			self._alive = false
			world.destroy(self)
		end
	end,
}
