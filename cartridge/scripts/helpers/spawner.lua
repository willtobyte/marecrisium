local spawner = {}

function spawner.at_center(name, kind, center_x, center_y)
	local instance = world.spawn(name, kind, center_x, center_y)
	if instance then
		instance.x = instance.x + (center_x - instance.center_x)
		instance.y = instance.y + (center_y - instance.center_y)
	end
	return instance
end

return spawner
