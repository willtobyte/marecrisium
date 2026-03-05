return {
	animation = {
		running = {
			{ 0, 0, 16, 16, 200, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
		self.scale = 3
		self.x = 100
		self.y = 100
	end,

	on_loop = function(self, delta)
		if keyboard.space then
			self.angle = self.angle + 180 * delta
		end

		if keyboard.r then
			self.alpha = self.alpha - 255 * delta
		else
			self.alpha = self.alpha + 255 * delta
		end
	end,
}
