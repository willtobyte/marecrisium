local enemy = require("helpers/enemy")

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
})
