return {
	animation = {
		running = {
			{ x, y, w, h, duration, cx, cy, cw, ch }, -- x, y, w, h on spritesheet. duration is duration of frame in ms. cx, cy, cw, ch are collision quad
		},
	},

	on_spawn = function(self)
		self.animation = "running"
	end,

	on_loop = function(self, delta)
		-- every frame
	end,

	on_animation_begin = function(self, animation)
		-- when the animation begins, animation is its name
	end,

	on_animation_end = function(self, animation)
		-- when the animation ends, animation is its name
	end,

	on_collision_begin = function(self, name, kind)
		-- when collision begins, name is the other object's name and kind is the other object's kind
	end,

	on_collision_end = function(self, name, kind)
		-- when collision ends, name is the other object's name and kind is the other object's kind
	end,

	on_screen_exit = function(self, direction)
		-- when the object exits the screen, direction is the exit direction
	end,

	on_screen_enter = function(self, direction)
		-- when the object enters the screen, direction is the enter direction
	end,

	on_damage = function(self, amount)
		-- when pool.player:damage(10) is called
	end,
}
