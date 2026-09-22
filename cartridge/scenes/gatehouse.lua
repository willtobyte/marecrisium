local navigation = require("toolkit/navigation")
local interaction = require("toolkit/interaction")
local navigator
local interact

return {
	objects = {
		{ name = "paperpiece", kind = "paperpiece", x = 0, y = 0 },
		{ name = "walkietalkie", kind = "walkietalkie", x = 0, y = 0 },
		{ name = "pda", kind = "pda", x = 0, y = 0 },
		{ name = "idmanual", kind = "idmanual", x = 0, y = 0 },
		{ name = "button", kind = "button", x = 0, y = 0 },
		{ name = "alarmlight", kind = "alarmlight", x = 0, y = 0 },
	},

	on_enter = function()
		navigator = navigation.new({ pool.walkietalkie, pool.pda, pool.idmanual, pool.button })
		interact = interaction.new({ pool.walkietalkie, pool.pda, pool.idmanual, pool.button })

		pool.walkietalkie.on_hover = function(object)
			print("ON HOVER")
			navigation.select(object)
		end

		pool.walkietalkie.on_unhover = function(object)
			print("ON UNNNNHOVER")
			navigation.unselect(object)
		end

		pool.pda.on_hover = function(object)
			navigation.select(object)
		end

		pool.pda.on_unhover = function(object)
			navigation.unselect(object)
		end

		pool.idmanual.on_hover = function(object)
			navigation.select(object)
		end

		pool.idmanual.on_unhover = function(object)
			navigation.unselect(object)
		end

		pool.button.on_hover = function(object)
			navigation.select(object)
		end

		pool.button.on_unhover = function(object)
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
