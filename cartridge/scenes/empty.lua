local navigation = require("helpers/navigation")
local navigator

return {
	objects = {
		{ name = "walkie_northwest", kind = "walkietalkie", x = 0, y = 0 },
		{ name = "walkie_northeast", kind = "walkietalkie", x = 448, y = 0 },
		{ name = "walkie_southwest", kind = "walkietalkie", x = 0, y = 238 },
		{ name = "walkie_southeast", kind = "walkietalkie", x = 448, y = 238 },
		{ name = "door", kind = "door", x = 0, y = 0 },
	},

	on_enter = function()
		navigator = navigation.new({
			pool.walkie_northwest,
			pool.walkie_northeast,
			pool.walkie_southwest,
			pool.walkie_southeast,
		})
	end,

	on_leave = function()
		navigator:clear()
		navigator = nil
	end,

	on_loop = function()
		navigator:update()
	end,
}
