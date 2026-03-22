local fmod = math.fmod

local layers = {
	{ speed = 35, alpha = 120, scale = 0.75 },
}

local offsets = { 0, 0, 0 }

local STRIDE = 5
local MAX_QUADS = 512
local drawings = {}
for i = 1, MAX_QUADS * STRIDE do
	drawings[i] = 0
end

local sprite_width, sprite_height, viewport_width, viewport_height

return {
	on_bump = function(self)
		print("foreground: bump!")
	end,

	on_appear = function(self)
		local pixmap = self.pixmap
		sprite_width = pixmap.width
		sprite_height = pixmap.height
		viewport_width = viewport.width
		viewport_height = viewport.height
	end,

	on_loop = function(self, delta)
		for i = 1, #layers do
			offsets[i] = offsets[i] + layers[i].speed * delta
		end
	end,

	on_paint = function(self)
		local n = 0

		for i = 1, #layers do
			local scale = layers[i].scale
			local alpha = layers[i].alpha
			local dw = sprite_width * scale
			local dh = sprite_height * scale
			local ox = fmod(offsets[i], dw)

			for y = -dh, viewport_height - 1, dh do
				for x = -dw + ox, viewport_width - 1, dw do
					drawings[n + 1] = x
					drawings[n + 2] = y
					drawings[n + 3] = dw
					drawings[n + 4] = dh
					drawings[n + 5] = alpha
					n = n + 5
				end
			end
		end

		self:draw(drawings, n)
	end,
}
