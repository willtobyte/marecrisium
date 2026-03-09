local ticker = require("helpers/ticker")

local camera_x = 0
local camera_y = 0

return ticker.wrap({
	tilemap = "forest",

	objects = {
		{ name = "player", kind = "player" },
	},

	on_enter = function()
		director.overlay("hud")
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

		tilemap:draw(camera_x, camera_y, viewport.width, viewport.height)

		return camera_x, camera_y
	end,
})
