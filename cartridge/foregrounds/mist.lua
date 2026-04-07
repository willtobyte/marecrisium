local fmod = math.fmod

local layers = {
	{ speed = 35, alpha = 120, scale = 0.75 },
}

local offsets = { 0 }

local STRIDE = 6
local MAX_QUADS = 512
local drawings = {}
for i = 1, MAX_QUADS * STRIDE do
	drawings[i] = 0
end

local sw, sh, vw, vh

return {
	on_bump = function(self)
		print("foreground: bump!")
	end,

	on_appear = function(self)
		local pixmap = self.pixmap
		sw = pixmap.width
		sh = pixmap.height
		vw = viewport.width
		vh = viewport.height
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
			local dw = sw * scale
			local dh = sh * scale
			local ox = fmod(offsets[i], dw)

			for y = -dh, vh - 1, dh do
				for x = -dw + ox, vw - 1, dw do
					drawings[n + 1] = x
					drawings[n + 2] = y
					drawings[n + 3] = dw
					drawings[n + 4] = dh
					drawings[n + 5] = 0
					drawings[n + 6] = alpha
					n = n + 6
				end
			end
		end

		self:draw(drawings, n)
	end,
}
