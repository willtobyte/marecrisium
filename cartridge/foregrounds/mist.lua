local fmod = math.fmod

local LAYERS = {
	{ speed = 8, alpha = 80, scale = 1.5 },
	{ speed = 20, alpha = 100, scale = 1.0 },
	{ speed = 35, alpha = 120, scale = 0.75 },
}

local offsets = { 0, 0, 0 }

return {
	pixmaps = { "mist_layer1" },

	on_bump = function(self)
		print("foreground: bump!")
	end,

	on_loop = function(self, delta)
		for i = 1, #LAYERS do
			local layer = LAYERS[i]
			offsets[i] = offsets[i] + layer.speed * delta
		end
	end,

	on_paint = function(self)
		local sprite = self.mist_layer1
		local sw = sprite.width
		local sh = sprite.height
		local vw = viewport.width
		local vh = viewport.height

		for i = 1, #LAYERS do
			local layer = LAYERS[i]
			local dw = sw * layer.scale
			local dh = sh * layer.scale
			local ox = fmod(offsets[i], dw)

			for y = -dh, vh - 1, dh do
				for x = -dw + ox, vw - 1, dw do
					self:draw(sprite, x, y, dw, dh, layer.alpha)
				end
			end
		end
	end,
}
