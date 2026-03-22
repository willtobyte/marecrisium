local getenv = os.getenv

return {
	title = "My Game",
	width = 1920,
	height = 1080,
	scale = 3.0,
	ticks = 10,
	fullscreen = getenv("WINDOWED") ~= "1",
	sentry = "https://b73cc92e6e405d9dc02c2f6f040d6ac7@o4509972952907776.ingest.us.sentry.io/4511022227849216",
	on_begin = function()
		mouse.shown = false
		director.navigate("forest")
	end,
}
