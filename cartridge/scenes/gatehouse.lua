local navigation = require("toolkit/navigation")
local interaction = require("toolkit/interaction")
local navigator
local interact

return {
	objects = {
		{ name = "paperpiece", kind = "paperpiece", x = 0, y = 0 },
		{ name = "walkie", kind = "walkietalkie", x = 0, y = 0 },
	},

	on_enter = function()
		navigator = navigation.new({ pool.walkie })
		interact = interaction.new({ pool.walkie })

		pool.walkie.on_hover = function(object)
			navigation.select(object)
		end

		pool.walkie.on_unhover = function(object)
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
