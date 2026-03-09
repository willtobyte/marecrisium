local speed = 200
local jump = -400
local ground_contacts = {}
local ground_count = 0

return {
	body = "dynamic",

	animation = {
		running = {
			{ 0, 0, 16, 16, 200, 0, 0, 16, 16 },
		},
	},

	on_spawn = function(self)
		self.animation = "running"
		self.x = 60
		self.y = 800
	end,

	on_loop = function(self, delta)
		local vx = 0

		if keyboard.left or keyboard.a then
			vx = -speed
			self.flip = "horizontal"
		elseif keyboard.right or keyboard.d then
			vx = speed
			self.flip = "none"
		end

		self.vx = vx

		if ground_count > 0 and (keyboard.space or keyboard.up or keyboard.w) then
			self.vy = jump
		end
	end,

	on_collision_begin = function(self, other_name, other_kind, normal_x, normal_y)
		if normal_y and normal_y < -0.5 then
			ground_contacts[other_name] = true
			ground_count = ground_count + 1
		end
	end,

	on_collision_end = function(self, other_name, other_kind)
		if ground_contacts[other_name] then
			ground_contacts[other_name] = nil
			ground_count = ground_count - 1
		end
	end,
}
