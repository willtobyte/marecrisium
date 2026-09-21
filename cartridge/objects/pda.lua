local tween = require("3rdpary/tween")
local breathing = {
	duration = 0.8,
	target = { scale = 1.1 },
}

breathing.cycle = breathing.duration * 2

return {
	animation = {
		default = "default",

		["default"] = {
			loop = false,
			{ 0, 0, 55, 37, 215, 227, 480, 270, -1, 0, 0, 55, 37 },
		},
		["hover"] = {
			loop = false,
			{ 55, 0, 55, 37, 215, 227, 480, 270, -1, 0, 0, 55, 37 },
		},
	},

	select = function(self)
		self.animation = "hover"
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
	end,

	unselect = function(self)
		self.animation = "default"
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
	end,

	on_loop = function(self, delta)
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
	end,
}
