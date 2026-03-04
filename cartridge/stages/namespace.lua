return {
	objects = {
		{ name = "player", kind = "player" },
		{ name = "enemy", kind = "enemy" },
	},

	on_enter = function() end,

	on_loop = function(self, delta)
		local speed = 100 * delta
		if keyboard.w then
			pool.player.y = pool.player.y - speed
		end
		if keyboard.s then
			pool.player.y = pool.player.y + speed
		end
		if keyboard.a then
			pool.player.x = pool.player.x - speed
		end
		if keyboard.d then
			pool.player.x = pool.player.x + speed
		end
	end,

	on_leave = function() end,
}
