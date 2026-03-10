local ticker = require("helpers/ticker")
local camera = require("helpers/camera")

return ticker.wrap({
	gravity = { 0, 980 },

	tilemap = "forest",

	objects = {
		{ name = "player", kind = "player" },
		{ name = "enemy1", kind = "enemy" },
		{ name = "enemy2", kind = "enemy" },
		{ name = "enemy3", kind = "enemy" },
		{ name = "enemy4", kind = "enemy" },
		{ name = "enemy5", kind = "enemy" },
		{ name = "enemy6", kind = "enemy" },
		{ name = "enemy7", kind = "enemy" },
		{ name = "enemy8", kind = "enemy" },
	},

	on_enter = function()
		director.overlay("hud")

		camera.set_bounds(0, 0, 120 * 16 - viewport.width, 68 * 16 - viewport.height)
		camera.configure({
			dead_zone_x = 16,
			dead_zone_y = 16,
			smoothing = 5,
			damping = 0.85,
			offset_x = 8,
			offset_y = 8,
		})

		pool.enemy1.position = { 200, 1200 }
		pool.enemy2.position = { 400, 800 }
		pool.enemy3.position = { 700, 750 }
		pool.enemy4.position = { 1000, 800 }
		pool.enemy5.position = { 1300, 700 }
		pool.enemy6.position = { 1500, 800 }
		pool.enemy7.position = { 1800, 750 }
		pool.enemy8.position = { 10, 900 }
	end,

	on_leave = function()
		camera.reset()
	end,

	on_paint = function()
		local player = pool.player
		return camera.update(player.x, player.y)
	end,
})
