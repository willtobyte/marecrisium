local getenv = os.getenv

return {
	title = "Mare Crisium",
	width = 1920,
	height = 1080,
	scale = 4.0,
	fullscreen = getenv("WINDOWED") ~= "1",
	on_begin = function()
		director.enroll("gatehouse")

		director.navigate("gatehouse")
	end,
}
