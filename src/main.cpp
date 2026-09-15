#include <cstdio>
#include <filesystem>
#include <print>
#include <span>
#include <string>
#include <vector>

#include <hyprutils/path/Path.hpp>

#include "Converter.hpp"

using namespace H2L;

static void usage() {
    std::println(stderr, R"(hyprlang2lua - converts a legacy Hyprland .conf into the Lua config format

usage:
  hyprlang2lua [config.conf] [-o <out.lua>] [-f]

with no input, the .conf is looked up where Hyprland looks for it, and the Lua is
written next to it as hyprland.lua. With no -o and an explicit input, the Lua goes
to stdout. -f overwrites an existing output file.

Conversion notes and anything that could not be translated are written into the
output as comments, and to stderr.)");
}

// the search Hyprland itself does, via the same hyprutils helper its config path code uses
static std::string findLegacyConfig() {
    if (const auto FROM_ENV = getenv("HYPRLAND_CONFIG"); FROM_ENV)
        return FROM_ENV;

    const auto PATHS = Hyprutils::Path::findConfig("hyprland", "conf");

    return PATHS.first.value_or("");
}

int main(int argc, char** argv) {
    const auto               ARGS = std::span(argv, static_cast<size_t>(argc)).subspan(1);

    std::string              input;
    std::string              output;
    bool                     force = false;

    for (size_t i = 0; i < ARGS.size(); ++i) {
        const std::string ARG = ARGS[i];

        if (ARG == "-h" || ARG == "--help") {
            usage();
            return 0;
        }

        if (ARG == "-f" || ARG == "--force") {
            force = true;
            continue;
        }

        if (ARG == "-o") {
            if (i + 1 >= ARGS.size()) {
                std::println(stderr, "hyprlang2lua: -o needs a path");
                return 2;
            }
            output = ARGS[++i];
            continue;
        }

        if (!input.empty()) {
            std::println(stderr, "hyprlang2lua: more than one input file given");
            return 2;
        }

        input = ARG;
    }

    const bool AUTO_FOUND = input.empty();

    if (AUTO_FOUND) {
        input = findLegacyConfig();

        if (input.empty()) {
            std::println(stderr, "hyprlang2lua: no hyprland.conf found in the usual places; pass one explicitly");
            return 1;
        }

        std::println(stderr, "hyprlang2lua: converting {}", input);

        if (output.empty())
            output = (std::filesystem::path{input}.parent_path() / "hyprland.lua").string();
    }

    if (!std::filesystem::exists(input)) {
        std::println(stderr, "hyprlang2lua: {} does not exist", input);
        return 1;
    }

    if (!output.empty() && !force && std::filesystem::exists(output)) {
        std::println(stderr, "hyprlang2lua: {} already exists; pass -f to overwrite", output);
        return 1;
    }

    CConverter converter;
    converter.convert(input);

    const auto LUA = converter.document().render();

    if (output.empty())
        std::print("{}", LUA);
    else {
        std::println(stderr, "hyprlang2lua: writing {}", output);
        // std::ofstream would pull the whole iostreams machinery into a static binary
        // for one write; the file is already a single string by this point
        FILE* file = std::fopen(output.c_str(), "w");

        if (!file) {
            std::println(stderr, "hyprlang2lua: cannot write {}", output);
            return 1;
        }

        std::print(file, "{}", LUA);
        std::fclose(file);
    }

    for (const auto& e : converter.errors())
        std::println(stderr, "hyprlang2lua: {}: {}", e.file, e.message);

    return converter.errors().empty() ? 0 : 1;
}
