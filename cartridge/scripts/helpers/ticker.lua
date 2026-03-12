local remove = table.remove

local ticker = {}
local timers = {}
local n = 0

local function add(ticks, callback, once)
	local timer = { target = ticks, current = 0, callback = callback, once = once }
	n = n + 1
	timers[n] = timer
	return timer
end

function ticker.after(ticks, callback)
	return add(ticks, callback, true)
end

function ticker.every(ticks, callback)
	return add(ticks, callback, false)
end

function ticker.cancel(timer)
	timer.cancelled = true
end

function ticker.clear()
	for i = n, 1, -1 do
		timers[i] = nil
	end
	n = 0
end

function ticker.wrap(stage)
	local original_on_tick = stage.on_tick
	local original_on_leave = stage.on_leave

	stage.on_tick = function(self, tick)
		local list = timers
		for i = n, 1, -1 do
			local timer = list[i]
			if not timer then
				break
			end
			if timer.cancelled then
				remove(list, i)
				n = n - 1
			else
				timer.current = timer.current + 1
				if timer.current >= timer.target then
					timer.callback()
					if timer.once then
						remove(list, i)
						n = n - 1
					else
						timer.current = 0
					end
				end
			end
		end

		if original_on_tick then
			original_on_tick(self, tick)
		end
	end

	stage.on_leave = function()
		if original_on_leave then
			original_on_leave()
		end
		ticker.clear()
	end

	return stage
end

return ticker
