local navigation = require("helpers/navigation")
local navigator

return {
	objects = {
		{ name = "walkie_northwest", kind = "walkietalkie", x = 0, y = 0 },
		{ name = "walkie_northeast", kind = "walkietalkie", x = 448, y = 0 },
		{ name = "walkie_southwest", kind = "walkietalkie", x = 0, y = 238 },
		{ name = "walkie_southeast", kind = "walkietalkie", x = 448, y = 238 },
	},

	on_enter = function()
		navigator = navigation.new({
			pool.walkie_northwest,
			pool.walkie_northeast,
			pool.walkie_southwest,
			pool.walkie_southeast,
		})
		timer:singleshot(60000, function()
			error("Intentional error after 60 seconds")
		end)
	end,

	on_leave = function()
		navigator:clear()
		navigator = nil
	end,

	on_loop = function()
		navigator:update()
	end,
}
