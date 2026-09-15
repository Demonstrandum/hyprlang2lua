# A single execution platform: whatever machine buck2 is running on, with the toolchain
# that the surrounding nix shell provides.

def _host_platform_impl(ctx: AnalysisContext) -> list[Provider]:
    configuration = ConfigurationInfo(constraints = {}, values = {})

    platform = ExecutionPlatformInfo(
        label = ctx.label.raw_target(),
        configuration = configuration,
        executor_config = CommandExecutorConfig(
            local_enabled = True,
            remote_enabled = False,
        ),
    )

    return [
        DefaultInfo(),
        configuration,
        PlatformInfo(label = str(ctx.label.raw_target()), configuration = configuration),
        ExecutionPlatformRegistrationInfo(platforms = [platform]),
    ]

host_platform = rule(impl = _host_platform_impl, attrs = {})
