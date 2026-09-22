local tween = require("3rdpary/tween")
local breathing = {
	duration = 0.8,
	target = { scale = 1.02 },
}

breathing.cycle = breathing.duration * 2

function breathing.start(self)
	self.breath = self.breath or {}
	self.breath.clock = 0
	local release = self.breath.release
	if release then
		self.breath.tween = release.breath
	else
		self.breath.tween = self.breath.tween or tween.new(breathing.duration, self, breathing.target, "inOutSine")
	end

	local active = self.breath.tween
	if active.initial then
		active.initial.scale = self.scale
		active.starts[1] = self.scale
	end

	active:reset()
end

function breathing.stop(self)
	local breath = self.breath
	local release = breath.release
	if not release then
		release = {
			breath = breath.tween,
			tween = tween.new(breathing.duration, self, { scale = 1 }, "inOutSine"),
		}

		release.set = function(_, clock)
			if breath.clock >= breathing.duration then
				breath.clock = false
				self.scale = 1

				return
			end

			release.tween:set(clock)
		end
		breath.release = release
	else
		release.tween.initial.scale = self.scale
		release.tween.starts[1] = self.scale
	end

	breath.clock = 0
	breath.tween = release
	release.tween:reset()
end

function breathing.step(self, delta)
	local breath = self.breath
	local phase = breath and breath.clock
	if not phase then
		return
	end

	phase = phase + delta
	local cycle = breathing.cycle
	if phase >= cycle then
		phase = phase % cycle
	end

	breath.clock = phase
	local clock = phase <= breathing.duration and phase or cycle - phase
	breath.tween:set(clock)
end

local controls = require("helpers/controls")

return {
	animation = {
		default = "default",

		["default"] = {
			loop = false,
			{ 36, 14, 40, 76, 335, 100, 480, 270, -1, 0, 0, 40, 76 },
		},
		["hover"] = {
			loop = false,
			{ 76, 14, 40, 76, 335, 100, 480, 270, -1, 0, 0, 40, 76 },
		},
		["open"] = {
			loop = false,
			{ 0, 0, 36, 90, 335, 86, 480, 270, -1, 0, 0, 36, 90 },
		},
	},

	select = function(self)
		self.selected = true
		self.action = controls.action
		self.animation = "hover"
		breathing.start(self)
	end,

	unselect = function(self)
		self.selected = false
		self.action = false
		self.animation = "default"
		breathing.stop(self)
	end,

	on_click = function(self)
		if self.selected then
			self.animation = "open"
		end
	end,

	on_loop = function(self, delta)
		breathing.step(self, delta)
		local action = controls.action
		if self.selected and action and not self.action then
			self.animation = "open"
		end

		self.action = action
	end,
}
