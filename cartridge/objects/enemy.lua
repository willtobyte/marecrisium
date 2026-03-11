local speed = 30
local max_distance = 64

return {
	body = "kinematic",
	cullable = true,

	animation = {
		running = {
			{ 0, 0, 16, 16, 200, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
		self.direction = 1
		self.distance = 0
		self.speed_x = speed
	end,

	on_collision_begin = function(self, name, kind, normal_x, normal_y)
		if kind ~= "player" then
			return
		end
		if not normal_x then
			return
		end

		local side
		if normal_y > 0.5 then
			side = "bottom"
		elseif normal_y < -0.5 then
			side = "top"
		elseif normal_x > 0.5 then
			side = "right"
		elseif normal_x < -0.5 then
			side = "left"
		end

		if side then
			print("collision with " .. name .. " from " .. side)
			pool.player:damage()
		end
	end,

	on_loop = function(self, delta)
		self.speed_x = speed * self.direction
		self.x = self.x + self.speed_x * delta
		self.distance = self.distance + speed * delta

		if self.distance >= max_distance then
			self.direction = -self.direction
			self.distance = 0
			self.flip = self.direction < 0 and "horizontal" or "none"
		end
	end,
}
