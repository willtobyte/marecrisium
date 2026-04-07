local controls = require("helpers/controls")
local sqrt = math.sqrt

local speed = 99

return {
	body = "dynamic",

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
			{ 16, 0, 16, 24, 200, 2, 10, 12, 12 },
			{ 32, 0, 16, 24, 200, 2, 10, 12, 12 },
			{ 48, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},

	on_spawn = function(self)
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
			local im = speed / sqrt(vx * vx + vy * vy)
			vx = vx * im
			vy = vy * im
		end

		if vx < 0 then
			self.flip = flip.horizontal
		elseif vx > 0 then
			self.flip = flip.none
		end

		self.velocity_x = vx
		self.velocity_y = vy
	end,

	on_damage = function(self)
		print("on damage " .. self.name)
		-- foreground:bump()
	end,
}
