# The build graph.
#
# Three groups, and the split is not cosmetic: this project's sources carry -Werror with
# every warning turned on, while Hyprland's sources are included with -isystem and built
# without it, because their warnings belong to Hyprland's build.

load("@prelude//prelude.bzl", "cxx_binary", "cxx_library", "generated_header")

LEGACY = "third_party/hyprland-legacy/src"

CURRENT = "third_party/hyprland/src"

WARNINGS = [
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wshadow",
    "-Wnon-virtual-dtor",
    "-Wcast-align",
    "-Wunused",
    "-Woverloaded-virtual",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wnull-dereference",
    "-Wdouble-promotion",
    "-Wformat=2",
    "-Wimplicit-fallthrough",
    "-Werror",
]

# ---------------------------------------------------------------- generated tables

generated_header(
    name = "special_values",
    generator = "tools/gen_special_values.sh",
    args = [LEGACY + "/config/legacy/ConfigManager.cpp"],
    out = "SpecialValues.gen.hpp",
)

generated_header(
    name = "rule_names",
    generator = "tools/gen_rule_names.sh",
    args = [CURRENT],
    out = "RuleNames.gen.hpp",
)

# ---------------------------------------------------------------- Hyprland's own sources

cxx_library(
    name = "hyprland_reused",
    srcs = glob([
        LEGACY + "/config/values/*.cpp",
        LEGACY + "/config/values/types/*.cpp",
        LEGACY + "/config/shared/parserUtils/ParserUtils.cpp",
        LEGACY + "/helpers/env/Env.cpp",
    ]),
    include_dirs = [LEGACY],
)

# ---------------------------------------------------------------- this project

cxx_library(
    name = "converter",
    srcs = glob([
        "src/*.cpp",
        "src/handlers/*.cpp",
    ]),
    system_include_dirs = [LEGACY],
    generated_headers = [
        ":special_values",
        ":rule_names",
    ],
    flags = WARNINGS,
)

cxx_binary(
    name = "hyprlang2lua",
    out = "hyprlang2lua",
    deps = [
        ":converter",
        ":hyprland_reused",
    ],
)
