local min, max = math.min, math.max

local function clamp(x, a, b)
	return min(max(x, a), b)
end
local function lerp(a, b, t)
	return a + (b - a) * t
end
local function sign(x)
	return x > 0 and 1 or x < 0 and -1 or 0
end

return {
	clamp = clamp,
	lerp = lerp,
	sign = sign,
}
