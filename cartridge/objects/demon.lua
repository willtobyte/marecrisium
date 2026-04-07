local enemy = require("helpers/enemy")
local cos = math.cos
local sin = math.sin
local atan2 = math.atan2

local ATTACK_RANGE_SQUARED = 120 * 120
local ATTACK_COOLDOWN = 1.2
local KEEP_DISTANCE_SQUARED = 80 * 80

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
		self._cooldown = 0
		self._fireball_id = 0
	end,

	on_collision_begin = function(self, chaser)
		chaser:stop(self)
	end,

	on_loop = function(self, delta, player, chaser)
		self._cooldown = self._cooldown - delta

		local dx = player.x - self.x
		local dy = player.y - self.y
		local dsq = dx * dx + dy * dy

		if dsq <= ATTACK_RANGE_SQUARED and dsq > 0 then
			self.velocity_x = 0
			self.velocity_y = 0

			local angle = atan2(dy, dx)
			self.flip = dx < 0 and flip.horizontal or dx > 0 and flip.none or self.flip

			if self._cooldown <= 0 then
				self._cooldown = ATTACK_COOLDOWN
				self._fireball_id = self._fireball_id + 1
				local name = self.name .. "_fireball_" .. self._fireball_id
				local fx = self.x + cos(angle) * 12
				local fy = self.y + sin(angle) * 12
				world.spawn(name, "fireball", fx, fy)
				if pool[name] then
					pool[name]._angle = angle
				end
			end

			if dsq < KEEP_DISTANCE_SQUARED then
				local retreat = atan2(-dy, -dx)
				self.velocity_x = cos(retreat) * chaser.speed
				self.velocity_y = sin(retreat) * chaser.speed
			end

			return
		end

		chaser:chase(self, player, world)
	end,
})
