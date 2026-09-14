{
  description = "hyprlang2lua: converts legacy Hyprland .conf files to the Lua config format";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs { inherit system; };

        # Math.hpp (pulled in by the value descriptors) includes wayland-server-protocol.h
        # for the transform enum, so the wayland headers are needed even though this
        # program never talks to a compositor.
        deps = with pkgs; [ hyprlang hyprutils hyprgraphics pixman libdrm wayland wayland-protocols libxkbcommon ];

        nativeDeps = with pkgs; [ pkg-config cmake ];
      in
      {
        packages.default = pkgs.hyprlang.stdenv.mkDerivation {
          pname = "hyprlang2lua";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = nativeDeps;
          buildInputs = deps;

          # buck2 is the development build; the nix package uses a direct compile of the
          # same sources so that `nix build` works without a buck2 daemon in the sandbox.
          buildPhase = ''
            runHook preBuild
            ./tools/build.sh
            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall
            install -Dm755 build/hyprlang2lua $out/bin/hyprlang2lua
            runHook postInstall
          '';

          meta = {
            description = "Converts legacy Hyprland hyprlang configs to Lua";
            mainProgram = "hyprlang2lua";
          };
        };

        # hyprlang and hyprutils are built with a newer libstdc++ than the default stdenv
        # ships, so the shell and the package both use the compiler they were built with.
        devShells.default = (pkgs.mkShell.override { stdenv = pkgs.hyprlang.stdenv; }) {
          packages = with pkgs; [ buck2 lua luajit gnumake git ] ++ nativeDeps ++ deps;

          shellHook = ''
            export HYPRLANG2LUA_CXXFLAGS="$(pkg-config --cflags hyprlang hyprutils hyprgraphics pixman-1)"
            export HYPRLANG2LUA_LDFLAGS="$(pkg-config --libs hyprlang hyprutils hyprgraphics pixman-1)"
          '';
        };

        checks.default = self.packages.${system}.default;
      });
}
