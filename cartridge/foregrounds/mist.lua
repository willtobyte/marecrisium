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

local sheet_width, sheet_height, viewport_width, viewport_height

return {
	on_func = function(self) end,

	on_appear = function(self)
		local pixmap = self.pixmap
		sheet_width = pixmap.width
		sheet_height = pixmap.height
		viewport_width = viewport.width
		viewport_height = viewport.height
	end,

	on_loop = function(self, delta)
		for i = 1, #layers do
			offsets[i] = offsets[i] + layers[i].speed * delta
		end
	end,

	on_paint = function(self)
		local count = 0

		for i = 1, #layers do
			local scale = layers[i].scale
			local alpha = layers[i].alpha
			local draw_width = sheet_width * scale
			local draw_height = sheet_height * scale
			local offset_x = fmod(offsets[i], draw_width)

			for y = -draw_height, viewport_height - 1, draw_height do
				for x = -draw_width + offset_x, viewport_width - 1, draw_width do
					drawings[count + 1] = x
					drawings[count + 2] = y
					drawings[count + 3] = draw_width
					drawings[count + 4] = draw_height
					drawings[count + 5] = 0
					drawings[count + 6] = alpha
					count = count + 6
				end
			end
		end

		self:draw(drawings, count)
	end,
}
