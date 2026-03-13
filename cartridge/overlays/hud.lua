local sin = math.sin
local floor = math.floor
local format = string.format

local elapsed = 0
local title = _("nostalgia")
local effects = {}

for i = 1, #title do
	effects[i] = {}
end

return {
	fonts = { "pixel" },

	layers = {
		"background",
		10,
		10,
		30,
		30,
		"health100",
		10,
		10,
		30,
		30,
	},

	on_loop = function(self, delta)
		elapsed = elapsed + delta

		for i = 1, #title do
			local phase = elapsed * 3 + (i - 1) * 0.7
			local wave = sin(phase) * 2
			local hue = ((i - 1) / #title + elapsed * 0.1) % 1

			local r, g, b
			local sector = hue * 6
			local frac = sector - floor(sector)

			if sector < 1 then
				r, g, b = 1, frac, 0
			elseif sector < 2 then
				r, g, b = 1 - frac, 1, 0
			elseif sector < 3 then
				r, g, b = 0, 1, frac
			elseif sector < 4 then
				r, g, b = 0, 1 - frac, 1
			elseif sector < 5 then
				r, g, b = frac, 0, 1
			else
				r, g, b = 1, 0, 1 - frac
			end

			local effect = effects[i]
			effect.yoffset = wave
			effect.r = r
			effect.g = g
			effect.b = b
			effect.angle = sin(elapsed * 2 + (i - 1) * 0.5) * 20
			-- effect.alpha = 0.6 + 0.4 * math.sin(elapsed * 4 + (i - 1) * 0.9)
		end
	end,

	on_paint = function(self)
		local minutes = floor(elapsed / 60)
		local seconds = floor(elapsed % 60)
		local time = format("%02d:%02d", minutes, seconds)

		overlay:label("pixel", title, 3, 3, effects)
		overlay:label("pixel", time, 3, 18)
	end,
}
