local navigation = require("helpers/navigation")
local navigator

return {
	-- objects = {
	-- 	{ name = "walkie_center", kind = "walkietalkie", x = 224, y = 119 },
	-- 	{ name = "walkie_north", kind = "walkietalkie", x = 224, y = 39 },
	-- 	{ name = "walkie_east", kind = "walkietalkie", x = 384, y = 119 },
	-- 	{ name = "walkie_south", kind = "walkietalkie", x = 224, y = 199 },
	-- 	{ name = "walkie_west", kind = "walkietalkie", x = 64, y = 119 },
	-- },

	-- on_enter = function()
	-- 	navigator = navigation.new({
	-- 		pool.walkie_center,
	-- 		pool.walkie_north,
	-- 		pool.walkie_east,
	-- 		pool.walkie_south,
	-- 		pool.walkie_west,
	-- 	})
	-- end,

	-- on_leave = function()
	-- 	navigator:clear()
	-- 	navigator = nil
	-- end,

	-- on_loop = function()
	-- 	navigator:update()
	-- end,
}
