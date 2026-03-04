return {
	animation = {
		running = {
			{ 0, 0, 16, 16, 100, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
		self.scale = 3
		self.x = 300
		self.y = 100
	end,

	on_loop = function(self, delta) end,

	on_animation_begin = function(self, animation) end,
	on_animation_end = function(self, animation) end,

	on_collision_begin = function(self, name, kind)
		print("enemy: collision BEGIN with " .. name .. " (" .. kind .. ")")
	end,

	on_collision_end = function(self, name, kind)
		print("enemy: collision END with " .. name .. " (" .. kind .. ")")
	end,
}
