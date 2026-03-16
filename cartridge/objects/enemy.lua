local scheduler = require("helpers/scheduler")

local speed = 30

return {
	body = "kinematic",
	cullable = true,

	animation = {
		running = {
			{ 0, 0, 16, 16, 200, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
		self.direction = 1

		scheduler.spawn(function()
			while self.alive do
				self.direction = 1
				self.flip = "none"
				scheduler.wait(60)

				self.direction = -1
				self.flip = "horizontal"
				scheduler.wait(60)
			end
		end)
	end,

	on_collision_begin = function(self, name, kind, normal_x, normal_y)
		if kind ~= "player" or not normal_x then
			return
		end

		if normal_y > 0.5 then
			print("collision with " .. name .. " from bottom")
			pool.player:damage()
		elseif normal_y < -0.5 then
			print("collision with " .. name .. " from top")
			pool.player:damage()
		elseif normal_x > 0.5 then
			print("collision with " .. name .. " from right")
			pool.player:damage()
		elseif normal_x < -0.5 then
			print("collision with " .. name .. " from left")
			pool.player:damage()
		end
	end,

	on_loop = function(self, delta)
		self.speed_x = self.direction * speed
		self.x = self.x + self.speed_x * delta
	end,
}
