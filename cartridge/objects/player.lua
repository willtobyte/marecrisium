local controls = require("helpers/controls")
local sqrt = math.sqrt

local speed = 99
local facing = "south"
local idle = { south = "idle.south", north = "idle.north", east = "idle.east" }

return {
	body = "dynamic",

	animation = {
		["east"] = {
			{ 1, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 35, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 69, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 103, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		["north"] = {
			{ 137, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 171, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 137, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 205, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		["south"] = {
			{ 239, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 273, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 307, 1, 32, 48, 100, 8, 34, 16, 14 },
			{ 341, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		["idle.east"] = {
			{ 1, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		["idle.north"] = {
			{ 137, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
		["idle.south"] = {
			{ 239, 1, 32, 48, 100, 8, 34, 16, 14 },
		},
	},

	on_spawn = function(self)
		self.x = 0
		self.y = 0
	end,

	on_loop = function(self, delta)
		local control_x = (controls.right and 1 or 0) - (controls.left and 1 or 0)
		local control_y = (controls.down and 1 or 0) - (controls.up and 1 or 0)

		local velocity_x = control_x * speed
		local velocity_y = control_y * speed
		if velocity_x ~= 0 and velocity_y ~= 0 then
			local inverse = speed / sqrt(velocity_x * velocity_x + velocity_y * velocity_y)
			velocity_x, velocity_y = velocity_x * inverse, velocity_y * inverse
		end

		if velocity_x ~= 0 then
			self.flip = velocity_x < 0 and flip.horizontal or flip.none
		end

		if velocity_y > 0 then
			facing = "south"
		elseif velocity_y < 0 then
			facing = "north"
		elseif velocity_x ~= 0 then
			facing = "east"
		end

		self.animation = (velocity_x ~= 0 or velocity_y ~= 0) and facing or idle[facing]

		self.velocity_x, self.velocity_y = velocity_x, velocity_y
	end,

	on_damage = function(self)
		print("on damage " .. self.name)
	end,
}
