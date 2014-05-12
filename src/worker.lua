function doit(ud, count)
    -- increase the invocation count of lua code
    inccount(ud)

    local t1 = {}
    local t2 = {}
    for i=1,count do
        t1[i] = "value " .. i
        t2[i] = i + 1024
    end
end
