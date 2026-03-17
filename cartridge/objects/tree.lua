return {
	body = "static",
	sleepable = true,

	animation = {
		idle = {
			{ 0, 0, 32, 48, 1000, 16, 32, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "idle"
	end,
}
