local scheduler = require("helpers/scheduler")

return scheduler.wrap({
	tilemap = "forest",

	overlay = { widgets = "hud" }, --, foreground = "mist" },

	particles = {
		{ name = "smoke", kind = "smoke", x = 50, y = 100 },
	},

	objects = {
		{ name = "player", kind = "player", x = 100, y = 100 },
		{ name = "robot", kind = "robot", x = 250, y = 150 },
		-- { name = "drone", kind = "drone", x = 180, y = 200 },
		--  { name = "demon", kind = "demon", x = 300, y = 100 },
		{ name = "tree1", kind = "tree", x = 400, y = 80 },
		{ name = "tree2", kind = "tree", x = 500, y = 250 },
		{ name = "tree3", kind = "tree", x = 150, y = 300 },
		{ name = "tree4", kind = "tree", x = 50, y = 200 },
		{ name = "tree5", kind = "tree", x = 350, y = 200 },
		{ name = "tree6", kind = "tree", x = 550, y = 130 },
		{ name = "tree7", kind = "tree", x = 220, y = 50 },
		{ name = "tree8", kind = "tree", x = 450, y = 320 },
	},

	on_enter = function(self)
		print("User: " .. user.persona)
		print("Friends:")
		for _, friend in ipairs(user.friends) do
			print("  " .. friend.name .. " (ID: " .. friend.id .. ")")
		end
	end,
})
