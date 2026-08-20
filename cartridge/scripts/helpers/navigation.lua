local controls = require("helpers/controls")
local navigator = {}
local navigation = {}

navigator.__index = navigator

function navigation.new(objects)
	return setmetatable({
		objects = objects,
		left = controls.left,
		right = controls.right,
		up = controls.up,
		down = controls.down,
	}, navigator)
end

function navigator:update()
	local left = controls.left
	local right = controls.right
	local up = controls.up
	local down = controls.down
	local dx = 0
	local dy = 0

	if self.left and not left then
		dx = -1
	elseif self.right and not right then
		dx = 1
	elseif self.up and not up then
		dy = -1
	elseif self.down and not down then
		dy = 1
	end

	self.left = left
	self.right = right
	self.up = up
	self.down = down

	if dx == 0 and dy == 0 then
		return
	end

	local objects = self.objects
	local current = self.current
	if not current then
		current = objects[1]
		self.current = current
		current:select()
		return
	end

	local x = current.x
	local y = current.y
	local winner
	local nearest

	for i = 1, #objects do
		local object = objects[i]
		local ox = object.x - x
		local oy = object.y - y

		if ox * dx + oy * dy > 0 then
			local distance = ox * ox + oy * oy
			if not nearest or distance < nearest then
				winner = object
				nearest = distance
			end
		end
	end

	if not winner then
		return
	end

	current:unselect()
	self.current = winner
	winner:select()
end

function navigator:clear()
	if self.current then
		self.current:unselect()
	end

	self.objects = nil
	self.current = nil
end

return navigation
