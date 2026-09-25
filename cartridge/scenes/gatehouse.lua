local navigation = require("toolkit/navigation")
local interaction = require("toolkit/interaction")
local navigator
local interact

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
		navigator = navigation.new({ pool.walkietalkie, pool.datapad, pool.handbook, pool.panicbutton })
		interact = interaction.new({ pool.walkietalkie, pool.datapad, pool.handbook, pool.panicbutton })

		pool.walkietalkie.on_hover = function(object)
			print("ON HOVER")
			navigation.select(object)
		end

		pool.walkietalkie.on_unhover = function(object)
			print("ON UNNNNHOVER")
			navigation.unselect(object)
		end

		pool.datapad.on_hover = function(object)
			navigation.select(object)
		end

		pool.datapad.on_unhover = function(object)
			navigation.unselect(object)
		end

		pool.handbook.on_hover = function(object)
			navigation.select(object)
		end

		pool.handbook.on_unhover = function(object)
			navigation.unselect(object)
		end

		pool.panicbutton.on_hover = function(object)
			navigation.select(object)
		end

		pool.panicbutton.on_unhover = function(object)
			navigation.unselect(object)
		end
	end,

	on_leave = function()
		interact:clear()
		navigator:clear()
		navigator = nil
		interact = nil
	end,

	on_loop = function()
		navigator:update()
		interact:update()
	end,
}
