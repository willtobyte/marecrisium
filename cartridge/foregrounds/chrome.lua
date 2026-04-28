local sin = math.sin
local floor = math.floor
local format = string.format

local elapsed = 0
local title = _("Mare Crisium")
local length = #title
local effects = {}

for i = 1, length do
	effects[i] = {}
end

local function hue2rgb(hue)
	local sector = hue * 6
	local fraction = sector - floor(sector)

	if sector < 1 then
		return 1, fraction, 0
	elseif sector < 2 then
		return 1 - fraction, 1, 0
	elseif sector < 3 then
		return 0, 1, fraction
	elseif sector < 4 then
		return 0, 1 - fraction, 1
	elseif sector < 5 then
		return fraction, 0, 1
	else
		return 1, 0, 1 - fraction
	end
end

return {
	fonts = { "pixel" },

	on_loop = function(self, delta)
		elapsed = elapsed + delta

		for i = 1, length do
			local offset = i - 1
			local effect = effects[i]

			effect.r, effect.g, effect.b = hue2rgb((offset / length + elapsed * 0.1) % 1)
			effect.y_offset = sin(elapsed * 3 + offset * 0.7) * 2
			effect.angle = sin(elapsed * 2 + offset * 0.5) * 20
		end
	end,

	on_paint = function(self)
		local minutes = floor(elapsed / 60)
		local seconds = floor(elapsed % 60)

		self.pixel:label(title, 3, 3, effects)
		self.pixel:label(format("%02d:%02d", minutes, seconds), 3, 18)
	end,
}
