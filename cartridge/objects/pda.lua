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
	end,

	unselect = function(self)
		self.animation = "default"
	end,
}
