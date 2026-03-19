local agent = require("helpers/agent")
local cos = math.cos
local sin = math.sin
local atan2 = math.atan2

local ATTACK_RANGE_SQ = 120 * 120
local ATTACK_COOLDOWN = 60
local KEEP_DISTANCE_SQ = 80 * 80

local chaser = agent.new({
	detect_radius = 250,
	speed = 60,
	waypoint_reach = 10,
	path_interval = 25,
	probe_distance = 32,
	body_radius = 6,
	stall_threshold = 10,
	steer_blend = 0.3,
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
		self._cooldown = 0
		self._fireball_id = 0
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

		self._cooldown = self._cooldown - 1

		local dx = player.x - self.x
		local dy = player.y - self.y
		local dist_sq = dx * dx + dy * dy

		if dist_sq <= ATTACK_RANGE_SQ and dist_sq > 0 then
			self.vx = 0
			self.vy = 0

			local angle = atan2(dy, dx)
			self.flip = dx < 0 and "horizontal" or dx > 0 and "none" or self.flip

			if self._cooldown <= 0 then
				self._cooldown = ATTACK_COOLDOWN
				self._fireball_id = self._fireball_id + 1
				local name = self.name .. "_fb_" .. self._fireball_id
				local fx = self.x + cos(angle) * 12
				local fy = self.y + sin(angle) * 12
				world.spawn(name, "fireball", fx, fy)
				if pool[name] then
					pool[name]._angle = angle
				end
			end

			if dist_sq < KEEP_DISTANCE_SQ then
				local retreat_angle = atan2(-dy, -dx)
				self.vx = cos(retreat_angle) * chaser.speed
				self.vy = sin(retreat_angle) * chaser.speed
			end

			return
		end

		chaser:chase(self, player, world)
	end,

	on_collision_begin = function(self, name, kind)
		if kind == "player" then
			self._touching_player = true
			chaser:stop(self)
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
