# Toolchain definition, read from the environment nix provides.

load("@prelude//cxx.bzl", "CxxToolchainInfo")

def _toolchain_impl(ctx: AnalysisContext) -> list[Provider]:
    return [
        DefaultInfo(),
        CxxToolchainInfo(
            compiler = read_config("cxx", "compiler", "g++"),
            # empty entries would reach the command line as a "" argument, which the
            # linker reads as a filename
            compiler_flags = [f for f in read_config("cxx", "flags", "-std=c++26 -O2").split(" ") if f],
            linker_flags = [f for f in read_config("cxx", "ldflags", "").split(" ") if f],
        ),
    ]

cxx_toolchain_from_env = rule(
    impl = _toolchain_impl,
    attrs = {},
    is_toolchain_rule = True,
)
