local controls = require("helpers/controls")

local speed = 100
local jump = -360

return {
	body = "dynamic",

	animation = {
		running = {
			{ 0, 0, 16, 16, 200, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
		self.x = 60
		self.y = 800
	end,

	on_loop = function(self, delta)
		local vx = 0

		if controls.left then
			vx = -speed
			self.flip = "horizontal"
		elseif controls.right then
			vx = speed
			self.flip = "none"
		end

		if self.grounded and controls.jump then
			self.vy = jump
		end

		local riding = self.riding and pool[self.riding]
		if riding then
			vx = vx + (riding.speed_x or 0)
		end

		self.vx = vx
	end,

	on_damage = function(self)
		print("on damage " .. self.name)
	end,
}
