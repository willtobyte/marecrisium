local threshold = 0.5

local controls = {}

setmetatable(controls, {
	__index = function(_, key)
		if key == "left" then
			return keyboard.left or gamepad.left or gamepad.left_x < -threshold
		elseif key == "right" then
			return keyboard.right or gamepad.right or gamepad.left_x > threshold
		elseif key == "up" then
			return keyboard.up or gamepad.up or gamepad.left_y < -threshold
		elseif key == "down" then
			return keyboard.down or gamepad.down or gamepad.left_y > threshold
		end
	end,
})

return controls
