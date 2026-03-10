return {
	title = "My Game",
	width = 1920,
	height = 1080,
	scale = 4.0,
	ticks = 10,
	fullscreen = os.getenv("FULLSCREEN") == "1",
	sentry = "https://b73cc92e6e405d9dc02c2f6f040d6ac7@o4509972952907776.ingest.us.sentry.io/4511022227849216",
	on_begin = function()
		director.navigate("forest")
	end,
}
