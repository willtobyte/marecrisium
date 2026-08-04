local clock = os.clock
local format = string.format
local getenv = os.getenv
local randomseed = math.randomseed
local time = os.time

return {
	title = "Mare Crisium",
	-- splash = "loading",
	width = 1920,
	height = 1080,
	scale = 4.0,
	fullscreen = getenv("WINDOWED") ~= "1",
	on_begin = function()
		local seed = tonumber(cassette.seed)
		if not seed then
			seed = time()
			cassette.seed = seed
		end

		randomseed(seed)
	end,
}
