------------------
---- MONITORS ----
------------------
hl.monitor({
    output = "DP-2",
    mode = "3440x1440@175",
    position = "auto",
    scale = "1",
    reserved_area = { top = 10, bottom = 10, left = 0, right = 0 },
    vrr = true,
})

------------------
---- GESTURES ----
------------------
hl.gesture({ fingers = 3, direction = "horizontal", action = "workspace" })
hl.gesture({ fingers = 4, direction = "up", action = "special", workspace_name = "magic" })
hl.gesture({ fingers = 3, direction = "pinchin", action = "close", disable_inhibit = true })

---------------------
---- KEYBINDINGS ----
---------------------
hl.bind("SUPER + R", hl.dsp.submap("resize"))
hl.define_submap("resize", function()
    hl.bind("right", hl.dsp.window.resize({ delta = "10 0" }))
    hl.bind("left", hl.dsp.window.resize({ delta = "-10 0" }))
    hl.bind("escape", hl.dsp.submap("reset"))
end)
