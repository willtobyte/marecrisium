local controls = require("helpers/controls")
local sqrt = math.sqrt

local speed = 90

return {
	body = "dynamic",

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},

	on_spawn = function(self)
		self.animation = "idle"
		self.x = 0
		self.y = 0
	end,

	on_loop = function(self, delta)
		local vx = 0
		local vy = 0

		if controls.left then
			vx = vx - speed
		end
		if controls.right then
			vx = vx + speed
		end
		if controls.up then
			vy = vy - speed
		end
		if controls.down then
			vy = vy + speed
		end

		if vx ~= 0 and vy ~= 0 then
			local inv = speed / sqrt(vx * vx + vy * vy)
			vx = vx * inv
			vy = vy * inv
		end

		if vx < 0 then
			self.flip = "horizontal"
		elseif vx > 0 then
			self.flip = "none"
		end

		self.vx = vx
		self.vy = vy
	end,

	on_damage = function(self)
		print("on damage " .. self.name)
	end,
}
