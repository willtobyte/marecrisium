local agent = require("helpers/agent")

local chaser = agent.new({
	detect_radius = 250,
	speed = 108,
	waypoint_reach = 12,
	path_interval = 12,
	probe_distance = 40,
	body_radius = 5,
	stall_threshold = 8,
	steer_blend = 0.5,
})

return {
	body = "dynamic",
	sleepable = true,

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},

	on_spawn = function(self)
		self._touching_player = false
		chaser:init(self)
	end,

	on_loop = function(self, delta)
		local player = pool.player
		if not player or not player.alive then
			return
		end

		if self._touching_player then
			self.vx = 0
			self.vy = 0
			return
		end

		chaser:chase(self, player, world)
	end,

	on_collision_begin = function(self, name, kind)
		if kind == "player" then
			self._touching_player = true
			pool.player:damage()
		end
	end,

	on_collision_end = function(self, name, kind)
		if kind == "player" then
			self._touching_player = false
			chaser:reset(self)
		end
	end,
}
