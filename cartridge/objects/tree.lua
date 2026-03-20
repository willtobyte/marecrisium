return {
	body = "static",

	animation = {
		idle = {
			{ 0, 0, 20, 32, 1000, 6, 22, 8, 10 },
		},
	},

	on_spawn = function(self)
		self.animation = "idle"
	end,
}
