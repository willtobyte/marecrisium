return {
	title = "My Game",
	width = 1920,
	height = 1080,
	scale = 3.0,
	ticks = 10,
	-- gravity = { 0, 980 },
	fullscreen = os.getenv("FULLSCREEN") == "1",
	on_begin = function()
		director.navigate("mainmenu")
	end,
}
