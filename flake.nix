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

        # The release artefacts: one file, no nix store paths, no shared library hunt on
        # the user's machine.
        #
        # hyprlang and hyprutils both hardcode add_library(... SHARED ...), so
        # BUILD_SHARED_LIBS has nothing to switch off and the target is rewritten instead.
        # staticShellFor takes a package set so that the x86_64 artefact can be built by a
        # cross toolchain running natively rather than by an emulated one.
        staticShellFor = base:
          let
            asStatic = drv: name:
              drv.overrideAttrs (old: {
                postPatch = (old.postPatch or "") + ''
                  substituteInPlace CMakeLists.txt \
                    --replace-fail "add_library(${name} SHARED" "add_library(${name} STATIC"
                '';
              });

            static = base.pkgsStatic.extend (final: prev: {
              hyprutils = asStatic prev.hyprutils "hyprutils";
              hyprlang = asStatic prev.hyprlang "hyprlang";
            });
          in
          (pkgs.mkShell.override { stdenv = static.hyprlang.stdenv; }) {
            # luajit and binutils are for tools/test.sh and the strip in tools/release.sh,
            # which run inside this shell; qemu runs the foreign-architecture binary so a
            # cross-built artefact is still tested rather than assumed
            packages = with pkgs; [ pkg-config luajit binutils qemu upx ];
            buildInputs = [ static.hyprlang static.hyprutils static.wayland static.libxkbcommon ];

            # hyprgraphics is needed for its headers only: Hyprland's Color.hpp includes
            # them, and src/Color.cpp replaces the implementation that would link the
            # library (see the comment there).
            shellHook = ''
              export HYPRGRAPHICS_INCLUDE=${pkgs.hyprgraphics.dev}/include
            '';
          };

      in
      {
        packages.default = pkgs.hyprlang.stdenv.mkDerivation {
          pname = "hyprlang2lua";
          version = "0.1.2"; # keep in step with VERSION
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

        devShells.static = staticShellFor pkgs;
        devShells.static-x86_64 = staticShellFor pkgs.pkgsCross.musl64;
        devShells.static-aarch64 = staticShellFor pkgs.pkgsCross.aarch64-multiplatform-musl;

        checks.default = self.packages.${system}.default;
      });
}
