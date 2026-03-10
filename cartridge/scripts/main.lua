return {
	title = "My Game",
	width = 1920,
	height = 1080,
	scale = 4.0,
	ticks = 10,
	meter = 100,
	gravity = { 0, 980 },
	fullscreen = os.getenv("FULLSCREEN") == "1",
	on_begin = function()
		director.navigate("forest")
	end,
}
