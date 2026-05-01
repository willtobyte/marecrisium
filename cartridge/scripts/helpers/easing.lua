local function quad_in(t)
	return t * t
end

local function quad_out(t)
	return t * (2 - t)
end

local function quad_in_out(t)
	if t < 0.5 then
		return 2 * t * t
	end
	return -1 + (4 - 2 * t) * t
end

return {
	quad_in = quad_in,
	quad_out = quad_out,
	quad_in_out = quad_in_out,
}
