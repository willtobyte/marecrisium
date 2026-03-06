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
		local speed = 300 * delta

		if keyboard.w then
			self.y = self.y - speed
		end
		if keyboard.s then
			self.y = self.y + speed
		end
		if keyboard.a then
			self.x = self.x - speed
		end
		if keyboard.d then
			self.x = self.x + speed
		end

		if gamepad.connected then
			self.x = self.x + gamepad.left_x * speed
			self.y = self.y + gamepad.left_y * speed
		end

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
