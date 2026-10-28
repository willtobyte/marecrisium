local controls = require("helpers/controls")
local pullable = {
	duration = 0.2,
}

return {
	animation = {
		default = "idle",
		hover = {
			{ 10, 56, 10, 133, 56, 0, 480, 270, -1, 0, 0, 10, 133 },
		},
		idle = {
			{ 20, 56, 10, 133, 56, 0, 480, 270, -1, 0, 0, 10, 133 },
		},
		pulled = {
			{ 0, 0, 10, 189, 56, 0, 480, 270, -1, 0, 0, 10, 189 },
		},
	},

	select = function(self)
		self.selected = true
		self.action = controls.action
		if not self.pulled then
			self.animation = "hover"
		end
	end,

	unselect = function(self)
		self.selected = false
		self.action = false
		if not self.pulled then
			self.animation = "idle"
		end
	end,

	on_click = function(self)
		if self.selected then
			self.animation = "pulled"
			self.pulled = pullable.duration
		end
	end,

	on_loop = function(self, delta)
		local action = controls.action
		if self.selected and action and not self.action then
			self.animation = "pulled"
			self.pulled = pullable.duration
		elseif self.pulled then
			self.pulled = self.pulled - delta
			if self.pulled <= 0 then
				self.pulled = nil
				self.animation = self.selected and "hover" or "idle"
			end
		end

		self.action = action
	end,
}
