local navigation = require("toolkit/navigation")
local interaction = require("toolkit/interaction")
local navigator
local interact

return {
	objects = {
		{ name = "paperpiece", kind = "paperpiece", x = 0, y = 0 },
		{ name = "walkietalkie", kind = "walkietalkie", x = 0, y = 0 },
	},

	on_enter = function()
		navigator = navigation.new({ pool.walkietalkie })
		interact = interaction.new({ pool.walkietalkie })

		pool.walkietalkie.on_hover = function(object)
			navigation.select(object)
		end

		pool.walkietalkie.on_unhover = function(object)
			navigation.unselect(object)
		end
	end,

	on_leave = function()
		navigator:clear()
		interact:clear()
		navigator = nil
		interact = nil
	end,

	on_loop = function()
		navigator:update()
		interact:update()
	end,
}
