return {
	animation = {
		running = {
			-- x, y, w, h on the spritesheet; duration is the frame duration in ms; cx, cy, cw, ch define the collision quad
			{ 0, 0, 16, 16, 100 },
		},
	},

	on_spawn = function(self)
		-- Called once when the object is spawned
		self.animation = "running"
	end,

	on_loop = function(self, delta)
		-- Called every frame with the elapsed time since the last frame
	end,

	on_animation_begin = function(self, animation)
		print("on_animation_begin", animation)
		-- Called when an animation begins; animation is its name
	end,

	on_animation_end = function(self, animation)
		print("on_animation_end", animation)
		-- Called when an animation ends; animation is its name
	end,

	on_collision_begin = function(self, name, kind)
		-- Called when a collision begins; name is the other object's name and kind is the other object's kind
	end,

	on_collision_end = function(self, name, kind)
		-- Called when a collision ends; name is the other object's name and kind is the other object's kind
	end,

	on_screen_exit = function(self, direction)
		-- Called when the object exits the screen; direction is the exit direction
	end,

	on_screen_enter = function(self, direction)
		-- Called when the object enters the screen; direction is the entry direction
	end,

	on_damage = function(self, amount)
		-- Called when the object takes damage, e.g. pool.player:damage(10)
	end,
}
