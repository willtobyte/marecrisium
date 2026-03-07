return {
	body = "dynamic",

	animation = {
		running = {
			{ 0, 0, 16, 16, 200, 0, 0, 16, 16 },
		},
	},

	grounded = false,

	on_spawn = function(self)
		self.animation = "running"
		self.x = 100
		self.y = 100
	end,

	on_collision_begin = function(self, other_name, other_kind)
		self.grounded = true
	end,

	on_collision_end = function(self, other_name, other_kind)
		self.grounded = false
	end,

	on_loop = function(self, delta)
		local speed = 200
		local vx = 0

		if keyboard.a or keyboard.left then
			vx = vx - speed
		end
		if keyboard.d or keyboard.right then
			vx = vx + speed
		end

		if gamepad.connected then
			vx = vx + gamepad.left_x * speed
		end

		self.vx = vx

		if (keyboard.space or keyboard.w or keyboard.up) and self.grounded then
			self.vy = -400
		end

		if keyboard.space then
			self.angle = self.angle + 360 * delta
		end

		if keyboard.r then
			self.shown = false
		end
	end,
}
