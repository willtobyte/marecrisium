return {
	width = 1280,
	height = 720,
	title = "My Game",
	scale = 3.0,
	fullscreen = os.getenv("FULLSCREEN") == "1",
	on_begin = function()
		director.navigate("mainmenu")
	end,
}
