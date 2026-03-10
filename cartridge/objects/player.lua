local controls = require("helpers/controls")

local speed = 200
local jump = -400
local spin = 360
local ground = {}
local grounded = 0
local riding = {}

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

		if grounded > 0 and controls.jump then
			self.vy = jump
			riding = {}
		end

		for name, _ in pairs(riding) do
			local enemy = pool[name]
			if enemy then
				vx = vx + enemy.vx
			end
		end

		self.vx = vx
	end,

	on_collision_begin = function(self, name, kind, _, normal_y)
		if normal_y and normal_y > 0.5 then
			ground[name] = (ground[name] or 0) + 1
			grounded = grounded + 1
			if kind == "enemy" then
				riding[name] = true
			end
		end
	end,

	on_collision_end = function(self, name)
		if ground[name] and ground[name] > 0 then
			ground[name] = ground[name] - 1
			grounded = grounded - 1
			if ground[name] == 0 then
				ground[name] = nil
				riding[name] = nil
			end
		end
	end,
}
