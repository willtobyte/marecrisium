return {
	animation = {
		default = "default",

		["default"] = {
			loop = false,
			{ 0, 0, 47, 108, 46, 136, 480, 270, -1, 0, 0, 47, 108 },
		},
		["hover"] = {
			loop = false,
			{ 47, 0, 47, 108, 46, 136, 480, 270, -1, 0, 0, 47, 108 },
		},
	},

	select = function(self)
		self.animation = "hover"
	end,

	unselect = function(self)
		self.animation = "default"
	end,
}
