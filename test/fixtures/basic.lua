------------------
---- MONITORS ----
------------------
hl.monitor({
    output = "DP-1",
    mode = "2560x1440@144",
    position = "0x0",
    scale = "1",
    vrr = 1,
    bitdepth = 10,
})
hl.monitor({ output = "HDMI-A-1", disabled = true })

-------------------
---- AUTOSTART ----
-------------------
hl.on("hyprland.start", function()
    hl.exec_cmd("waybar & hyprpaper")
    hl.exec_cmd("nm-applet --indicator")
end)

-------------------------------
---- ENVIRONMENT VARIABLES ----
-------------------------------
hl.env("XCURSOR_SIZE", "24")
hl.env("HYPRCURSOR_SIZE", "24")

---------------------
---- PERMISSIONS ----
---------------------
hl.permission("/usr/(bin|local/bin)/grim", "screencopy", "allow")

-----------------------
---- LOOK AND FEEL ----
-----------------------
hl.config({
    general = {
        border_size = 2,
        gaps_in = 5,
        gaps_out = { top = 5, right = 10, bottom = 5, left = 10 },
        col = {
            inactive_border = "rgba(595959aa)",
            active_border = { colors = { "rgba(33ccffee)", "rgba(00ff99ee)" }, angle = 45 },
        },
        layout = "dwindle",
        resize_on_border = true,
    },
    decoration = { rounding = 10, active_opacity = 1, blur = { enabled = true, size = 3 } },
    misc = { force_default_wallpaper = -1 },
})

----------------
---- CURVES ----
----------------
hl.curve("easeOutQuint", { points = { {0.23, 1}, {0.32, 1} }, type = "bezier" })

--------------------
---- ANIMATIONS ----
--------------------
hl.animation({ leaf = "windows", enabled = true, speed = 4.79, bezier = "easeOutQuint" })
hl.animation({ leaf = "fade", enabled = false })

-------------------------
---- WORKSPACE RULES ----
-------------------------
hl.workspace_rule({ workspace = "1", monitor = "DP-1", default = true })
hl.workspace_rule({ workspace = "special:magic", gaps_in = 0, no_border = true, on_created_empty = "kitty" })

----------------------
---- WINDOW RULES ----
----------------------
hl.window_rule({ float = true, match = { class = "^(pavucontrol)$" } })
hl.window_rule({ size = "800 600", match = { class = "^(pavucontrol)$" } })
hl.window_rule({ opacity = 0.9, match = { title = "^(.*Firefox.*)$" } })

---------------------
---- LAYER RULES ----
---------------------
hl.layer_rule({ blur = true, match = { namespace = "^(waybar)$" } })

-----------------
---- DEVICES ----
-----------------
hl.device({ name = "epic-mouse-v1", sensitivity = -0.5, natural_scroll = 1 })

-----------------
---- PLUGINS ----
-----------------
hl.plugin.load("/path/to/plugin.so")

---------------------
---- KEYBINDINGS ----
---------------------
hl.bind("SUPER + Q", hl.dsp.exec_cmd("kitty"))
hl.bind("SUPER + C", hl.dsp.window.close())
hl.bind("SUPER + SHIFT + S", hl.dsp.window.move({ workspace = "special:magic", follow = false }))
hl.bind("SUPER + left", hl.dsp.focus({ direction = "left" }))
hl.bind("XF86AudioRaiseVolume", hl.dsp.exec_cmd("wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+"), { locked = true, repeating = true })
hl.bind("SUPER + mouse:272", hl.dsp.window.drag(), { mouse = true })
hl.bind("SUPER + T", hl.dsp.exec_cmd("kitty"), { description = "open terminal" })
-- bind = SUPER, X, somethingremoved, arg

-- hyprlang2lua could not convert the following faithfully:
--   bind = SUPER, X, somethingremoved, arg: dispatcher "somethingremoved" has no known Lua equivalent
--   Config error in file test/fixtures/basic.conf at line 25: config option <misc:vfr> does not exist.