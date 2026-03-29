local scheduler = require("helpers/scheduler")

return scheduler.wrap({
	tilemap = "forest",

	overlay = { widgets = "hud" }, --, foreground = "mist" },

	particles = {
		{ name = "smoke", kind = "smoke", x = 50, y = 100 },
	},

	sounds = {
		{ name = "fx", autoplay = true },
	},

	objects = {
		{ name = "player", kind = "player", x = 100, y = 100 },
		{ name = "robot", kind = "robot", x = 250, y = 150 },
		{ name = "drone", kind = "drone", x = 180, y = 200 },
		{ name = "demon", kind = "demon", x = 300, y = 100 },
		{ name = "tree1", kind = "tree", x = 400, y = 80 },
		{ name = "tree2", kind = "tree", x = 500, y = 250 },
		{ name = "tree3", kind = "tree", x = 150, y = 300 },
		{ name = "tree4", kind = "tree", x = 50, y = 200 },
		{ name = "tree5", kind = "tree", x = 350, y = 200 },
		{ name = "tree6", kind = "tree", x = 550, y = 130 },
		{ name = "tree7", kind = "tree", x = 220, y = 50 },
		{ name = "tree8", kind = "tree", x = 450, y = 320 },
	},

	on_enter = function(self)
		local pairs = pairs
		local type = type

		local function deep_equal(a, b)
			if type(a) ~= type(b) then
				return false
			end
			if type(a) ~= "table" then
				return a == b
			end
			for k, v in pairs(a) do
				if not deep_equal(v, b[k]) then
					return false
				end
			end
			for k in pairs(b) do
				if a[k] == nil then
					return false
				end
			end
			return true
		end

		-- boolean
		cassette.bool_true = true
		assert(cassette.bool_true == true, "bool true")
		cassette.bool_false = false
		assert(cassette.bool_false == false, "bool false")

		-- number
		cassette.int_val = 42
		assert(cassette.int_val == 42, "integer")
		cassette.float_val = 3.14
		assert(cassette.float_val == 3.14, "float")

		-- string
		cassette.str_val = "hello"
		assert(cassette.str_val == "hello", "string")

		-- table array
		local arr = { 1, 2, 3 }
		cassette.arr_val = arr
		assert(deep_equal(cassette.arr_val, arr), "table array")

		-- table map
		local map = { a = 1, b = "two", c = true }
		cassette.map_val = map
		assert(deep_equal(cassette.map_val, map), "table map")

		-- nested table
		local nested = { x = { 1, 2 }, y = { z = "deep" } }
		cassette.nested_val = nested
		assert(deep_equal(cassette.nested_val, nested), "nested table")

		-- delete via nil
		cassette.to_delete = "exists"
		assert(cassette.to_delete == "exists", "before delete")
		cassette.to_delete = nil
		assert(cassette.to_delete == nil, "after delete")

		-- non-existent key returns nil
		assert(cassette.no_such_key == nil, "non-existent key")

		-- clear
		-- cassette:clear()
		-- assert(cassette.bool_true == nil, "clear bool")
		-- assert(cassette.int_val == nil, "clear int")
		-- assert(cassette.str_val == nil, "clear str")
		-- assert(cassette.arr_val == nil, "clear arr")
		-- assert(cassette.map_val == nil, "clear map")
		-- assert(cassette.nested_val == nil, "clear nested")

		print("[cassette] all tests passed")
	end,
})
