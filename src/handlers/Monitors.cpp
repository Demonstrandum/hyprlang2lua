// monitor, workspace and device conversion.
//
// monitor = <output>, <mode>, <position>, <scale> [, <key>, <value>]... is the positional
// form CMonitorRuleParser reads; hl.monitor takes the same information as named fields
// (MONITOR_FIELDS in LuaBindingsConfigRules.cpp).
//
// workspace = <selector>, <rule>:<value>, ... maps onto hl.workspace_rule, where several
// legacy rules invert: "border:false" is the rule "no_border".

#include <format>
#include <optional>
#include <string>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>

#include "../Converter.hpp"

using namespace H2L;
using namespace Hyprutils::String;

namespace {
    PLuaValue scalarValue(std::string_view raw) {
        const auto VALUE = trim(std::string{raw});

        if (VALUE == "true" || VALUE == "yes" || VALUE == "on")
            return CLuaValue::boolean(true);

        if (VALUE == "false" || VALUE == "no" || VALUE == "off")
            return CLuaValue::boolean(false);

        if (isNumber(VALUE, true)) {
            try {
                return isNumber(VALUE, false) ? CLuaValue::integer(std::stoll(VALUE)) : CLuaValue::number(std::stod(VALUE));
            } catch (...) { return CLuaValue::string(VALUE); }
        }

        return CLuaValue::string(VALUE);
    }

    // legacy monitor key -> hl.monitor field, where the names differ
    std::string monitorField(const std::string& key) {
        if (key == "bitdepth")
            return "bitdepth";
        if (key == "transform")
            return "transform";
        if (key == "mirror")
            return "mirror";
        if (key == "vrr")
            return "vrr";
        if (key == "cm")
            return "cm";
        if (key == "sdrbrightness")
            return "sdrbrightness";
        if (key == "sdrsaturation")
            return "sdrsaturation";
        if (key == "icc")
            return "icc";
        return key;
    }
}

static Hyprlang::CParseResult handleMonitor(const char*, const char* value) {
    Hyprlang::CParseResult result;

    auto*                  converter = CConverter::active();
    if (!converter)
        return result;

    const auto ARGS = CVarList2(std::string{value});

    auto monitor = CLuaValue::table();
    monitor->set("output", CLuaValue::string(std::string{ARGS[0]}));

    const auto SECOND = std::string{ARGS[1]};

    if (SECOND == "disable" || SECOND == "disabled") {
        monitor->set("disabled", CLuaValue::boolean(true));
        converter->document().addStatement(SECTION_MONITORS, std::format("hl.monitor({})", monitor->render()));
        return result;
    }

    size_t next = 1;

    if (SECOND == "addreserved") {
        // addreserved takes top, bottom, left, right
        auto reserved = CLuaValue::table();
        reserved->set("top", scalarValue(ARGS[2]));
        reserved->set("bottom", scalarValue(ARGS[3]));
        reserved->set("left", scalarValue(ARGS[4]));
        reserved->set("right", scalarValue(ARGS[5]));
        monitor->set("reserved_area", reserved);

        converter->document().addStatement(SECTION_MONITORS, std::format("hl.monitor({})", monitor->render()));
        return result;
    }

    if (SECOND == "transform") {
        monitor->set("transform", scalarValue(ARGS[2]));
        converter->document().addStatement(SECTION_MONITORS, std::format("hl.monitor({})", monitor->render()));
        return result;
    }

    static constexpr std::string_view POSITIONAL[] = {"mode", "position", "scale"};

    for (const auto& field : POSITIONAL) {
        const auto ARG = std::string{ARGS[next++]};
        if (ARG.empty())
            break;
        monitor->set(field, CLuaValue::string(ARG));
    }

    // whatever follows is <key>, <value> pairs
    while (!ARGS[next].empty()) {
        const auto KEY = std::string{ARGS[next]};
        const auto VAL = std::string{ARGS[next + 1]};

        if (VAL.empty()) {
            converter->document().addWarning(std::format("monitor = {}: trailing \"{}\" has no value", value, KEY));
            break;
        }

        monitor->set(monitorField(KEY), scalarValue(VAL));
        next += 2;
    }

    converter->document().addStatement(SECTION_MONITORS, std::format("hl.monitor({})", monitor->render()));
    return result;
}

static Hyprlang::CParseResult handleWorkspace(const char*, const char* value) {
    Hyprlang::CParseResult result;

    auto*                  converter = CConverter::active();
    if (!converter)
        return result;

    const auto ARGS = CVarList2(std::string{value});

    if (ARGS[0].empty()) {
        result.setError("workspace: no workspace selector");
        return result;
    }

    auto rule = CLuaValue::table();
    rule->set("workspace", CLuaValue::string(std::string{ARGS[0]}));

    for (size_t i = 1; !ARGS[i].empty(); ++i) {
        const auto ITEM  = std::string{ARGS[i]};
        const auto COLON = ITEM.find(':');

        if (COLON == std::string::npos) {
            converter->document().addWarning(std::format("workspace = {}: rule \"{}\" has no value", value, ITEM));
            continue;
        }

        const auto KEY = trim(ITEM.substr(0, COLON));
        const auto VAL = trim(ITEM.substr(COLON + 1));

        // the rules whose sense flips between the two languages
        if (KEY == "border") {
            rule->set("no_border", CLuaValue::boolean(!(VAL == "true" || VAL == "1" || VAL == "yes")));
            continue;
        }

        if (KEY == "rounding") {
            rule->set("no_rounding", CLuaValue::boolean(!(VAL == "true" || VAL == "1" || VAL == "yes")));
            continue;
        }

        if (KEY == "shadow") {
            rule->set("no_shadow", CLuaValue::boolean(!(VAL == "true" || VAL == "1" || VAL == "yes")));
            continue;
        }

        static constexpr std::pair<std::string_view, std::string_view> RENAMES[] = {
            {"gapsin", "gaps_in"}, {"gapsout", "gaps_out"},   {"floatgaps", "float_gaps"},       {"bordersize", "border_size"},
            {"layoutopt", "layout"}, {"defaultName", "default_name"}, {"on-created-empty", "on_created_empty"},
        };

        std::string field = KEY;
        for (const auto& [from, to] : RENAMES) {
            if (KEY != from)
                continue;
            field = to;
            break;
        }

        rule->set(field, scalarValue(VAL));
    }

    converter->document().addStatement(SECTION_WORKSPACE_RULES, std::format("hl.workspace_rule({})", rule->render()));
    return result;
}

void CConverter::registerMonitorHandlers() {
    m_config->registerHandler(&::handleMonitor, "monitor", {false});
    m_config->registerHandler(&::handleWorkspace, "workspace", {false});
}
