local floor = math.floor

local camera = {}

function camera.update(target)
	camera.x = floor(target.x - viewport.width * 0.5 + 0.5)
	camera.y = floor(target.y - viewport.height * 0.5 + 0.5)
end

function camera.position()
	return camera.x or 0, camera.y or 0
end

return camera
