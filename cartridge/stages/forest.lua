local scheduler = require("helpers/scheduler")
local camera = require("helpers/camera")
local controls = require("helpers/controls")

local pressed = true

return scheduler.wrap({
	tilemap = "forest",

	minimap = {
		solid = { 90, 85, 95 },
		passable = { 195, 190, 185 },
		void = { 10, 10, 25 },
		player = { 140, 200, 255 },
		entity = { 255, 220, 130 },
	},

	overlay = { widgets = "hud" }, --, foreground = "mist" },

	particles = {
		{ name = "smoke", kind = "smoke", x = 50, y = 100 },
	},

	sounds = {
		{ name = "theme1", loop = true },
	},

	objects = {
		{ name = "player", kind = "player", x = 100, y = 100 },
		-- { name = "robot", kind = "robot", x = 250, y = 150 },
		-- { name = "drone", kind = "drone", x = 180, y = 200 },
		-- { name = "demon", kind = "demon", x = 300, y = 100 },
		{ name = "tree1", kind = "tree", x = 400, y = 80 },
		{ name = "tree2", kind = "tree", x = 500, y = 250 },
		{ name = "tree3", kind = "tree", x = 150, y = 300 },
		{ name = "tree4", kind = "tree", x = 50, y = 200 },
		{ name = "tree5", kind = "tree", x = 350, y = 200 },
		{ name = "tree6", kind = "tree", x = 550, y = 130 },
		{ name = "tree7", kind = "tree", x = 220, y = 50 },
		{ name = "tree8", kind = "tree", x = 450, y = 320 },
	},

	on_loop = function(self, delta)
		local down = controls.minimap
		if down and not pressed then
			pool.minimap.visible = not pool.minimap.visible
		end
		pressed = down
	end,

	on_camera = function(self)
		local player = pool.player
		if player then
			camera.update(player)
		end

		return camera.position()
	end,

	on_enter = function(self)
		pool.theme1:fade(0, 1, 500)
		pool.theme1:play()
	end,

	on_leave = function(self)
		pool.theme1:stop()
	end,
})
