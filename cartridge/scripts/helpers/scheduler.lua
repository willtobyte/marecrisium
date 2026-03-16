local resume = coroutine.resume
local create = coroutine.create
local yield = coroutine.yield

local scheduler = {}

local list = {}
local n = 0
local tick = 0

function scheduler.spawn(fn)
	n = n + 1
	list[n] = { co = create(fn), resume_at = tick }
end

scheduler.wait = yield

function scheduler.advance(current_tick)
	tick = current_tick
	local i = 1
	while i <= n do
		local entry = list[i]
		if current_tick >= entry.resume_at then
			local ok, result = resume(entry.co)
			if ok and result then
				entry.resume_at = current_tick + result
				i = i + 1
			else
				if not ok then
					print("[scheduler] " .. tostring(result))
				end
				list[i] = list[n]
				list[n] = nil
				n = n - 1
			end
		else
			i = i + 1
		end
	end
end

function scheduler.clear()
	for i = 1, n do
		list[i] = nil
	end
	n = 0
end

local function chain(original, fn)
	if original then
		return function(self, ...)
			fn(self, ...)
			original(self, ...)
		end
	end
	return function(self, ...)
		fn(self, ...)
	end
end

function scheduler.wrap(stage)
	stage.on_tick = chain(stage.on_tick, function(_, t)
		scheduler.advance(t)
	end)
	stage.on_leave = chain(stage.on_leave, function()
		scheduler.clear()
	end)
	return stage
end

return scheduler
