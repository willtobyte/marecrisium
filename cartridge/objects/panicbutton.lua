local controls = require("helpers/controls")

return {
	animation = {
		default = "normal",

		["hover"] = {
			{ 36, 14, 40, 76, 335, 100, 480, 270, -1, 0, 0, 40, 76 },
		},
		["normal"] = {
			{ 76, 14, 40, 76, 335, 100, 480, 270, -1, 0, 0, 40, 76 },
		},
		["open"] = {
			{ 0, 0, 36, 90, 335, 86, 480, 270, -1, 0, 0, 36, 90 },
		},
	},

	select = function(self)
		self.selected = true
		self.action = controls.action
		self.dismiss = controls.dismiss
		self.animation = "hover"
		pool.alarmlight.animation = "normal"
	end,

	unselect = function(self)
		self.selected = false
		self.action = false
		self.dismiss = false
		self.animation = "normal"
		pool.alarmlight.animation = "normal"
	end,

	on_click = function(self)
		if self.selected then
			self.animation = "open"
			pool.alarmlight.animation = "on"
		end
	end,

	on_loop = function(self, delta)
		local action = controls.action
		local dismiss = controls.dismiss
		if self.selected and action and not self.action then
			self.animation = "open"
			pool.alarmlight.animation = "on"
		elseif self.selected and dismiss and not self.dismiss then
			self.animation = "hover"
			pool.alarmlight.animation = "normal"
		end

		self.action = action
		self.dismiss = dismiss
	end,
}
