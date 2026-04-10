local agent = require("helpers/agent")
local scheduler = require("helpers/scheduler")

local function enemy(config)
	local chaser = agent.new(config)

	local spawner = config.on_spawn
	local loop = config.on_loop
	local patrol = config.on_patrol

	return {
		body = "dynamic",
		sleepable = true,

		animation = config.animation,

		on_spawn = function(self)
			self._touching = false
			self._chasing = false
			chaser:init(self)
			if patrol then
				scheduler.spawn(function()
					patrol(self, chaser, scheduler)
				end)
			end
			if spawner then
				spawner(self, chaser)
			end
		end,

		on_loop = function(self, delta)
			local player = pool.player
			if not player or not player.alive then
				self._chasing = false
				return
			end

			if self._touching then
				self.velocity_x = 0
				self.velocity_y = 0
				self._chasing = false
				return
			end

			local inrange = chaser:in_range(self, player)

			if inrange then
				self._chasing = true
				if loop then
					loop(self, delta, player, chaser)
				else
					chaser:chase(self, player, world)
				end
			else
				self._chasing = false
				if not patrol then
					self.velocity_x = 0
					self.velocity_y = 0
				end
			end
		end,

		on_collision_begin = function(self, name, kind)
			if kind == "player" then
				self._touching = true
				if config.on_collision_begin then
					config.on_collision_begin(self, chaser)
				end
				pool.player:damage()
			end
		end,

		on_collision_end = function(self, name, kind)
			if kind == "player" then
				self._touching = false
				chaser:reset(self)
			end
		end,
	}
end

return enemy
