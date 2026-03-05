return {
	animation = {
		running = {
			{ 0, 0, 16, 16, 100, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
	end,
}
