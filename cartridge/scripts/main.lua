jit.opt.start("maxtrace=8000", "maxrecord=16000", "maxmcode=16384", "sizemcode=128", "maxside=200")
collectgarbage("setpause", 100)
collectgarbage("setstepmul", 400)

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
	sentry = "https://b73cc92e6e405d9dc02c2f6f040d6ac7@o4509972952907776.ingest.us.sentry.io/4511022227849216",
	on_begin = function()
		local seed = cassette.seed
		if not seed then
			seed = time()
			cassette.seed = seed
		end
		randomseed(seed)

		local before = clock()
		director.enroll("forest")
		-- director.enroll...
		local elapsed = (clock() - before) * 1000

		print(format("[director] enrolled all scenes in %.2f ms", elapsed))

		print(
			"[client] connect accepted:",
			internet.connect("127.0.0.1", 7777, function(ok)
				print("[client] connect callback:", ok)
			end)
		)

		director.navigate("forest")
	end,
}
