local spawner = {}

function spawner.at_center(name, kind, center_x, center_y)
	world.spawn(name, kind, center_x, center_y)
	local instance = pool[name]
	if instance then
		instance.x = instance.x + (center_x - instance.center_x)
		instance.y = instance.y + (center_y - instance.center_y)
	end
	return instance
end

return spawner
