local enemy = require("helpers/enemy")

return enemy({
	detect_radius = 250,
	speed = 108,
	waypoint_reach = 12,
	path_interval = 12,
	probe_distance = 40,
	body_radius = 5,
	stall_threshold = 8,
	steer_blend = 0.5,

	animation = {
		idle = {
			{ 0, 0, 16, 24, 200, 2, 10, 12, 12 },
		},
	},
})
