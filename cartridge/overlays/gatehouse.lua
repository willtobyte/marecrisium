local floor = math.floor
local sin = math.sin
local title = _("Mare Crisium")
local length = #title
local effects = {}
local elapsed = 0

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
	end

	return 1, 0, 1 - fraction
end

return {
	fonts = { "pixel" },

	on_loop = function(_, delta)
		elapsed = elapsed + delta

		for i = 1, length do
			local offset = i - 1
			local effect = effects[i]

			effect.r, effect.g, effect.b = hue2rgb((offset / length + elapsed * 0.1) % 1)
			effect.y_offset = sin(elapsed * 3 + offset * 0.7) * 2
			effect.angle = sin(elapsed * 2 + offset * 0.5) * 20
		end
	end,

	on_paint = function()
		pool.pixel:draw(title, (viewport.width - 96) / 2, (viewport.height - 17) / 2, effects)
	end,
}
