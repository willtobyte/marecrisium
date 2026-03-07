return {
	body = "static",

	animation = {
		running = {
			{ 0, 0, 16, 16, 100, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
		self.x = 200
		self.y = 100
	end,

	on_collision_begin = function(self, other_name, other_kind)
		print("enemy collision begin", other_name, other_kind)
	end,

	on_collision_end = function(self, other_name, other_kind)
		print("enemy collision end", other_name, other_kind)
	end,
}
