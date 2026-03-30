local threshold = 0.5

local controls = {}

setmetatable(controls, {
	__index = function(_, key)
		if key == "left" then
			return gamepad.left_x < -threshold or gamepad.left or keyboard.left
		elseif key == "right" then
			return gamepad.left_x > threshold or gamepad.right or keyboard.right
		elseif key == "up" then
			return gamepad.left_y < -threshold or gamepad.up or keyboard.up
		elseif key == "down" then
			return gamepad.left_y > threshold or gamepad.down or keyboard.down
		end
	end,
})

return controls
