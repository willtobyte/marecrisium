local ticker = require("helpers/ticker")

return ticker.wrap({
	background = "forest_sky",
	tilemap = "forest",

	objects = {
		{ name = "player", kind = "player" },
	},

	on_enter = function()
		ticker.every(100, function()
			print("on timer")
		end)
	end,
})
