local ticker = {}
local timers = {}

local function add(ticks, callback, once)
	local timer = { target = ticks, current = 0, callback = callback, once = once }
	timers[timer] = true
	return timer
end

function ticker.after(ticks, callback)
	return add(ticks, callback, true)
end

function ticker.every(ticks, callback)
	return add(ticks, callback, false)
end

function ticker.cancel(timer)
	timers[timer] = nil
end

function ticker.clear()
	timers = {}
end

function ticker.wrap(stage)
	local original_on_tick = stage.on_tick
	local original_on_leave = stage.on_leave

	stage.on_tick = function(self, tick)
		for timer in pairs(timers) do
			timer.current = timer.current + 1
			if timer.current >= timer.target then
				timer.callback()
				if timer.once then
					timers[timer] = nil
				else
					timer.current = 0
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
