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
	end,

	on_loop = function(self, delta)
		self.x = self.x + speed * self.direction * delta
		self.distance = self.distance + speed * delta

		if self.distance >= max_distance then
			self.direction = -self.direction
			self.distance = 0
			self.flip = self.direction < 0 and "horizontal" or "none"
		end
	end,

	on_sleep = function(self) end,

	on_wake = function(self) end,

	on_collision_begin = function(self, name, kind) end,

	on_collision_end = function(self, name, kind) end,
}
