local scheduler = require("helpers/scheduler")

return scheduler.wrap({
	tilemap = "forest",
	overlay = "hud",
	objects = {
		{ name = "player", kind = "player", x = 100, y = 100 },
		{ name = "robot", kind = "robot", x = 250, y = 150 },
		{ name = "drone", kind = "drone", x = 180, y = 200 },
		{ name = "demon", kind = "demon", x = 300, y = 100 },
	},
})
