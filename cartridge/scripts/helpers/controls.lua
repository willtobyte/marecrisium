local threshold = 0.5

local controls = {}

setmetatable(controls, {
	__index = function(_, key)
		if key == "left" then
			return gamepad.left_x < -threshold or gamepad.left or keyboard.a
		elseif key == "right" then
			return gamepad.left_x > threshold or gamepad.right or keyboard.d
		elseif key == "up" then
			return gamepad.left_y < -threshold or gamepad.up or keyboard.w
		elseif key == "down" then
			return gamepad.left_y > threshold or gamepad.down or keyboard.s
		elseif key == "minimap" then
			return gamepad.back or keyboard.tab
		end
	end,
})

return controls
