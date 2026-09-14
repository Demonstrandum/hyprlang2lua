#include <filesystem>
#include <fstream>
#include <print>
#include <span>
#include <string>
#include <vector>

#include "Converter.hpp"

using namespace H2L;

static void usage() {
    std::println(stderr, R"(hyprlang2lua - converts a legacy Hyprland .conf into the Lua config format

usage:
  hyprlang2lua <config.conf> [-o <out.lua>]

with no -o, the Lua goes to stdout. Conversion notes and anything that could not
be translated are written into the output as comments, and to stderr.)");
}

int main(int argc, char** argv) {
    const auto               ARGS = std::span(argv, static_cast<size_t>(argc)).subspan(1);

    std::string              input;
    std::string              output;

    for (size_t i = 0; i < ARGS.size(); ++i) {
        const std::string ARG = ARGS[i];

        if (ARG == "-h" || ARG == "--help") {
            usage();
            return 0;
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

    if (input.empty()) {
        usage();
        return 2;
    }

    if (!std::filesystem::exists(input)) {
        std::println(stderr, "hyprlang2lua: {} does not exist", input);
        return 1;
    }

    CConverter converter;
    converter.convert(input);

    const auto LUA = converter.document().render();

    if (output.empty())
        std::print("{}", LUA);
    else {
        std::ofstream file(output, std::ios::trunc);
        if (!file.good()) {
            std::println(stderr, "hyprlang2lua: cannot write {}", output);
            return 1;
        }
        file << LUA;
    }

    for (const auto& e : converter.errors())
        std::println(stderr, "hyprlang2lua: {}: {}", e.file, e.message);

    return converter.errors().empty() ? 0 : 1;
}
