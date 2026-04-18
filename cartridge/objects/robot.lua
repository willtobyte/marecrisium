local enemy = require("helpers/enemy")

local PATROL_SPEED = 20
local PATROL_WALK = 60
local PATROL_PAUSE = 40

return enemy({
	radius = 150,
	speed = 50,
	reach = 10,
	interval = 30,
	probe = 32,
	body_radius = 8,
	threshold = 15,
	blend = 0.2,

	animation = {
		idle = {
			{ 0, 0, 16, 24, 0, 0, 200, 2, 10, 12, 12 },
		},
	},

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
})
