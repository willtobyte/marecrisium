local speed = 200
local jump = -400
local grounded = false

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

		if grounded and (keyboard.space or keyboard.up or keyboard.w) then
			self.vy = jump
			grounded = false
		end
	end,

	on_collision_begin = function(self, other_name, other_kind)
		grounded = true
	end,

	on_collision_end = function(self, other_name, other_kind)
		grounded = false
	end,
}
