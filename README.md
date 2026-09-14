# hyprlang2lua

converts a legacy hyprland `.conf` into the lua config format.

## install

needs nix. the dependencies are linux-only, so on macos run it through the container wrapper.

```sh
git clone --recurse-submodules https://github.com/Demonstrandum/hyprlang2lua.git
cd hyprlang2lua
nix build
```

## run

```sh
./result/bin/hyprlang2lua ~/.config/hypr/hyprland.conf -o ~/.config/hypr/hyprland.lua
```

with no `-o`, the lua goes to stdout. anything that could not be converted is written
into the output as a comment and to stderr.

## develop

```sh
nix develop                # shell with the toolchain and deps
./tools/build.sh           # build to build/hyprlang2lua
./tools/nix-run.sh ./tools/build.sh   # same, inside a linux container (macos)
```
