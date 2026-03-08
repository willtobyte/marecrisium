local ticker = require("helpers/ticker")

return ticker.wrap({
	objects = {
		{ name = "player", kind = "player" },
		{ name = "enemy", kind = "enemy" },
	},

	on_enter = function()
		director.overlay("hud")

		ticker.every(100, function()
			print("on timer")
		end)
	end,
})
