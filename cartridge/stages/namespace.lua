return {
	objects = { "player" },
	on_enter = function()
		-- Called once when the stage is entered
	end,

	on_loop = function(self, delta)
		-- Called every frame with the elapsed time since the last frame
		-- Access objects via pool, e.g. pool.player.x, pool.player.y
	end,

	on_leave = function()
		-- Called once when the stage is left
	end,
}
