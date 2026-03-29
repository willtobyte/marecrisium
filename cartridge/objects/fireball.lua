local cos = math.cos
local sin = math.sin

local SPEED = 160
local TTL = 0.7

return {
	body = "dynamic",

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 4, 8, 8, 8 },
		},
	},

	on_spawn = function(self)
		self._ttl = TTL
		self._alive = true
	end,

	on_loop = function(self, delta)
		if not self._alive then
			return
		end

		self._ttl = self._ttl - delta
		if self._ttl <= 0 then
			self._alive = false
			world.destroy(self)
			return
		end

		local angle = self._angle or 0
		self.vx = cos(angle) * SPEED
		self.vy = sin(angle) * SPEED
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

		error("Fireball")
	end,
}
