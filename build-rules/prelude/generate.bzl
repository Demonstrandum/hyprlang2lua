# Generated sources: the tables derived from the pinned Hyprland submodules.
#
# These exist as rules rather than as a step in a shell script so that a change to a
# generator, or to the source it reads, rebuilds exactly what depends on it.

def _generated_header_impl(ctx: AnalysisContext) -> list[Provider]:
    out = ctx.actions.declare_output(ctx.attrs.out)

    ctx.actions.run(
        cmd_args(ctx.attrs.generator, ctx.attrs.args, out.as_output()),
        category = "generate",
        identifier = ctx.attrs.out,
    )

    return [DefaultInfo(default_output = out)]

generated_header = rule(
    impl = _generated_header_impl,
    attrs = {
        "generator": attrs.source(),
        "args": attrs.list(attrs.string(), default = []),
        "out": attrs.string(),
    },
)
