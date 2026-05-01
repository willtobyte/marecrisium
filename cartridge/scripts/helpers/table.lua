local function copy(t)
	local r = {}
	for k, v in pairs(t) do
		r[k] = v
	end
	return r
end

local function keys(t)
	local r, n = {}, 0
	for k in pairs(t) do
		n = n + 1
		r[n] = k
	end
	return r
end

return {
	copy = copy,
	keys = keys,
}
