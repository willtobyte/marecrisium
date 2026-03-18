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

local function hue2rgb(h)
	local s = h * 6
	local f = s - floor(s)

	if s < 1 then
		return 1, f, 0
	elseif s < 2 then
		return 1 - f, 1, 0
	elseif s < 3 then
		return 0, 1, f
	elseif s < 4 then
		return 0, 1 - f, 1
	elseif s < 5 then
		return f, 0, 1
	else
		return 1, 0, 1 - f
	end
end

return {
	fonts = { "pixel" },

	on_loop = function(self, delta)
		elapsed = elapsed + delta

		for i = 1, length do
			local k = i - 1
			local effect = effects[i]

			effect.r, effect.g, effect.b = hue2rgb((k / length + elapsed * 0.1) % 1)
			effect.yoffset = sin(elapsed * 3 + k * 0.7) * 2
			effect.angle = sin(elapsed * 2 + k * 0.5) * 20
		end
	end,

	on_paint = function(self)
		local minutes = floor(elapsed / 60)
		local seconds = floor(elapsed % 60)

		overlay:label("pixel", title, 3, 3, effects)
		overlay:label("pixel", format("%02d:%02d", minutes, seconds), 3, 18)
	end,
}
