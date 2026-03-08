local elapsed = 0

return {
	fonts = { "pixel" },
	on_loop = function(self, delta)
		elapsed = elapsed + delta

		local minutes = math.floor(elapsed / 60)
		local seconds = math.floor(elapsed % 60)
		local time = string.format("%02d:%02d", minutes, seconds)

		overlay:label("pixel", "nostalgia", 3, 3)
		overlay:label("pixel", time, 3, 16)
	end,
}
