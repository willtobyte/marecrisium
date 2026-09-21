local floor = math.floor
local sin = math.sin
local title = _("Mare Crisium")
local length = #title
local effects = {}
local elapsed = 0
local a
local b
local c
local d
local t

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
	fonts = { "pixel", "arcade8" },

	on_appear = function()
		a = Label.new("pixel", 10, 10, 200, 60)
		b = Label.new("pixel", 10, 80, 200, 60)
		c = Label.new("pixel", 10, 150, 200, 50)
		d = Label.new("arcade8", 230, 10, 220, 100)
		t = Label.new("pixel", 192, 213, 250, 24)
	end,

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
		-- a:draw("Mare Crisium log: long text wraps by word, never splits and never leaks.")
		-- b:draw("supercalifragilisticexpialidocious breaks mid-word only when wider than w.")
		-- c:draw("Short h cuts extra lines clean. This tail stays out. Extra words out.")
		-- d:draw("Direct box: Label.new(arcade8, 230, 10, 220, 100) wraps too.")
		-- t:draw(title, effects)
	end,
}
