local controls = require("helpers/controls")
local sqrt = math.sqrt

local speed = 99
local facing = "south"

return {
	body = "dynamic",

	animation = {
		east = {
			{ 1, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 35, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 69, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 103, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		north = {
			{ 137, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 171, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 137, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 205, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		south = {
			{ 239, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 273, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 307, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 341, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		idle_east = {
			{ 1, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		idle_north = {
			{ 137, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		idle_south = {
			{ 239, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
	},

	on_spawn = function(self)
		self.x = 20
		self.y = 20
	end,

	on_loop = function(self, delta)
		local vx = (controls.right and speed or 0) - (controls.left and speed or 0)
		local vy = (controls.down and speed or 0) - (controls.up and speed or 0)

		if vx ~= 0 and vy ~= 0 then
			local im = speed / sqrt(vx * vx + vy * vy)
			vx, vy = vx * im, vy * im
		end

		if vx ~= 0 then
			self.flip = vx < 0 and flip.horizontal or flip.none
		end

		if vy > 0 then
			facing = "south"
		elseif vy < 0 then
			facing = "north"
		elseif vx ~= 0 then
			facing = "east"
		end

		self.animation = (vx ~= 0 or vy ~= 0) and facing or ("idle_" .. facing)

		self.velocity_x, self.velocity_y = vx, vy
	end,

	on_damage = function(self)
		print("on damage " .. self.name)
	end,
}
