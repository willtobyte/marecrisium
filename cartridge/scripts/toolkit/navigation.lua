local controls = require("helpers/controls")
local navigator = {}
local navigation = {}
local active

navigator.__index = navigator

function navigation.new(objects)
	active = setmetatable({
		objects = objects,
		left = controls.left,
		right = controls.right,
		up = controls.up,
		down = controls.down,
	}, navigator)

	return active
end

function navigation.select(object)
	active:select(object)
end

function navigation.unselect(object)
	active:unselect(object)
end

function navigator:select(object)
	if self.selected and self.current ~= object then
		self.current:unselect()
	end

	self.current = object
	self.selected = true
	object:select()
end

function navigator:unselect(object)
	if self.current == object then
		object:unselect()
		self.selected = false
	end
end

function navigator:update()
	local left = controls.left
	local right = controls.right
	local up = controls.up
	local down = controls.down
	local dx = 0
	local dy = 0

	if left and not self.left then
		dx = -1
	elseif right and not self.right then
		dx = 1
	elseif up and not self.up then
		dy = -1
	elseif down and not self.down then
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
	local current = self.current or objects[1]

	local cx, cy, cw, ch = current:collider()
	local x = cx + cw * 0.5
	local y = cy + ch * 0.5
	local winner
	local nearest

	for i = 1, #objects do
		local object = objects[i]
		local ox, oy, w, h = object:collider()
		ox = ox + w * 0.5 - x
		oy = oy + h * 0.5 - y

		if ox * dx + oy * dy > 0 then
			local distance = ox * ox + oy * oy
			if not nearest or distance < nearest then
				winner = object
				nearest = distance
			end
		end
	end

	if not winner then
		if self.current then
			return
		end

		winner = current
	end

	self:select(winner)
end

function navigator:clear()
	if self.current then
		self.current:unselect()
	end

	self.objects = nil
	self.current = nil
	self.selected = nil
	active = nil
end

return navigation
