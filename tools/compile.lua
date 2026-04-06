for i = 1, #arg do
	local path = arg[i]
	local fn = assert(loadfile(path))
	local out = assert(io.open(path, "wb"))
	out:write(string.dump(fn))
	out:close()
end
