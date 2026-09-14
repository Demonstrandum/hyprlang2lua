// windowrule and layerrule conversion.
//
// Both keywords take a comma separated list of "<field> <value>" items, where a field is
// either an effect name or "match:<property>". That grammar is the one in
// CConfigManager::handleWindowrule / handleLayerrule; the effect and property names come
// out of the rule engine tables via tools/gen_rule_names.sh, so the two sides cannot drift
// apart without the build noticing.

#include <algorithm>
#include <format>
#include <ranges>
#include <string>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList2.hpp>

#include "../Converter.hpp"

#include "RuleNames.gen.hpp"

using namespace H2L;
using namespace Hyprutils::String;

namespace {
    // rule values arrive as text; the Lua side takes the natural type for each
    PLuaValue ruleValue(std::string_view raw);

    PLuaValue ruleValue(std::string_view raw) {
        const auto VALUE = trim(std::string{raw});

        if (VALUE == "true" || VALUE == "yes" || VALUE == "on" || VALUE == "1")
            return CLuaValue::boolean(true);

        if (VALUE == "false" || VALUE == "no" || VALUE == "off" || VALUE == "0")
            return CLuaValue::boolean(false);

        if (isNumber(VALUE, true)) {
            try {
                return isNumber(VALUE, false) ? CLuaValue::integer(std::stoll(VALUE)) : CLuaValue::number(std::stod(VALUE));
            } catch (...) { return CLuaValue::string(VALUE); }
        }

        return CLuaValue::string(VALUE);
    }

    std::optional<std::string> buildRule(std::string_view value, std::span<const std::string_view> effects, CLuaValue& rule) {
        auto match = CLuaValue::table();

        for (const auto& el : CVarList2(std::string{value})) {
            const auto SPACE = el.find(' ');

            if (SPACE == std::string::npos)
                return std::format("invalid field {}: missing a value", el);

            const bool IS_PROP = el.starts_with("match:");
            const auto FIELD   = IS_PROP ? std::string{el.substr(6, SPACE - 6)} : std::string{el.substr(0, SPACE)};
            const auto VALUE   = std::string{el.substr(SPACE + 1)};

            if (IS_PROP) {
                if (!std::ranges::contains(RULE_MATCH_PROPS, FIELD))
                    return std::format("invalid match property {}", FIELD);

                match->set(FIELD, ruleValue(VALUE));
                continue;
            }

            if (!std::ranges::contains(effects, FIELD))
                return std::format("invalid effect {}", FIELD);

            rule.set(FIELD, ruleValue(VALUE));
        }

        if (!match->empty())
            rule.set("match", match);

        return std::nullopt;
    }

    Hyprlang::CParseResult convertRule(std::string_view value, std::span<const std::string_view> effects, std::string_view fn, eSection section) {
        Hyprlang::CParseResult result;

        auto*                  converter = CConverter::active();
        if (!converter)
            return result;

        auto       rule  = CLuaValue::table();
        const auto ERROR = buildRule(value, effects, *rule);

        if (ERROR) {
            result.setError(ERROR->c_str());
            return result;
        }

        converter->document().addStatement(section, std::format("{}({})", fn, rule->render()));
        return result;
    }
}

static Hyprlang::CParseResult handleWindowrule(const char*, const char* value) {
    return convertRule(value, WINDOW_RULE_EFFECTS, "hl.window_rule", SECTION_WINDOW_RULES);
}

static Hyprlang::CParseResult handleLayerrule(const char*, const char* value) {
    return convertRule(value, LAYER_RULE_EFFECTS, "hl.layer_rule", SECTION_LAYER_RULES);
}

void CConverter::registerRuleHandlers() {
    m_config->registerHandler(&::handleWindowrule, "windowrule", {false});
    m_config->registerHandler(&::handleLayerrule, "layerrule", {false});

    // rules also have a block form: `windowrule { name = x, match:class = y, float = true }`,
    // a special category keyed by name whose keys are the same effect and match vocabulary
    const auto CATEGORY = [this](const char* category, std::span<const std::string_view> effects) {
        m_config->addSpecialCategory(category, {.key = "name"});
        m_config->addSpecialConfigValue(category, "enable", Hyprlang::INT{1});

        for (const auto& prop : RULE_MATCH_PROPS) {
            const auto KEY = std::format("match:{}", prop);
            m_config->addSpecialConfigValue(category, KEY.c_str(), Hyprlang::STRING{""});
        }

        for (const auto& effect : effects) {
            const std::string KEY{effect};
            m_config->addSpecialConfigValue(category, KEY.c_str(), Hyprlang::STRING{""});
        }
    };

    CATEGORY("windowrule", WINDOW_RULE_EFFECTS);
    CATEGORY("layerrule", LAYER_RULE_EFFECTS);
}

void CConverter::emitRuleBlocks() {
    const auto EMIT = [this](const char* category, std::span<const std::string_view> effects, std::string_view fn, eSection section) {
        for (const auto& NAME : m_config->listKeysForSpecialCategory(category)) {
            auto rule  = CLuaValue::table();
            auto match = CLuaValue::table();

            rule->set("name", CLuaValue::string(NAME));

            const auto ENABLED = m_config->getSpecialConfigValuePtr(category, "enable", NAME.c_str());
            if (ENABLED && ENABLED->m_bSetByUser)
                rule->set("enabled", CLuaValue::boolean(std::any_cast<Hyprlang::INT>(ENABLED->getValue()) != 0));

            for (const auto& prop : RULE_MATCH_PROPS) {
                const auto KEY = std::format("match:{}", prop);
                const auto PTR = m_config->getSpecialConfigValuePtr(category, KEY.c_str(), NAME.c_str());

                if (!PTR || !PTR->m_bSetByUser)
                    continue;

                match->set(prop, ruleValue(std::any_cast<Hyprlang::STRING>(PTR->getValue())));
            }

            for (const auto& effect : effects) {
                const std::string KEY{effect};
                const auto        PTR = m_config->getSpecialConfigValuePtr(category, KEY.c_str(), NAME.c_str());

                if (!PTR || !PTR->m_bSetByUser)
                    continue;

                rule->set(KEY, ruleValue(std::any_cast<Hyprlang::STRING>(PTR->getValue())));
            }

            if (!match->empty())
                rule->set("match", match);

            m_document.addStatement(section, std::format("{}({})", fn, rule->render()));
        }
    };

    EMIT("windowrule", WINDOW_RULE_EFFECTS, "hl.window_rule", SECTION_WINDOW_RULES);
    EMIT("layerrule", LAYER_RULE_EFFECTS, "hl.layer_rule", SECTION_LAYER_RULES);
}
