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
		self.x = 0
		self.y = 0
	end,

	on_loop = function(self, delta)
		local velocity_x = 0
		local velocity_y = 0

		if controls.left then
			velocity_x = velocity_x - speed
		end
		if controls.right then
			velocity_x = velocity_x + speed
		end
		if controls.up then
			velocity_y = velocity_y - speed
		end
		if controls.down then
			velocity_y = velocity_y + speed
		end

		if velocity_x ~= 0 and velocity_y ~= 0 then
			local inverse_magnitude = speed / sqrt(velocity_x * velocity_x + velocity_y * velocity_y)
			velocity_x = velocity_x * inverse_magnitude
			velocity_y = velocity_y * inverse_magnitude
		end

		if velocity_x < 0 then
			self.flip = "horizontal"
		elseif velocity_x > 0 then
			self.flip = "none"
		end

		self.vx = velocity_x
		self.vy = velocity_y
	end,

	on_damage = function(self)
		print("on damage " .. self.name)
		-- foreground:bump()
	end,

	on_mouse_down = function(self, x, y, button)
		print("on_mouse_down " .. self.name .. " " .. button .. " at " .. x .. "," .. y)
	end,

	on_mouse_up = function(self, x, y, button)
		error("mouse up")
		print("on_mouse_up " .. self.name .. " " .. button .. " at " .. x .. "," .. y)
	end,

	on_hover = function(self)
		print("on_hover " .. self.name)
	end,

	on_unhover = function(self)
		print("on_unhover " .. self.name)
	end,
}
