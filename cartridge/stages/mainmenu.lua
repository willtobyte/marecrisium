local ticker = require("helpers/ticker")

local camera_x = 0
local camera_y = 0

local LERP = 5

return ticker.wrap({
	tilemap = "brownhill",

	objects = {
		{ name = "player", kind = "player" },
	},

	on_camera = function(self, delta)
		local player = pool.player
		local target_x = player.x - viewport.width / 2
		local target_y = player.y - viewport.height / 2

		target_x = math.max(0, math.min(target_x, tilemap.width - viewport.width))
		target_y = math.max(0, math.min(target_y, tilemap.height - viewport.height))

		local t = 1 - math.exp(-LERP * delta)
		camera_x = camera_x + (target_x - camera_x) * t
		camera_y = camera_y + (target_y - camera_y) * t

		return camera_x, camera_y, viewport.width, viewport.height
	end,

	on_enter = function()
		ticker.every(100, function()
			print("on timer")
		end)
	end,
})
