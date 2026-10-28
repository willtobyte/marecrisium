return {
	animation = {
		default = "normal",
		normal = {
			{ 374, 1, 374, 268, 45, 1, 480, 270, -1 },
		},
		slam = {
			{ 374, 1, 374, 268, 45, 1, 480, 270, 160 },
			{ 0, 269, 374, 269, 45, 0, 480, 270, 160 },
			{ 0, 0, 374, 269, 45, 0, 480, 270, 160 },
			{ 374, 269, 374, 269, 45, 0, 480, 270, 480 },
			{ 0, 0, 374, 269, 45, 0, 480, 270, 160 },
		},
	},

	on_spawn = function(self)
		self.shown = false
		self:on_end(function(object)
			object.shown = false
			object.animation = "normal"
			object.done()
		end)
	end,

	on_slam = function(self, done)
		self.done = done
		self.shown = true
		self.animation = "slam"
	end,
}
