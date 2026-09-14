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

        deps = with pkgs; [ hyprlang hyprutils hyprgraphics pixman libdrm ];

        nativeDeps = with pkgs; [ pkg-config cmake ];
      in
      {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "hyprlang2lua";
          version = "0.1.0";
          src = ./.;

          nativeBuildInputs = nativeDeps;
          buildInputs = deps;

          # buck2 is the development build; the nix package uses a direct compile of the
          # same sources so that `nix build` works without a buck2 daemon in the sandbox.
          buildPhase = ''
            runHook preBuild
            mkdir -p build
            $CXX -std=c++26 -O2 -o build/hyprlang2lua \
              -Ishim -Ivendor \
              $(pkg-config --cflags hyprlang hyprutils hyprgraphics pixman-1) \
              src/*.cpp vendor/config/values/*.cpp vendor/config/values/types/*.cpp vendor/helpers/*.cpp \
              $(pkg-config --libs hyprlang hyprutils hyprgraphics pixman-1)
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

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [ buck2 clang-tools lua luajit gnumake git ] ++ nativeDeps ++ deps;

          shellHook = ''
            export HYPRLANG2LUA_CXXFLAGS="$(pkg-config --cflags hyprlang hyprutils hyprgraphics pixman-1)"
            export HYPRLANG2LUA_LDFLAGS="$(pkg-config --libs hyprlang hyprutils hyprgraphics pixman-1)"
          '';
        };

        checks.default = self.packages.${system}.default;
      });
}
