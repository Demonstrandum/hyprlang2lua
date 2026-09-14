# Where the Hyprland sources come from

Nothing from Hyprland is copied into this repository. Two submodules of the same
upstream repo are pinned at the two commits this converter needs, and the build reads
the files directly out of them.

| submodule | commit | why |
| --- | --- | --- |
| `third_party/hyprland-legacy` | `4588b295` | last commit that still had the hyprlang config: `src/config/legacy/` (keyword handlers, dispatcher translation) and the option table those handlers registered |
| `third_party/hyprland` | `c26dbf93` | the Lua config the converter writes for: `src/config/lua/` bindings, `src/desktop/rule/` effect and match names, current `src/config/values/ConfigValues.cpp` |

`4588b295` is the parent of `a9902ea6` ("config: remove legacy config support", #15539),
which deleted `src/config/legacy/`.

The input language is hyprlang, which is frozen and is linked as a library from nixpkgs,
so its parser is used rather than reproduced.

Files under `shim/` are the only stand-ins written here: they replace compositor-only
headers (the logger, the prop refresher) that the vendored value types include but a
converter has no use for.
