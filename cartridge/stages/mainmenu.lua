return {
	objects = {
		{ name = "player", kind = "player" },
		{ name = "enemy", kind = "enemy" },
	},

	-- sounds = { "theme" },

	on_enter = function()
		-- pool.theme.loop = true
		-- pool.theme:play()
	end,

	on_loop = function(self, delta) end,

	on_leave = function() end,
}
