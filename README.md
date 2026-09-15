# hyprlang2lua

converts a legacy hyprland `.conf` into the lua config format.

## install

grab a binary from the releases page. they are statically linked, so there is nothing else to install.

```sh
curl -LO https://github.com/Demonstrandum/hyprlang2lua/releases/latest/download/hyprlang2lua-0.1.1-x86_64-linux
chmod +x hyprlang2lua-0.1.1-x86_64-linux
```

to build instead, with nix:

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
nix develop                           # shell with the toolchain and deps
./tools/build.sh                      # buck2 build, output at build/hyprlang2lua
./tools/test.sh                       # convert the fixtures and check the output
./tools/nix-run.sh ./tools/build.sh   # same, inside a linux container (macos)
buck2 build //:hyprlang2lua           # the graph directly, flags from the nix shell
```
