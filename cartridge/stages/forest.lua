local scheduler = require("helpers/scheduler")

return scheduler.wrap({
	tilemap = "forest",
	overlay = "hud",
	particles = {
		{ name = "smoke", kind = "smoke", x = 50, y = 100 },
	},
	objects = {
		{ name = "player", kind = "player", x = 100, y = 100 },
		-- { name = "robot", kind = "robot", x = 250, y = 150 },
		-- { name = "drone", kind = "drone", x = 180, y = 200 },
		{ name = "demon", kind = "demon", x = 300, y = 100 },
	},
	on_enter = function(self)
		print("User: " .. user.persona)
		print("Friends:")
		for _, friend in ipairs(user.friends) do
			print("  " .. friend.name .. " (ID: " .. friend.id .. ")")
		end
	end,

	on_loop = function(self, dt)
		if mouse.button == 1 then
			local x, y = mouse.position()
			local hits = world.at(x, y)
			for _, obj in ipairs(hits) do
				print("at " .. x .. "," .. y .. ": " .. obj.name)
			end
		end
	end,

	on_mouse_down = function(x, y, button)
		print("stage on_mouse_down miss " .. button .. " at " .. x .. "," .. y)
	end,

	on_mouse_up = function(x, y, button)
		print("stage on_mouse_up miss " .. button .. " at " .. x .. "," .. y)
	end,
})
