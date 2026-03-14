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

		local target = self.riding
		if target then
			local enemy = pool[target]
			if enemy then
				vx = vx + (enemy.speed_x or 0)
			end
		end

		self.vx = vx
	end,

	on_press = function(self, x, y, button)
		print("on_press", self.name, x, y, button)
		pool.smoke1.active = false
		pool.smoke2.active = false
		pool.smoke3.active = false
	end,

	on_click = function(self, x, y, button)
		print("on_click", self.name, x, y, button)
	end,

	on_hover = function(self)
		print("on_hover", self.name)
	end,

	on_unhover = function(self)
		print("on_unhover", self.name)
	end,

	on_damage = function(self)
		print("on damage " .. self.name)
	end,
}
