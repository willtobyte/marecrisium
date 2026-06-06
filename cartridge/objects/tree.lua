return {
	body = "static",

	animation = {
		idle = {
			{ 0, 0, 20, 32, 1000, 0, 0, 20, 32 },
		},
	},

	on_press = function(self, x, y, button)
		print(string.format("[mouse] press   %-6s on %-6s at %.1f,%.1f", button, self.name, x, y))
	end,

	on_release = function(self, x, y, button)
		print(string.format("[mouse] release %-6s on %-6s at %.1f,%.1f", button, self.name, x, y))
	end,

	on_hover = function(self)
		print(string.format("[mouse] hover   %s", self.name))
	end,

	on_unhover = function(self)
		print(string.format("[mouse] unhover %s", self.name))
	end,
}
