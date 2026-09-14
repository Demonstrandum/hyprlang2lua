#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Lua.hpp"

namespace H2L {

    // The converted config, kept as ordered sections rather than a string, so that
    // conversion order (which follows the input file) and output order (which follows the
    // shape of example/hyprland.lua) stay independent.
    enum eSection : uint8_t {
        SECTION_HEADER = 0,
        SECTION_VARIABLES, // $foo = bar -> local foo = "bar"
        SECTION_MONITORS,
        SECTION_AUTOSTART, // exec / exec-once -> hl.on("hyprland.start", ...)
        SECTION_ENV,
        SECTION_PERMISSIONS,
        SECTION_CONFIG, // the single hl.config{} tree
        SECTION_CURVES,
        SECTION_ANIMATIONS,
        SECTION_WORKSPACE_RULES,
        SECTION_WINDOW_RULES,
        SECTION_LAYER_RULES,
        SECTION_GESTURES,
        SECTION_DEVICES,
        SECTION_PLUGINS,
        SECTION_BINDS,
        SECTION_LAST,
    };

    class CDocument {
      public:
        // a free-standing line of Lua, e.g. a hl.* call
        void        addStatement(eSection section, std::string_view lua);
        void        addComment(eSection section, std::string_view text);
        // a note about something the converter could not translate faithfully
        void        addWarning(std::string_view text);

        // the accumulating hl.config{} tree
        CLuaValue&  config();

        bool        hasWarnings() const;
        std::string render() const;

      private:
        struct SEntry {
            std::string text;
            bool        comment = false;
        };

        std::vector<SEntry>      m_sections[SECTION_LAST];
        std::vector<std::string> m_warnings;
        PLuaValue                       m_config = CLuaValue::table();
    };
}
