#include "Document.hpp"

#include <format>

using namespace H2L;

struct SSectionMeta {
    eSection         section;
    std::string_view heading;
};

// headings mirror the ones in Hyprland's example/hyprland.lua, so a converted config
// reads like the shipped one
static constexpr SSectionMeta SECTION_META[] = {
    {SECTION_HEADER, ""},
    {SECTION_VARIABLES, "VARIABLES"},
    {SECTION_MONITORS, "MONITORS"},
    {SECTION_AUTOSTART, "AUTOSTART"},
    {SECTION_ENV, "ENVIRONMENT VARIABLES"},
    {SECTION_PERMISSIONS, "PERMISSIONS"},
    {SECTION_CONFIG, "LOOK AND FEEL"},
    {SECTION_CURVES, "CURVES"},
    {SECTION_ANIMATIONS, "ANIMATIONS"},
    {SECTION_WORKSPACE_RULES, "WORKSPACE RULES"},
    {SECTION_WINDOW_RULES, "WINDOW RULES"},
    {SECTION_LAYER_RULES, "LAYER RULES"},
    {SECTION_GESTURES, "GESTURES"},
    {SECTION_DEVICES, "DEVICES"},
    {SECTION_PLUGINS, "PLUGINS"},
    {SECTION_BINDS, "KEYBINDINGS"},
};

void CDocument::addStatement(eSection section, std::string_view lua) {
    m_sections[section].emplace_back(SEntry{.text = std::string{lua}, .comment = false});
}

void CDocument::addComment(eSection section, std::string_view text) {
    m_sections[section].emplace_back(SEntry{.text = std::string{text}, .comment = true});
}

void CDocument::addWarning(std::string_view text) {
    m_warnings.emplace_back(text);
}

CLuaValue& CDocument::config() {
    return *m_config;
}

bool CDocument::hasWarnings() const {
    return !m_warnings.empty();
}

static std::string banner(std::string_view heading) {
    if (heading.empty())
        return "";

    const auto RULE = std::string(heading.size() + 10, '-');
    return std::format("{}\n---- {} ----\n{}\n", RULE, heading, RULE);
}

std::string CDocument::render() const {
    std::string out;

    for (const auto& [section, heading] : SECTION_META) {
        auto entries = m_sections[section];

        if (section == SECTION_CONFIG && !m_config->empty())
            entries.emplace_back(SEntry{.text = std::format("hl.config({})", m_config->render()), .comment = false});

        if (entries.empty())
            continue;

        if (!out.empty())
            out += "\n";

        out += banner(heading);

        for (const auto& e : entries)
            out += e.comment ? std::format("-- {}\n", e.text) : std::format("{}\n", e.text);
    }

    if (!m_warnings.empty()) {
        out += "\n-- hyprlang2lua could not convert the following faithfully:\n";
        for (const auto& w : m_warnings)
            out += std::format("--   {}\n", w);
    }

    return out;
}
