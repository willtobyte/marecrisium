local function split(s, sep)
	local r, n = {}, 0
	for piece in s:gmatch("([^" .. sep .. "]+)") do
		n = n + 1
		r[n] = piece
	end
	return r
end

local function trim(s)
	return s:match("^%s*(.-)%s*$")
end

return {
	split = split,
	trim = trim,
}
