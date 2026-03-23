local controls = require("helpers/controls")
local sqrt = math.sqrt

local speed = 90

local topic = 0x0001

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

		self.websocket = WebSocket.new("localhost:8080")
		self.subscription = self.websocket:subscribe(topic, function(data)
			print("received: " .. tostring(data))
		end)
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
			if self.subscription then
				self.subscription:publish({ action = "down", x = self.x, y = self.y })
			end
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
}
