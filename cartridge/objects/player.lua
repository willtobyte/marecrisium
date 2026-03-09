local controls = require("helpers/controls")

local speed = 200
local jump = -400
local ground = {}
local grounded = 0

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

		self.vx = vx

		if grounded > 0 and controls.jump then
			self.vy = jump
		end
	end,

	on_collision_begin = function(self, other_name, _, _, normal_y)
		if normal_y and normal_y < -0.5 then
			ground[other_name] = true
			grounded = grounded + 1
		end
	end,

	on_collision_end = function(self, other_name)
		if ground[other_name] then
			ground[other_name] = nil
			grounded = grounded - 1
		end
	end,
}
