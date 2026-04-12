local clock = os.clock
local format = string.format
local getenv = os.getenv
local randomseed = math.randomseed

return {
	title = "My Game",
	splash = "loading",
	width = 1920,
	height = 1080,
	scale = 3.0,
	ticks = 10,
	fullscreen = getenv("WINDOWED") ~= "1",
	sentry = "https://b73cc92e6e405d9dc02c2f6f040d6ac7@o4509972952907776.ingest.us.sentry.io/4511022227849216",
	on_begin = function()
		randomseed(moment())
		mouse.shown = false

		local before = clock()
		director.enroll("forest")
		local elapsed = (clock() - before) * 1000

		print(format("[director] enrolled all scenes in %.2f ms", elapsed))

		director.navigate("forest")
	end,
}
