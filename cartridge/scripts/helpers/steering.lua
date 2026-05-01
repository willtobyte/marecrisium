local PROBE_OFFSETS = { 30, -30, 60, -60, 90, -90, 135, -135 }
local DTR = math.pi / 180
local RTD = 180 / math.pi

local steering = {}

function steering.avoid(object, x, y, desired, distance)
	local degrees = desired * RTD
	local hit = world.raycast(object, x, y, degrees, distance)[1]
	if not hit then
		return desired
	end

	for _, offset in ipairs(PROBE_OFFSETS) do
		local candidate = world.raycast(object, x, y, degrees + offset, distance)[1]
		if not candidate then
			return desired + offset * DTR
		end
	end

	return nil
end

return steering
