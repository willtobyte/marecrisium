local camera = require("helpers/camera")

return {
	tilemap = "forest",

	objects = {
		{ name = "player", kind = "player" },
		{ name = "enemy1", kind = "enemy", x = 3200, y = 2000 },
	},

	sounds = {},

	on_enter = function(self)
		director.overlay = "hud"

		camera.set_bounds(0, 0, 480 * 16 - viewport.width, 272 * 16 - viewport.height)

		camera.configure({
			dead_zone_x = 8,
			dead_zone_y = 8,
			smoothing = 6,
			damping = 0.88,
			offset_x = 0,
			offset_y = 0,
		})
	end,

	on_leave = function(self)
		camera.reset()
	end,

	on_loop = function(self, delta)
		camera.update(pool.player, delta)
	end,

	on_camera = function(self)
		return camera.position()
	end,
}
