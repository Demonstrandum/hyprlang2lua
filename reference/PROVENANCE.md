# Provenance of the files in `reference/`

Nothing in this directory is written by this project. Every file is a verbatim copy,
kept so that the conversion tables in `data/` can be regenerated (and audited) against
the exact sources they were derived from.

## Hyprland

`legacy/` is the hyprlang config implementation as it existed in the last commit that
still had it, i.e. the parent of the commit that deleted it:

- base commit: `4588b2958e0dcd6199aa258b19de9dc944f5947e`
  (parent of `a9902ea6` "config: remove legacy config support (#15539)")
- `legacy/ConfigManager.cpp`          (2184 lines) hyprlang keyword handlers + option registration
- `legacy/ConfigManager.hpp`
- `legacy/DefaultConfig.hpp`
- `legacy/DispatcherTranslator.cpp`   (868 lines) legacy dispatcher name + arg string -> Config::Actions
- `legacy/DispatcherTranslator.hpp`
- `values/ConfigValues.cpp.base`      option table as of the base commit

`lua/`, `values/ConfigValues.cpp` and `rule/` are from Hyprland `main`, and describe the
target the converter emits for:

- `values/ConfigValues.cpp`           current option table (name + type)
- `lua/LuaBindingsConfigRules.cpp`    hl.config / hl.monitor / hl.window_rule / ... bindings
- `lua/LuaBindingsDispatchers.cpp`    hl.dsp.* tree and the dsp_ -> Config::Actions calls
- `lua/LuaBindingsToplevel.cpp`       hl.bind / hl.on / hl.submap / ...
- `lua/hyprland.lua`                  shipped example Lua config
- `rule/Rule.cpp`                     window/layer rule match property names
- `rule/WindowRuleEffectContainer.cpp` window rule effect names
- `rule/LayerRuleEffectContainer.cpp`  layer rule effect names

## hyprlang

`hyprlang/` is the parser whose grammar this project re-implements. The `.conf` language
is frozen, so this is a fixed target.

- upstream commit: `9508458be316a0d70d37ebed1ab725ccd10411ff`
- `hyprlang/config.cpp`, `config.hpp`, `hyprlang.hpp`
