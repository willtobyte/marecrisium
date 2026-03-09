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
		elseif key == "jump" then
			return keyboard.x or gamepad.south
		elseif key == "attack" then
			return keyboard.z or gamepad.west
		elseif key == "start" then
			return keyboard.enter or gamepad.start
		end
	end,
})

return controls
