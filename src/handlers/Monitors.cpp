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

#include "SpecialValues.gen.hpp"

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

// gesture = <fingers>, <direction>, [mod:MOD,] [scale:N,] <action> [, args...]
static Hyprlang::CParseResult handleGesture(const char* command, const char* value) {
    Hyprlang::CParseResult result;

    auto*                  converter = CConverter::active();
    if (!converter)
        return result;

    const auto DATA = CVarList2(std::string{value});

    auto       gesture = CLuaValue::table();

    try {
        const auto FINGERS = std::stoul(std::string{DATA[0]});

        if (FINGERS <= 1 || FINGERS >= 10) {
            result.setError("gesture: finger count must be between 2 and 9");
            return result;
        }

        gesture->set("fingers", CLuaValue::integer(static_cast<int64_t>(FINGERS)));
    } catch (...) {
        result.setError("gesture: invalid finger count");
        return result;
    }

    if (DATA[1].empty()) {
        result.setError("gesture: no direction");
        return result;
    }

    gesture->set("direction", CLuaValue::string(std::string{DATA[1]}));

    size_t index = 2;

    while (true) {
        const auto ITEM = std::string{DATA[index]};

        if (ITEM.starts_with("mod:")) {
            gesture->set("mods", CLuaValue::string(ITEM.substr(4)));
            ++index;
            continue;
        }

        if (ITEM.starts_with("scale:")) {
            gesture->set("scale", scalarValue(ITEM.substr(6)));
            ++index;
            continue;
        }

        break;
    }

    const auto ACTION = std::string{DATA[index]};

    if (ACTION.empty()) {
        result.setError("gesture: no action");
        return result;
    }

    gesture->set("action", CLuaValue::string(ACTION));

    // what follows the action is action-specific, and hl.gesture names each one
    const auto FIRST_ARG  = std::string{DATA[index + 1]};
    const auto SECOND_ARG = std::string{DATA[index + 2]};

    if (!FIRST_ARG.empty()) {
        if (ACTION == "special")
            gesture->set("workspace_name", CLuaValue::string(FIRST_ARG));
        else if (ACTION == "float" || ACTION == "fullscreen")
            gesture->set("mode", CLuaValue::string(FIRST_ARG));
        else if (ACTION == "cursor_zoom" || ACTION == "cursorZoom") {
            gesture->set("zoom_level", scalarValue(FIRST_ARG));
            if (!SECOND_ARG.empty())
                gesture->set("mode", CLuaValue::string(SECOND_ARG));
        } else
            converter->document().addWarning(std::format("gesture = {}: arguments after the \"{}\" action are not converted", value, ACTION));
    }

    if (ACTION == "dispatcher")
        converter->document().addWarning(std::format("gesture = {}: a dispatcher gesture becomes a Lua callback; write it as a function field", value));

    if (std::string_view{command}.substr(7).contains('p'))
        gesture->set("disable_inhibit", CLuaValue::boolean(true));

    converter->document().addStatement(SECTION_GESTURES, std::format("hl.gesture({})", gesture->render()));
    return result;
}

void CConverter::registerMonitorHandlers() {
    m_config->registerHandler(&::handleMonitor, "monitor", {false});
    m_config->registerHandler(&::handleWorkspace, "workspace", {false});
    m_config->registerHandler(&::handleGesture, "gesture", {true});

    // monitorv2 { output = ..., mode = ..., ... } blocks, keyed by output
    m_config->addSpecialCategory("monitorv2", {.key = "output"});

    for (const auto& [name, type] : MONITORV2_VALUES) {
        const std::string KEY{name};

        switch (type) {
            case SPECIAL_VALUE_INT: m_config->addSpecialConfigValue("monitorv2", KEY.c_str(), Hyprlang::INT{0}); break;
            case SPECIAL_VALUE_FLOAT: m_config->addSpecialConfigValue("monitorv2", KEY.c_str(), Hyprlang::FLOAT{0.F}); break;
            case SPECIAL_VALUE_STRING: m_config->addSpecialConfigValue("monitorv2", KEY.c_str(), Hyprlang::STRING{""}); break;
        }
    }
}

void CConverter::emitMonitorBlocks() {
    for (const auto& OUTPUT : m_config->listKeysForSpecialCategory("monitorv2")) {
        auto monitor = CLuaValue::table();
        monitor->set("output", CLuaValue::string(OUTPUT));

        for (const auto& [name, type] : MONITORV2_VALUES) {
            const std::string KEY{name};
            const auto        PTR = m_config->getSpecialConfigValuePtr("monitorv2", KEY.c_str(), OUTPUT.c_str());

            if (!PTR || !PTR->m_bSetByUser)
                continue;

            // addreserved is four numbers in one string; hl.monitor takes them as a table
            if (KEY == "addreserved") {
                const auto AREA     = CVarList2(std::string{std::any_cast<Hyprlang::STRING>(PTR->getValue())}, 0, ' ');
                auto       reserved = CLuaValue::table();
                reserved->set("top", scalarValue(AREA[0]));
                reserved->set("bottom", scalarValue(AREA[1]));
                reserved->set("left", scalarValue(AREA[2]));
                reserved->set("right", scalarValue(AREA[3]));
                monitor->set("reserved_area", reserved);
                continue;
            }

            switch (type) {
                case SPECIAL_VALUE_INT: {
                    const auto VALUE = std::any_cast<Hyprlang::INT>(PTR->getValue());
                    // the flags hyprlang stores as 0/1 are booleans on the Lua side
                    if (KEY == "disabled" || KEY == "vrr" || KEY == "supports_wide_color" || KEY == "supports_hdr")
                        monitor->set(KEY, CLuaValue::boolean(VALUE != 0));
                    else
                        monitor->set(KEY, CLuaValue::integer(VALUE));
                    break;
                }
                case SPECIAL_VALUE_FLOAT: monitor->set(KEY, CLuaValue::number(static_cast<double>(std::any_cast<Hyprlang::FLOAT>(PTR->getValue())))); break;
                case SPECIAL_VALUE_STRING: monitor->set(KEY, CLuaValue::string(std::any_cast<Hyprlang::STRING>(PTR->getValue()))); break;
            }
        }

        m_document.addStatement(SECTION_MONITORS, std::format("hl.monitor({})", monitor->render()));
    }
}
