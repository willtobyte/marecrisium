return {
	animation = {
		default = "normal",
		hover = {
			{ 0, 0, 58, 67, 277, 178, 480, 270, -1, 0, 0, 58, 67 },
		},
		normal = {
			{ 58, 0, 58, 67, 277, 178, 480, 270, -1, 0, 0, 58, 67 },
		},
	},

	select = function(self)
		self.animation = "hover"
	end,

	unselect = function(self)
		self.animation = "normal"
	end,
}
