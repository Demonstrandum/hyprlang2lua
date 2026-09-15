# C++ compile and link rules.
#
# One object file per source, so a changed source recompiles only itself; the link step
# depends on every object. Warning flags are per-target, which is how this project keeps
# -Werror on its own sources while including Hyprland's with -isystem.

CxxObjectsInfo = provider(fields = ["objects", "headers"])

def _compile_impl(ctx: AnalysisContext) -> list[Provider]:
    cxx = ctx.attrs._toolchain[CxxToolchainInfo]

    generated = [dep[DefaultInfo].default_outputs[0] for dep in ctx.attrs.generated_headers]

    objects = []
    for src in ctx.attrs.srcs:
        obj = ctx.actions.declare_output("{}/{}.o".format(ctx.label.name, src.short_path.replace("/", "_")))

        cmd = cmd_args(
            cxx.compiler,
            cxx.compiler_flags,
            ctx.attrs.flags,
            ["-I" + d for d in ctx.attrs.include_dirs],
            ["-isystem" + d for d in ctx.attrs.system_include_dirs],
            # a generated header is reached by name, so its directory goes on the path
            cmd_args(generated, format = "-I{}", parent = 1),
            "-c",
            src,
            "-o",
            obj.as_output(),
        )

        # generated headers and vendored trees are inputs even though the command names
        # them only through -I
        cmd.add(cmd_args(hidden = generated))

        ctx.actions.run(cmd, category = "cxx_compile", identifier = src.short_path)
        objects.append(obj)

    return [
        DefaultInfo(default_outputs = objects),
        CxxObjectsInfo(objects = objects, headers = generated),
    ]

cxx_library = rule(
    impl = _compile_impl,
    attrs = {
        "srcs": attrs.list(attrs.source()),
        "flags": attrs.list(attrs.string(), default = []),
        "include_dirs": attrs.list(attrs.string(), default = []),
        "system_include_dirs": attrs.list(attrs.string(), default = []),
        "generated_headers": attrs.list(attrs.dep(), default = []),
        "_toolchain": attrs.toolchain_dep(default = "toolchains//:cxx"),
    },
)

def _binary_impl(ctx: AnalysisContext) -> list[Provider]:
    cxx = ctx.attrs._toolchain[CxxToolchainInfo]

    objects = []
    for dep in ctx.attrs.deps:
        objects.extend(dep[CxxObjectsInfo].objects)

    out = ctx.actions.declare_output(ctx.attrs.out or ctx.label.name)

    ctx.actions.run(
        cmd_args(cxx.compiler, cxx.compiler_flags, objects, "-o", out.as_output(), cxx.linker_flags),
        category = "cxx_link",
        identifier = ctx.label.name,
    )

    return [DefaultInfo(default_output = out), RunInfo(args = cmd_args(out))]

cxx_binary = rule(
    impl = _binary_impl,
    attrs = {
        "deps": attrs.list(attrs.dep()),
        "out": attrs.option(attrs.string(), default = None),
        "_toolchain": attrs.toolchain_dep(default = "toolchains//:cxx"),
    },
)

CxxToolchainInfo = provider(fields = ["compiler", "compiler_flags", "linker_flags"])
