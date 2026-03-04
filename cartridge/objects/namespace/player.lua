return {
	animation = {
		running = {
			{ 0, 0, 16, 16, 200, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
		-- sself.angle = 45
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

	on_animation_begin = function(self, animation) end,

	on_animation_end = function(self, animation) end,

	on_collision_begin = function(self, name, kind)
		print("player: collision BEGIN with " .. name .. " (" .. kind .. ")")
	end,

	on_collision_end = function(self, name, kind)
		print("player: collision END with " .. name .. " (" .. kind .. ")")
	end,

	on_screen_exit = function(self, direction)
		print("player: screen EXIT " .. direction)
	end,

	on_screen_enter = function(self, direction)
		print("player: screen ENTER " .. direction)
	end,
}
