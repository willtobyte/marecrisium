return {
	animation = {
		default = "normal",
		hover = {
			{ 0, 0, 55, 37, 215, 227, 480, 270, -1, 0, 0, 55, 37 },
		},
		normal = {
			{ 55, 0, 55, 37, 215, 227, 480, 270, -1, 0, 0, 55, 37 },
		},
	},

	select = function(self)
		self.animation = "hover"
	end,

	unselect = function(self)
		self.animation = "normal"
	end,
}
