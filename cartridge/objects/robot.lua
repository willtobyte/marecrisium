local enemy = require("helpers/enemy")

return enemy({
	detect_radius = 150,
	speed = 50,
	waypoint_reach = 10,
	path_interval = 30,
	probe_distance = 32,
	body_radius = 8,
	stall_threshold = 15,
	steer_blend = 0.2,

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},
})
