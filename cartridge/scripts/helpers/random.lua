local random = math.random

local function choice(t)
	return t[random(#t)]
end

local function range(a, b)
	return a + random() * (b - a)
end

local function chance(p)
	return random() < p
end

return {
	choice = choice,
	range = range,
	chance = chance,
}
