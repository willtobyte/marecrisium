local controls = require("helpers/controls")
local sqrt = math.sqrt

local speed = 99
local facing = "south"

return {
	body = "dynamic",

	animation = {
		east = {
			{ 1, 352, 24, 42, 100, 3, 30, 16, 14, 5, 4 },
			{ 1, 309, 25, 41, 100, 4, 29, 16, 14, 4, 5 },
			{ 1, 396, 24, 42, 100, 3, 30, 16, 14, 5, 4 },
			{ 1, 440, 24, 41, 100, 3, 29, 16, 14, 5, 5 },
		},
		north = {
			{ 1, 1, 26, 42, 100, 5, 30, 16, 14, 3, 4 },
			{ 1, 45, 26, 42, 100, 5, 29, 16, 14, 3, 5 },
			{ 1, 1, 26, 42, 100, 5, 30, 16, 14, 3, 4 },
			{ 1, 89, 26, 42, 100, 5, 29, 16, 14, 3, 5 },
		},
		south = {
			{ 1, 133, 26, 42, 100, 5, 31, 16, 14, 3, 3 },
			{ 1, 177, 26, 42, 100, 5, 30, 16, 14, 3, 4 },
			{ 1, 221, 26, 42, 100, 5, 31, 16, 14, 3, 3 },
			{ 1, 265, 26, 42, 100, 5, 30, 16, 14, 3, 4 },
		},
		idle_east = {
			{ 1, 352, 24, 42, 100, 3, 30, 16, 14, 5, 4 },
		},
		idle_north = {
			{ 1, 1, 26, 42, 100, 5, 30, 16, 14, 3, 4 },
		},
		idle_south = {
			{ 1, 133, 26, 42, 100, 5, 31, 16, 14, 3, 3 },
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

		if vy > 0 then
			facing = "south"
			self.animation = "south"
		elseif vy < 0 then
			facing = "north"
			self.animation = "north"
		elseif vx ~= 0 then
			facing = "east"
			self.animation = "east"
		else
			self.animation = "idle_" .. facing
		end

		self.velocity_x = vx
		self.velocity_y = vy
	end,

	on_damage = function(self)
		print("on damage " .. self.name)
	end,
}
