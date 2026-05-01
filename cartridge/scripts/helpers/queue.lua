local function new()
	return { first = 1, last = 0 }
end

local function push(q, v)
	q.last = q.last + 1
	q[q.last] = v
end

local function pop(q)
	if q.first > q.last then
		return nil
	end
	local v = q[q.first]
	q[q.first] = nil
	q.first = q.first + 1
	return v
end

return {
	new = new,
	push = push,
	pop = pop,
}
