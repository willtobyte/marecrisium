local snapshot = mouse.snapshot
local interaction = {}

interaction.__index = interaction

local function collider(object)
	local scale = object.scale
	if scale == 1 then
		return object:collider()
	end

	object.scale = 1
	local ox, oy, width, height = object:collider()
	object.scale = scale

	return ox, oy, width, height
end

function interaction.new(objects)
	return setmetatable({
		objects = objects,
		current = false,
		left = mouse.left,
	}, interaction)
end

function interaction:update()
	local objects = self.objects
	if not objects then
		return
	end

	local x, y, left = snapshot()
	local pressed = left and not self.left
	self.left = left
	local current = false
	local depth

	for i = #objects, 1, -1 do
		local object = objects[i]
		if object.shown then
			local z = object.z
			if not current or z > depth then
				local ox, oy, width, height = collider(object)
				if
					x >= ox
					and x < ox + width
					and y >= oy
					and y < oy + height
					and (object.on_hover or object.on_unhover or object.on_click)
				then
					current = object
					depth = z
				end
			end
		end
	end

	if current ~= self.current then
		local prior = self.current
		self.current = false
		if prior then
			local callback = prior.on_unhover
			if callback then
				callback(prior)
			end
		end

		if not self.objects then
			return
		end

		self.current = current
		if current then
			local callback = current.on_hover
			if callback then
				callback(current)
			end
		end
	end

	if pressed and current and self.current == current then
		local callback = current.on_click
		if callback then
			callback(current)
		end
	end
end

function interaction:clear()
	local current = self.current
	self.current = false
	self.objects = false
	self.left = false
	if current then
		local callback = current.on_unhover
		if callback then
			callback(current)
		end
	end
end

return interaction
