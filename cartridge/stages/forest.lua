local ticker = require("helpers/ticker")
local camera = require("helpers/camera")

local print = print
local WebSocket = WebSocket
local keyboard = keyboard

return ticker.wrap({
	gravity = { 0, 980 },

	tilemap = "forest",

	objects = {
		{ name = "player", kind = "player" },
		{ name = "enemy1", kind = "enemy", x = 200, y = 1200 },
		{ name = "enemy2", kind = "enemy", x = 400, y = 800 },
		{ name = "enemy3", kind = "enemy", x = 700, y = 750 },
		{ name = "enemy4", kind = "enemy", x = 1000, y = 800 },
		{ name = "enemy5", kind = "enemy", x = 1300, y = 700 },
		{ name = "enemy6", kind = "enemy", x = 1500, y = 800 },
		{ name = "enemy7", kind = "enemy", x = 1800, y = 750 },
		{ name = "enemy8", kind = "enemy", x = 10, y = 900 },
	},

	sounds = {},

	particles = {
		{ name = "smoke1", kind = "smoke", x = 420, y = 900 },
		{ name = "smoke2", kind = "smoke", x = 470, y = 920 },
		{ name = "smoke3", kind = "smoke", x = 420, y = 900 },
	},

	on_enter = function(self)
		ticker.every(50, function()
			print("5 seconds elapsed")
		end)

		director.overlay = "hud"

		camera.set_bounds(0, 0, 120 * 16 - viewport.width, 68 * 16 - viewport.height)

		camera.configure({
			dead_zone_x = 16,
			dead_zone_y = 16,
			smoothing = 5,
			damping = 0.85,
			offset_x = 8,
			offset_y = 8,
		})

		self.ws = WebSocket.new("localhost:8080")

		self.chat_echo = self.ws:subscribe("chat_echo", function(data)
			print("[echo] " .. tostring(data.text))
		end)

		self.chat = self.ws:subscribe("chat", function() end)

		self.r_was_down = false
	end,

	on_leave = function(self)
		camera.reset()
		self.chat_echo = nil
		self.chat = nil
		self.ws = nil
		self.r_was_down = false
	end,

	on_loop = function(self)
		if keyboard.r then
			if not self.r_was_down then
				self.r_was_down = true
				self.chat:publish({ text = "hello world" })
				print("[sent] hello world")
			end
		else
			self.r_was_down = false
		end
	end,

	on_paint = function()
		return camera.update(pool.player.x, pool.player.y)
	end,
})
