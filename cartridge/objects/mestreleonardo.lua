local controls = require("helpers/controls")

local animations =
	{ "normal", "variation1", "variation2", "variation3", "variation4", "variation5", "variation6", "variation7" }
local index = 1

return {
	animation = {
		default = "normal",
		normal = {
			{ 126, 340, 126, 113, 177, 57, 480, 270, -1, 0, 0, 126, 113 },
		},
		variation1 = {
			{ 0, 225, 126, 113, 177, 57, 480, 270, -1, 0, 0, 126, 113 },
		},
		variation2 = {
			{ 0, 112, 126, 113, 177, 57, 480, 270, -1, 0, 0, 126, 113 },
		},
		variation3 = {
			{ 0, 0, 126, 112, 177, 58, 480, 270, -1, 0, 0, 126, 112 },
		},
		variation4 = {
			{ 126, 1, 122, 113, 181, 57, 480, 270, -1, 0, 0, 122, 113 },
		},
		variation5 = {
			{ 0, 338, 126, 115, 177, 55, 480, 270, -1, 0, 0, 126, 115 },
		},
		variation6 = {
			{ 126, 227, 126, 113, 177, 57, 480, 270, -1, 0, 0, 126, 113 },
		},
		variation7 = {
			{ 126, 114, 126, 113, 177, 57, 480, 270, -1, 0, 0, 126, 113 },
		},
	},

	select = function(self)
		self.selected = true
		self.action = controls.action
	end,

	unselect = function(self)
		self.selected = false
		self.action = false
	end,

	on_click = function(self)
		if self.selected then
			index = index % #animations + 1
			self.animation = animations[index]
		end
	end,

	on_loop = function(self, delta)
		local action = controls.action
		if self.selected and action and not self.action then
			index = index % #animations + 1
			self.animation = animations[index]
		end

		self.action = action
	end,
}
