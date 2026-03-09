local ticker = require("helpers/ticker")

local camera_x = 0
local camera_y = 0

return ticker.wrap({
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

		pool.enemy1.position = { 200, 1200 }
		pool.enemy2.position = { 400, 800 }
		pool.enemy3.position = { 700, 750 }
		pool.enemy4.position = { 1000, 800 }
		pool.enemy5.position = { 1300, 700 }
		pool.enemy6.position = { 1500, 800 }
		pool.enemy7.position = { 1800, 750 }
		pool.enemy8.position = { 300, 500 }
	end,

	on_paint = function(self)
		local player = pool.player

		local half_w = viewport.width * 0.5
		local half_h = viewport.height * 0.5

		local target_x = player.x - half_w + 8
		local target_y = player.y - half_h + 8

		local max_x = 120 * 16 - viewport.width
		local max_y = 68 * 16 - viewport.height

		if target_x < 0 then
			target_x = 0
		end
		if target_y < 0 then
			target_y = 0
		end
		if target_x > max_x then
			target_x = max_x
		end
		if target_y > max_y then
			target_y = max_y
		end

		camera_x = camera_x + (target_x - camera_x) * 0.1
		camera_y = camera_y + (target_y - camera_y) * 0.1

		return camera_x, camera_y
	end,
})
