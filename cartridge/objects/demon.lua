local enemy = require("helpers/enemy")
local scheduler = require("helpers/scheduler")
local spawner = require("helpers/spawner")
local cos = math.cos
local sin = math.sin
local atan2 = math.atan2

local ATTACK_RANGE_SQUARED = 200 * 200
local KEEP_DISTANCE_SQUARED = 140 * 140
local COOLDOWN_TICKS = 12
local PATROL_SPEED = 15
local PATROL_WALK = 50
local PATROL_PAUSE = 60
local FIREBALL_OFFSET = 18

return enemy({
	radius = 250,
	speed = 60,
	reach = 10,
	interval = 25,
	probe = 32,
	body_radius = 6,
	threshold = 10,
	blend = 0.3,

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},

	on_spawn = function(self)
		self._ready = true
		self._fireball_id = 0
	end,

	on_patrol = function(self, chaser, scheduler)
		local direction = 1
		while self.alive do
			if not self._chasing then
				self.flip = direction < 0 and flip.horizontal or flip.none
				self.velocity_x = PATROL_SPEED * direction
				self.velocity_y = 0
			end
			scheduler.wait(PATROL_WALK)

			if not self._chasing then
				self.velocity_x = 0
				self.velocity_y = 0
			end
			scheduler.wait(PATROL_PAUSE)

			direction = -direction
		end
	end,

	on_collision_begin = function(self, chaser)
		chaser:stop(self)
	end,

	on_loop = function(self, delta, player, chaser)
		local origin_x = self.center_x
		local origin_y = self.center_y
		local horizontal = player.center_x - origin_x
		local vertical = player.center_y - origin_y
		local distance_squared = horizontal * horizontal + vertical * vertical

		if distance_squared <= ATTACK_RANGE_SQUARED and distance_squared > 0 then
			self.velocity_x = 0
			self.velocity_y = 0

			local angle = atan2(vertical, horizontal)
			self.flip = horizontal < 0 and flip.horizontal or horizontal > 0 and flip.none or self.flip

			if self._ready then
				self._ready = false
				self._fireball_id = self._fireball_id + 1
				local name = self.name .. "_fireball_" .. self._fireball_id
				local spawn_x = origin_x + cos(angle) * FIREBALL_OFFSET
				local spawn_y = origin_y + sin(angle) * FIREBALL_OFFSET
				local fireball = spawner.at_center(name, "fireball", spawn_x, spawn_y)
				if fireball then
					fireball._angle = angle
				end
				scheduler.spawn(function()
					scheduler.wait(COOLDOWN_TICKS)
					self._ready = true
				end)
			end

			if distance_squared < KEEP_DISTANCE_SQUARED then
				local retreat = atan2(-vertical, -horizontal)
				self.velocity_x = cos(retreat) * chaser.speed
				self.velocity_y = sin(retreat) * chaser.speed
			end

			return
		end

		chaser:chase(self, player, world)
	end,
})
