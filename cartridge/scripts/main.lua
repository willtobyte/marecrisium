local clock = os.clock
local format = string.format
local getenv = os.getenv
local randomseed = math.randomseed
local time = os.time

return {
	title = "Mare Crisium",
	splash = "loading",
	width = 1920,
	height = 1080,
	scale = 3.0,
	ticks = 10,
	fullscreen = getenv("WINDOWED") ~= "1",
	on_begin = function()
		local seed = cassette.seed
		if not seed then
			seed = time()
			cassette.seed = seed
		end
		randomseed(seed)

		local before = clock()
		director.enroll("forest")
		local elapsed = (clock() - before) * 1000

		print(format("[director] enrolled all scenes in %.2f ms", elapsed))

		director.navigate("forest")
	end,
}
