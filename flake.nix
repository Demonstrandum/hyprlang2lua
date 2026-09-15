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
          version = "0.1.0"; # keep in step with VERSION
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

        # The release artefacts: one file, no nix store paths, no shared library hunt on
        # the user's machine. hyprlang and hyprutils only ship a shared library by default,
        # so the static set builds them with BUILD_SHARED_LIBS off.
        packages.static =
          let
            # both projects hardcode add_library(... SHARED ...), so BUILD_SHARED_LIBS has
            # nothing to switch off and the target has to be rewritten
            asStatic = drv: name:
              drv.overrideAttrs (old: {
                postPatch = (old.postPatch or "") + ''
                  substituteInPlace CMakeLists.txt \
                    --replace-fail "add_library(${name} SHARED" "add_library(${name} STATIC"
                '';
              });

            static = pkgs.pkgsStatic.extend (final: prev: {
              hyprutils = asStatic prev.hyprutils "hyprutils";
              hyprlang = asStatic prev.hyprlang "hyprlang";
            });
          in
          static.hyprlang.stdenv.mkDerivation {
            # note: `nix build .#static` needs the submodules in the flake source, i.e.
            # `nix build "git+file://$PWD?submodules=1#static"`. tools/release.sh builds
            # through devShells.static instead, which sees the working tree directly.
            pname = "hyprlang2lua-static";
            version = "0.1.0"; # keep in step with VERSION
            src = ./.;

            nativeBuildInputs = [ pkgs.pkg-config ];
            buildInputs = [ static.hyprlang static.hyprutils static.wayland static.libxkbcommon ];

            buildPhase = ''
              runHook preBuild
              export HYPRGRAPHICS_INCLUDE=${pkgs.hyprgraphics.dev}/include
              STATIC=1 PKGS="hyprlang hyprutils" ./tools/build.sh
              runHook postBuild
            '';

            installPhase = ''
              runHook preInstall
              install -Dm755 build/hyprlang2lua $out/bin/hyprlang2lua
              runHook postInstall
            '';

            meta.mainProgram = "hyprlang2lua";
          };

        # the same toolchain and libraries as packages.static, for building from the
        # working tree (submodules included) rather than from a flake source copy
        devShells.static =
          let
            static = pkgs.pkgsStatic.extend (final: prev: {
              hyprutils = prev.hyprutils.overrideAttrs (old: {
                postPatch = (old.postPatch or "") + ''
                  substituteInPlace CMakeLists.txt --replace-fail "add_library(hyprutils SHARED" "add_library(hyprutils STATIC"
                '';
              });
              hyprlang = prev.hyprlang.overrideAttrs (old: {
                postPatch = (old.postPatch or "") + ''
                  substituteInPlace CMakeLists.txt --replace-fail "add_library(hyprlang SHARED" "add_library(hyprlang STATIC"
                '';
              });
            });
          in
          (pkgs.mkShell.override { stdenv = static.hyprlang.stdenv; }) {
            packages = [ pkgs.pkg-config ];
            buildInputs = [ static.hyprlang static.hyprutils static.wayland static.libxkbcommon ];

            # hyprgraphics is needed for its headers only: Hyprland's Color.hpp includes
            # them, and src/Color.cpp replaces the implementation that would link the
            # library (see the comment there).
            shellHook = ''
              export HYPRGRAPHICS_INCLUDE=${pkgs.hyprgraphics.dev}/include
            '';
          };

        checks.default = self.packages.${system}.default;
      });
}
