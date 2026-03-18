local scheduler = require("helpers/scheduler")

return scheduler.wrap({
	tilemap = "forest",
	overlay = "hud",
	objects = {
		{ name = "player", kind = "player", x = 100, y = 100 },
		{ name = "enemy", kind = "enemy", x = 200, y = 150 },
	},
})
