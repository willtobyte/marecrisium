local ticker = require("helpers/ticker")
local camera = require("helpers/camera")

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
		self.socket = WebSocket.new("localhost:8080")
		self.ping_subscription = self.socket:subscribe("health", function() end)

		ticker.every(10, function()
			self.ping_subscription:publish({ action = "ping" })
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
	end,

	on_leave = function(self)
		camera.reset()
		self.ping_subscription = nil
		self.socket = nil
	end,

	on_loop = function(self, delta)
		camera.update(pool.player.x, pool.player.y, delta)
	end,

	on_paint = function(self)
		return camera.position()
	end,
})
