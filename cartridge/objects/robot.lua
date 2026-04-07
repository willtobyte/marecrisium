local enemy = require("helpers/enemy")

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
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},
})
