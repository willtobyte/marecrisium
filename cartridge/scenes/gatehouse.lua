local navigation = require("toolkit/navigation")
local navigator

return {
	objects = {
		{ name = "paperpiece", kind = "paperpiece", x = 0, y = 0 },
		{ name = "walkietalkie", kind = "walkietalkie", x = 0, y = 0 },
		{ name = "datapad", kind = "datapad", x = 0, y = 0 },
		{ name = "handbook", kind = "handbook", x = 0, y = 0 },
		{ name = "panicbutton", kind = "panicbutton", x = 0, y = 0 },
		{ name = "alarmlight", kind = "alarmlight", x = 0, y = 0 },
	},

	on_enter = function()
		local objects = { pool.walkietalkie, pool.datapad, pool.handbook, pool.panicbutton }
		navigator = navigation.new(objects)

		for _, object in ipairs(objects) do
			object:on_hover(navigation.select)
			object:on_unhover(navigation.unselect)
		end
	end,

	on_leave = function()
		navigator:clear()
		navigator = nil
	end,

	on_loop = function()
		navigator:update()
	end,
}
