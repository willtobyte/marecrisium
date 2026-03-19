local agent = require("helpers/agent")

local chaser = agent.new({
	detect_radius = 150,
	speed = 50,
	waypoint_reach = 10,
	path_interval = 30,
	probe_distance = 32,
	body_radius = 8,
	stall_threshold = 15,
	steer_blend = 0.2,
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
		self.animation = "idle"
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
			chaser:stop(self)
			pool.player:damage()
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
