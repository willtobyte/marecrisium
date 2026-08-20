local controls = {}
local deadzone = 0.5

setmetatable(controls, {
	__index = function(_, key)
		if key == "left" then
			return gamepad.left_x < -deadzone or gamepad.left or keyboard.a or keyboard.left
		elseif key == "right" then
			return gamepad.left_x > deadzone or gamepad.right or keyboard.d or keyboard.right
		elseif key == "up" then
			return gamepad.left_y < -deadzone or gamepad.up or keyboard.w or keyboard.up
		elseif key == "down" then
			return gamepad.left_y > deadzone or gamepad.down or keyboard.s or keyboard.down
		elseif key == "action" then
			return gamepad.south or keyboard.space
		elseif key == "dismiss" then
			return gamepad.east or keyboard.escape
		end
	end,
})

return controls
