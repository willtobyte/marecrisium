local enemy = require("helpers/enemy")
local sin = math.sin

local HOVER_SPEED = 15
local HOVER_DRIFT = 30
local HOVER_HALF = 20
local HOVER_PAUSE = 10

return enemy({
	radius = 250,
	speed = 108,
	reach = 12,
	interval = 12,
	probe = 40,
	body_radius = 5,
	threshold = 8,
	blend = 0.5,

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},

	on_patrol = function(self, chaser, scheduler)
		local phase = 0
		while self.alive do
			if not self._chasing then
				self.velocity_x = sin(phase) * HOVER_DRIFT
				self.velocity_y = HOVER_SPEED
			end
			scheduler.wait(HOVER_HALF)

			if not self._chasing then
				self.velocity_x = sin(phase) * HOVER_DRIFT
				self.velocity_y = -HOVER_SPEED
			end
			scheduler.wait(HOVER_HALF)

			if not self._chasing then
				self.velocity_x = 0
				self.velocity_y = 0
			end
			scheduler.wait(HOVER_PAUSE)

			phase = phase + 1
		end
	end,
})
