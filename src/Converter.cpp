#include "Converter.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <ranges>

#include <hyprutils/string/VarList2.hpp>

#include <numbers>

#include "macros.hpp"
#include "config/values/ConfigValues.hpp"
#include "config/shared/complex/ComplexDataTypes.hpp"
#include "config/shared/parserUtils/ParserUtils.hpp"

#include "SpecialValues.gen.hpp"

using namespace H2L;
using namespace Hyprutils::String;

static CConverter* g_converter = nullptr;

CConverter*        CConverter::active() {
    return g_converter;
}

// ---------------------------------------------------------------- custom hyprlang types
//
// hyprlang stores gaps, gradients and font weights as opaque custom types. The set/destroy
// pairs below are the ones from the legacy config manager
// (third_party/hyprland-legacy/src/config/legacy/ConfigManager.cpp), kept because they are
// what turns the config text into the structured value this converter then writes out.

static Hyprlang::CParseResult configHandleGradientSet(const char* VALUE, void** data) {
    const std::string V = VALUE;

    if (!*data)
        *data = new Config::CGradientValueData();

    const auto             DATA = static_cast<Config::CGradientValueData*>(*data);
    CVarList2              varlist(V, 0, ' ');
    Hyprlang::CParseResult result;
    std::string            parseError;

    DATA->m_colors.clear();

    for (const auto& var : varlist) {
        if (var.find("deg") != std::string::npos) {
            try {
                DATA->m_angle = static_cast<float>(std::stoi(std::string(var.substr(0, var.find("deg")))) * (std::numbers::pi / 180.0));
            } catch (...) { parseError = std::format("error parsing gradient angle in {}", V); }

            break;
        }

        if (DATA->m_colors.size() >= 10) {
            parseError = std::format("error parsing gradient {}: max colors is 10", V);
            break;
        }

        const auto COL = Config::ParserUtils::parseColor(var);
        if (!COL) {
            parseError = std::format("error parsing gradient {}: {} is not a color", V, var);
            continue;
        }

        DATA->m_colors.emplace_back(COL.value());
    }

    if (DATA->m_colors.empty()) {
        if (parseError.empty())
            parseError = std::format("error parsing gradient {}: no colors", V);
        DATA->m_colors.emplace_back(0);
    }

    DATA->updateColorsOk();

    if (!parseError.empty())
        result.setError(parseError.c_str());

    return result;
}

static void configHandleGradientDestroy(void** data) {
    if (*data)
        delete static_cast<Config::CGradientValueData*>(*data);
}

static Hyprlang::CParseResult configHandleGapSet(const char* VALUE, void** data) {
    std::string V = VALUE;

    if (!*data)
        *data = new Config::CCssGapData();

    const auto             DATA = static_cast<Config::CCssGapData*>(*data);
    CVarList2              varlist((std::string(V)));
    Hyprlang::CParseResult result;

    try {
        DATA->parseGapData(varlist);
    } catch (...) { result.setError("Error parsing gaps"); }

    return result;
}

static void configHandleGapDestroy(void** data) {
    if (*data)
        delete static_cast<Config::CCssGapData*>(*data);
}

static Hyprlang::CParseResult configHandleFontWeightSet(const char* VALUE, void** data) {
    if (!*data)
        *data = new Config::CFontWeightConfigValueData();

    const auto             DATA = static_cast<Config::CFontWeightConfigValueData*>(*data);
    Hyprlang::CParseResult result;

    try {
        DATA->parseWeight(VALUE);
    } catch (...) {
        const auto ERR = std::format("{} is not a valid font weight", VALUE);
        result.setError(ERR.c_str());
    }

    return result;
}

static void configHandleFontWeightDestroy(void** data) {
    if (*data)
        delete static_cast<Config::CFontWeightConfigValueData*>(*data);
}

// ---------------------------------------------------------------- converter

CConverter::CConverter() {
    g_converter = this;
}

CConverter::~CConverter() {
    if (g_converter == this)
        g_converter = nullptr;
}

CDocument& CConverter::document() {
    return m_document;
}

const std::vector<SConvertError>& CConverter::errors() const {
    return m_errors;
}

void CConverter::error(const std::string& message) {
    // hyprlang reports the absolute path it resolved, which would put the machine that ran
    // the conversion into the converted config; the path the user typed is what belongs there
    auto       text     = message;
    const auto ABSOLUTE = std::filesystem::absolute(m_path).string();

    for (auto at = text.find(ABSOLUTE); at != std::string::npos; at = text.find(ABSOLUTE, at + m_path.size()))
        text.replace(at, ABSOLUTE.size(), m_path);

    m_errors.emplace_back(SConvertError{.file = m_path, .line = 0, .message = text});
    // the same note goes into the output, so a converted config carries its own caveats
    m_document.addWarning(text);
}

void CConverter::registerOptions() {
    // the same walk the legacy config manager did, minus the compositor: every option
    // Hyprland declares gets registered with hyprlang under its declared type, so that
    // hyprlang parses values and records which ones the file actually set
    for (const auto& v : Config::Values::CONFIG_VALUES) {
        if (!v)
            continue;

        const char* NAME = v->name();

        if (const auto INT_VALUE = dynamic_cast<Config::Values::CIntValue*>(v.get())) {
            m_config->addConfigValue(NAME, Hyprlang::INT{INT_VALUE->defaultVal()});
            continue;
        }

        if (const auto FLOAT_VALUE = dynamic_cast<Config::Values::CFloatValue*>(v.get())) {
            m_config->addConfigValue(NAME, Hyprlang::FLOAT{FLOAT_VALUE->defaultVal()});
            continue;
        }

        if (const auto BOOL_VALUE = dynamic_cast<Config::Values::CBoolValue*>(v.get())) {
            m_config->addConfigValue(NAME, Hyprlang::INT{BOOL_VALUE->defaultVal() ? 1 : 0});
            continue;
        }

        if (const auto STRING_VALUE = dynamic_cast<Config::Values::CStringValue*>(v.get())) {
            m_config->addConfigValue(NAME, Hyprlang::STRING{STRING_VALUE->defaultVal().c_str()});
            continue;
        }

        if (const auto COLOR_VALUE = dynamic_cast<Config::Values::CColorValue*>(v.get())) {
            m_config->addConfigValue(NAME, Hyprlang::INT{COLOR_VALUE->defaultVal()});
            continue;
        }

        if (const auto VEC2_VALUE = dynamic_cast<Config::Values::CVec2Value*>(v.get())) {
            m_config->addConfigValue(NAME, Hyprlang::VEC2{VEC2_VALUE->defaultVal().x, VEC2_VALUE->defaultVal().y});
            continue;
        }

        if (const auto GAP_VALUE = dynamic_cast<Config::Values::CCssGapValue*>(v.get())) {
            const auto DEFAULT = std::to_string(GAP_VALUE->defaultVal().m_top);
            m_config->addConfigValue(NAME, Hyprlang::CConfigCustomValueType{configHandleGapSet, configHandleGapDestroy, DEFAULT.c_str()});
            continue;
        }

        if (const auto WEIGHT_VALUE = dynamic_cast<Config::Values::CFontWeightValue*>(v.get())) {
            const auto DEFAULT = std::format("{}", WEIGHT_VALUE->defaultVal().m_value);
            m_config->addConfigValue(NAME, Hyprlang::CConfigCustomValueType{configHandleFontWeightSet, configHandleFontWeightDestroy, DEFAULT.c_str()});
            continue;
        }

        if (const auto GRADIENT_VALUE = dynamic_cast<Config::Values::CGradientValue*>(v.get())) {
            const auto& COLORS  = GRADIENT_VALUE->defaultVal().m_colors;
            const auto  DEFAULT = std::format("0x{:x}", COLORS.empty() ? uint32_t{0} : COLORS.front().getAsHex());
            m_config->addConfigValue(NAME, Hyprlang::CConfigCustomValueType{configHandleGradientSet, configHandleGradientDestroy, DEFAULT.c_str()});
            continue;
        }

        error(std::format("option {} has a type this converter does not know", NAME));
    }

    m_config->addConfigValue("autogenerated", Hyprlang::INT{0});
}

// ---------------------------------------------------------------- option emission
//
// The descriptor that told hyprlang how to parse an option also says what the Lua side
// accepts for it (src/config/lua/types/LuaConfig*.cpp), so one switch over the descriptor
// types covers every option there is.

// Hyprland keeps colors as ARGB; the config text, in both languages, writes RGBA.
static std::string colorLiteral(uint32_t argb) {
    const auto A = (argb >> 24) & 0xFF;
    const auto R = (argb >> 16) & 0xFF;
    const auto G = (argb >> 8) & 0xFF;
    const auto B = argb & 0xFF;
    return std::format("rgba({:02x}{:02x}{:02x}{:02x})", R, G, B, A);
}

// m_angle is radians in a float, so the degrees it came from need rounding back out
static double degreesFromRadians(float radians) {
    return std::round(static_cast<double>(radians) * 180.0 / std::numbers::pi * 1000.0) / 1000.0;
}

static PLuaValue luaFromGradient(const Config::CGradientValueData& grad) {
    if (grad.m_colors.size() == 1 && grad.m_angle == 0.F)
        return CLuaValue::string(colorLiteral(grad.m_colors.front().getAsHex()));

    auto colors = CLuaValue::array();
    for (const auto& c : grad.m_colors)
        colors->push(CLuaValue::string(colorLiteral(c.getAsHex())));

    auto out = CLuaValue::table();
    out->set("colors", colors);

    if (grad.m_angle != 0.F)
        out->set("angle", CLuaValue::number(degreesFromRadians(grad.m_angle)));

    return out;
}

static PLuaValue luaFromGap(const Config::CCssGapData& gap) {
    if (gap.m_top == gap.m_right && gap.m_right == gap.m_bottom && gap.m_bottom == gap.m_left)
        return CLuaValue::integer(gap.m_top);

    auto out = CLuaValue::table();
    out->set("top", CLuaValue::integer(gap.m_top));
    out->set("right", CLuaValue::integer(gap.m_right));
    out->set("bottom", CLuaValue::integer(gap.m_bottom));
    out->set("left", CLuaValue::integer(gap.m_left));
    return out;
}

PLuaValue CConverter::luaForOption(const Config::Values::IValue* descriptor, Hyprlang::CConfigValue* value) {
    if (dynamic_cast<const Config::Values::CBoolValue*>(descriptor))
        return CLuaValue::boolean(std::any_cast<Hyprlang::INT>(value->getValue()) != 0);

    if (dynamic_cast<const Config::Values::CIntValue*>(descriptor))
        return CLuaValue::integer(std::any_cast<Hyprlang::INT>(value->getValue()));

    if (dynamic_cast<const Config::Values::CFloatValue*>(descriptor))
        return CLuaValue::number(static_cast<double>(std::any_cast<Hyprlang::FLOAT>(value->getValue())));

    if (dynamic_cast<const Config::Values::CStringValue*>(descriptor))
        return CLuaValue::string(std::any_cast<Hyprlang::STRING>(value->getValue()));

    if (dynamic_cast<const Config::Values::CColorValue*>(descriptor))
        return CLuaValue::string(colorLiteral(static_cast<uint32_t>(std::any_cast<Hyprlang::INT>(value->getValue()))));

    if (dynamic_cast<const Config::Values::CVec2Value*>(descriptor)) {
        const auto VEC = std::any_cast<Hyprlang::VEC2>(value->getValue());
        auto       out = CLuaValue::array();
        out->push(CLuaValue::number(VEC.x));
        out->push(CLuaValue::number(VEC.y));
        return out;
    }

    // a custom type hands back the raw data pointer it stored
    const auto CUSTOM_DATA = [value]() -> void* {
        const auto ANY = value->getValue();
        return ANY.type() == typeid(void*) ? std::any_cast<void*>(ANY) : nullptr;
    };

    if (dynamic_cast<const Config::Values::CGradientValue*>(descriptor)) {
        const auto DATA = static_cast<Config::CGradientValueData*>(CUSTOM_DATA());
        return DATA ? luaFromGradient(*DATA) : nullptr;
    }

    if (dynamic_cast<const Config::Values::CCssGapValue*>(descriptor)) {
        const auto DATA = static_cast<Config::CCssGapData*>(CUSTOM_DATA());
        return DATA ? luaFromGap(*DATA) : nullptr;
    }

    if (dynamic_cast<const Config::Values::CFontWeightValue*>(descriptor)) {
        const auto DATA = static_cast<Config::CFontWeightConfigValueData*>(CUSTOM_DATA());
        return DATA ? CLuaValue::integer(DATA->m_value) : nullptr;
    }

    return nullptr;
}

void CConverter::emitOptions() {
    for (const auto& v : Config::Values::CONFIG_VALUES) {
        if (!v)
            continue;

        const auto PTR = m_config->getConfigValuePtr(v->name());

        if (!PTR || !PTR->m_bSetByUser)
            continue;

        const auto LUA = luaForOption(v.get(), PTR);

        if (!LUA) {
            error(std::format("option {} was set but has no Lua form", v->name()));
            continue;
        }

        m_document.config().setPath(v->name(), LUA);
    }
}

void CConverter::registerDeviceCategory() {
    // hyprlang's special categories: `device { name = ..., ... }` blocks, keyed by name.
    // The key set is the one the legacy config manager registered, read back below as
    // hl.device{} calls.
    m_config->addSpecialCategory("device", {"name"});

    for (const auto& [name, type] : DEVICE_VALUES) {
        const std::string NAME{name};

        switch (type) {
            case SPECIAL_VALUE_INT: m_config->addSpecialConfigValue("device", NAME.c_str(), Hyprlang::INT{0}); break;
            case SPECIAL_VALUE_FLOAT: m_config->addSpecialConfigValue("device", NAME.c_str(), Hyprlang::FLOAT{0.F}); break;
            case SPECIAL_VALUE_STRING: m_config->addSpecialConfigValue("device", NAME.c_str(), Hyprlang::STRING{STRVAL_EMPTY}); break;
        }
    }
}

void CConverter::registerHandlers() {
    registerKeywordHandlers();
    registerRuleHandlers();
    registerBindHandlers();
    registerMonitorHandlers();
}

void CConverter::addBind(const std::string& statement) {
    if (m_currentSubmap.empty()) {
        m_document.addStatement(SECTION_BINDS, statement);
        return;
    }

    m_submapBinds.back().second.emplace_back(statement);
}

void CConverter::setSubmap(const std::string& name, const std::string& reset) {
    m_currentSubmap = name;

    if (name.empty())
        return;

    if (!reset.empty())
        m_document.addWarning(std::format("submap = {}, {}: the reset mode is not carried over", name, reset));

    m_submapBinds.emplace_back(name, std::vector<std::string>{});
}

void CConverter::emitSubmaps() {
    for (const auto& [name, binds] : m_submapBinds) {
        if (binds.empty())
            continue;

        std::string lua = std::format("hl.define_submap({}, function()\n", quoteLuaString(name));
        for (const auto& b : binds)
            lua += std::format("    {}\n", b);
        lua += "end)";

        m_document.addStatement(SECTION_BINDS, lua);
    }
}

void CConverter::addStartupExec(const std::string& command) {
    m_startupExecs.emplace_back(command);
}

void CConverter::addShutdownExec(const std::string& command) {
    m_shutdownExecs.emplace_back(command);
}

void CConverter::emitExecs() {
    const auto SUBSCRIPTION = [this](const std::string& event, const std::vector<std::string>& commands) {
        if (commands.empty())
            return;

        std::string lua = std::format("hl.on({}, function()\n", quoteLuaString(event));
        for (const auto& c : commands)
            lua += std::format("    hl.exec_cmd({})\n", quoteLuaString(c));
        lua += "end)";

        m_document.addStatement(SECTION_AUTOSTART, lua);
    };

    SUBSCRIPTION("hyprland.start", m_startupExecs);
    SUBSCRIPTION("hyprland.shutdown", m_shutdownExecs);
}

void CConverter::emitDevices() {
    // `device { name = ..., ... }` is a hyprlang special category: one instance per key,
    // read back here as the hl.device{} call that says the same thing
    for (const auto& DEVICE : m_config->listKeysForSpecialCategory("device")) {
        auto              device = CLuaValue::table();
        device->set("name", CLuaValue::string(DEVICE));

        for (const auto& [name, type] : DEVICE_VALUES) {
            const std::string FIELD{name};
            const auto        PTR = m_config->getSpecialConfigValuePtr("device", FIELD.c_str(), DEVICE.c_str());

            if (!PTR || !PTR->m_bSetByUser)
                continue;

            switch (type) {
                case SPECIAL_VALUE_INT: device->set(FIELD, CLuaValue::integer(std::any_cast<Hyprlang::INT>(PTR->getValue()))); break;
                case SPECIAL_VALUE_FLOAT: device->set(FIELD, CLuaValue::number(static_cast<double>(std::any_cast<Hyprlang::FLOAT>(PTR->getValue())))); break;
                case SPECIAL_VALUE_STRING: device->set(FIELD, CLuaValue::string(std::any_cast<Hyprlang::STRING>(PTR->getValue()))); break;
            }
        }

        m_document.addStatement(SECTION_DEVICES, std::format("hl.device({})", device->render()));
    }
}

bool CConverter::convert(const std::string& path) {
    m_path   = path;
    m_config = std::make_unique<Hyprlang::CConfig>(path.c_str(), Hyprlang::SConfigOptions{.verifyOnly = false, .throwAllErrors = true, .allowMissingConfig = false});

    registerOptions();
    registerDeviceCategory();
    registerHandlers();

    m_config->commence();

    const auto RESULT = m_config->parse();

    if (RESULT.error)
        error(RESULT.getError());

    emitOptions();
    emitDevices();
    emitExecs();
    emitRuleBlocks();
    emitMonitorBlocks();
    emitSubmaps();

    return m_errors.empty();
}
