return {
	pixmaps = { "mist_layer1" },

	on_bump = function(self)
		print("foreground: bump!")
	end,

	on_loop = function(self, delta) end,

	on_paint = function(self)
		local sprite = self.mist_layer1
		local sw = sprite.width
		local sh = sprite.height
		local vw = viewport.width
		local vh = viewport.height

		for y = 0, vh - 1, sh do
			for x = 0, vw - 1, sw do
				self:draw(sprite, x, y, sw, sh, 200)
			end
		end
	end,
}
