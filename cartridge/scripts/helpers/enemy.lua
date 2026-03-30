local agent = require("helpers/agent")

local function enemy(config)
	local chaser = agent.new(config)

	local custom_on_spawn = config.on_spawn
	local custom_on_loop = config.on_loop

	return {
		body = "dynamic",
		sleepable = true,

		animation = config.animation,

		on_spawn = function(self)
			self._touching_player = false
			chaser:init(self)
			if custom_on_spawn then
				custom_on_spawn(self, chaser)
			end
		end,

		on_loop = function(self, delta)
			local player = pool.player
			if not player or not player.alive then
				return
			end

			if self._touching_player then
				self.velocity_x = 0
				self.velocity_y = 0
				return
			end

			if custom_on_loop then
				custom_on_loop(self, delta, player, chaser)
			else
				chaser:chase(self, player, world)
			end
		end,

		on_collision_begin = function(self, name, kind)
			if kind == "player" then
				self._touching_player = true
				if config.on_collision_begin then
					config.on_collision_begin(self, chaser)
				end
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
end

return enemy
