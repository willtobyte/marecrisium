local glyphs = [[ abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,:;!?"'`()[]{}<>_-+*/=\|@#$%&^~]]
local sample = "The quick brown fox jumps over the lazy dog. THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG."
local a
local b

return {
	fonts = { "monogram" },

	on_appear = function()
		a = Label.new(pool.monogram, 10, 10, 460, 60)
		b = Label.new(pool.monogram, 10, 80, 460, 60)
	end,

	on_paint = function()
		a:draw(glyphs)
		b:draw(sample)
	end,
}
