// Keyword conversion.
//
// Unlike options, keywords are positional mini-languages, one per keyword. Each converter
// below follows the parsing half of the matching handler in
// third_party/hyprland-legacy/src/config/legacy/ConfigManager.cpp, and replaces the half
// that applied the result to a running compositor with the hl.* call that says the same
// thing.

#include <format>
#include <string>
#include <vector>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>

#include "../Converter.hpp"

using namespace H2L;
using namespace Hyprutils::String;

namespace {
    // every handler is registered through this, so a keyword that appears in a config but
    // has no converter is reported rather than silently dropped
    Hyprlang::CParseResult report(const std::optional<std::string>& error) {
        Hyprlang::CParseResult result;

        if (error)
            result.setError(error->c_str());

        return result;
    }

    std::string luaCall(std::string_view fn, const std::vector<std::string>& args) {
        std::string out{fn};
        out += "(";
        for (size_t i = 0; i < args.size(); ++i)
            out += std::format("{}{}", args[i], i + 1 < args.size() ? ", " : "");
        return out + ")";
    }
}

// ---------------------------------------------------------------- exec family
//
// exec / exec-once both run at startup, and the Lua config expresses that as a
// hyprland.start subscription. The converter collects them and writes one subscription at
// the end, so the output has a single autostart block like the shipped example config.

static Hyprlang::CParseResult handleExec(const char* command, const char* value) {
    auto* converter = CConverter::active();
    if (!converter)
        return {};

    const std::string KEYWORD = command;
    const std::string ARGS    = value;

    if (KEYWORD == "exec-shutdown") {
        converter->addShutdownExec(ARGS);
        return {};
    }

    // execr / execr-once bypass the shell. The Lua API's hl.exec_cmd always goes through
    // the executor, so the distinction is recorded rather than dropped.
    if (KEYWORD.starts_with("execr"))
        converter->document().addWarning(std::format("{} = {}: converted to hl.exec_cmd, which runs through a shell like exec does", KEYWORD, ARGS));

    converter->addStartupExec(ARGS);
    return {};
}

// ---------------------------------------------------------------- env

static Hyprlang::CParseResult handleEnv(const char* command, const char* value) {
    auto* converter = CConverter::active();
    if (!converter)
        return {};

    const std::string KEYWORD = command;
    const auto        ARGS    = CVarList(value, 2);

    if (ARGS[0].empty())
        return report("env: empty variable name");

    converter->document().addStatement(SECTION_ENV, luaCall("hl.env", {quoteLuaString(ARGS[0]), quoteLuaString(ARGS[1])}));

    if (KEYWORD.back() == 'd')
        converter->document().addWarning(std::format("envd = {}: hl.env does not push the variable to dbus; add an exec if you need that", value));

    return {};
}

// ---------------------------------------------------------------- plugin, permission

static Hyprlang::CParseResult handlePlugin(const char*, const char* value) {
    auto* converter = CConverter::active();
    if (!converter)
        return {};

    converter->document().addStatement(SECTION_PLUGINS, luaCall("hl.plugin.load", {quoteLuaString(value)}));
    return {};
}

static Hyprlang::CParseResult handlePermission(const char*, const char* value) {
    auto* converter = CConverter::active();
    if (!converter)
        return {};

    const auto DATA = CVarList(value, 3);

    if (DATA[0].empty() || DATA[1].empty() || DATA[2].empty())
        return report("permission: expected <regex>, <type>, <mode>");

    converter->document().addStatement(SECTION_PERMISSIONS, luaCall("hl.permission", {quoteLuaString(DATA[0]), quoteLuaString(DATA[1]), quoteLuaString(DATA[2])}));
    return {};
}

// ---------------------------------------------------------------- curves and animations

static Hyprlang::CParseResult handleBezier(const char*, const char* value) {
    auto* converter = CConverter::active();
    if (!converter)
        return {};

    const auto ARGS = CVarList(value);

    if (ARGS[0].empty())
        return report("bezier: no name");

    for (size_t i = 1; i <= 4; ++i) {
        if (ARGS[i].empty())
            return report("bezier: too few arguments");
        if (!isNumber(ARGS[i], true))
            return report(std::format("bezier: {} is not a point", ARGS[i]));
    }

    if (!ARGS[5].empty())
        return report("bezier: too many arguments");

    const auto POINTS = std::format("{{ points = {{ {{{}, {}}}, {{{}, {}}} }}, type = \"bezier\" }}", ARGS[1], ARGS[2], ARGS[3], ARGS[4]);

    converter->document().addStatement(SECTION_CURVES, luaCall("hl.curve", {quoteLuaString(ARGS[0]), POINTS}));
    return {};
}

static Hyprlang::CParseResult handleAnimation(const char*, const char* value) {
    auto* converter = CConverter::active();
    if (!converter)
        return {};

    const auto ARGS = CVarList(value);

    if (ARGS[0].empty())
        return report("animation: no name");

    auto animation = CLuaValue::table();
    animation->set("leaf", CLuaValue::string(ARGS[0]));

    const bool ENABLED = ARGS[1] == "1" || ARGS[1] == "true" || ARGS[1] == "yes" || ARGS[1] == "on";
    animation->set("enabled", CLuaValue::boolean(ENABLED));

    if (ENABLED) {
        if (!ARGS[2].empty() && isNumber(ARGS[2], true))
            animation->set("speed", CLuaValue::number(std::stod(ARGS[2])));

        if (!ARGS[3].empty())
            animation->set("bezier", CLuaValue::string(ARGS[3]));

        if (!ARGS[4].empty())
            animation->set("style", CLuaValue::string(ARGS[4]));
    }

    converter->document().addStatement(SECTION_ANIMATIONS, luaCall("hl.animation", {animation->render()}));
    return {};
}

// ---------------------------------------------------------------- registration

void CConverter::registerKeywordHandlers() {
    m_config->registerHandler(&::handleExec, "exec", {false});
    m_config->registerHandler(&::handleExec, "execr", {false});
    m_config->registerHandler(&::handleExec, "exec-once", {false});
    m_config->registerHandler(&::handleExec, "execr-once", {false});
    m_config->registerHandler(&::handleExec, "exec-shutdown", {false});
    m_config->registerHandler(&::handleEnv, "env", {true});
    m_config->registerHandler(&::handlePlugin, "plugin", {false});
    m_config->registerHandler(&::handlePermission, "permission", {false});
    m_config->registerHandler(&::handleBezier, "bezier", {false});
    m_config->registerHandler(&::handleAnimation, "animation", {false});
}
