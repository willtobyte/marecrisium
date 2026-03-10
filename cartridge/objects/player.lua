local controls = require("helpers/controls")

local speed = 200
local jump = -400

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

		local target = self.riding
		if target then
			local enemy = pool[target]
			if enemy then
				vx = vx + (enemy.speed_x or 0)
			end
		end

		self.vx = vx
	end,
}
