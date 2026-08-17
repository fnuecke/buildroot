-- Minimal lazy-loaded replacement that doesn't break due to missing bits.
-- We do lose the flat API (posix.fork() e.g.), but hey.
return setmetatable({}, {
    __index = function(t, k)
        local ok, mod = pcall(require, "posix." .. k)
        if not ok then
            return nil
        end
        rawset(t, k, mod)
        return mod
    end,
})
