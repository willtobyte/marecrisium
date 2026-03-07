return {
	body = "dynamic",

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

	on_click = function(self, x, y, button)
		print(x, y, button)
	end,

	on_hover = function(self)
		print("hover")
	end,

	on_unhover = function(self)
		print("unhover")
	end,

	on_collision_begin = function(self, other_name, other_kind)
		print("player collision begin", other_name, other_kind)
	end,

	on_collision_end = function(self, other_name, other_kind)
		print("player collision end", other_name, other_kind)
	end,

	on_loop = function(self, delta)
		local speed = 300
		local vx = 0
		local vy = 0

		if keyboard.w then
			vy = vy - speed
		end
		if keyboard.s then
			vy = vy + speed
		end
		if keyboard.a then
			vx = vx - speed
		end
		if keyboard.d then
			vx = vx + speed
		end

		if gamepad.connected then
			vx = vx + gamepad.left_x * speed
			vy = vy + gamepad.left_y * speed
		end

		self.vx = vx
		self.vy = vy

		if keyboard.space or gamepad.south then
			self.angle = self.angle + 180 * delta
		end

		if keyboard.r or gamepad.east then
			self.alpha = self.alpha - 255 * delta
		else
			self.alpha = self.alpha + 255 * delta
		end
	end,
}
