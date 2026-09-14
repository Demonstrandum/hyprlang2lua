#pragma once

// Stand-in for Hyprland's src/macros.hpp.
//
// The vendored value types pull in macros.hpp for RASSERT, STRVAL_EMPTY and the cast
// shorthands. The real header drags in the compositor's logger, which a converter has
// no use for, so only the pieces the vendored files actually reference live here.

#include <cstdint>
#include <cstdlib>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <utility>

#define STRVAL_EMPTY "[[EMPTY]]"

#define RASSERT(expr, reason, ...)                                                                                                                                                 \
    do {                                                                                                                                                                           \
        if (!(expr)) {                                                                                                                                                             \
            std::println(stderr, "hyprlang2lua: assertion failed at {}:{}: {}", __FILE__, __LINE__, std::format(reason __VA_OPT__(, ) __VA_ARGS__));                                \
            std::abort();                                                                                                                                                          \
        }                                                                                                                                                                          \
    } while (false)

#define ASSERT(expr) RASSERT(expr, "?")

#define UNREACHABLE()                                                                                                                                                              \
    do {                                                                                                                                                                           \
        std::println(stderr, "hyprlang2lua: unreachable at {}:{}", __FILE__, __LINE__);                                                                                            \
        std::abort();                                                                                                                                                              \
    } while (false)
