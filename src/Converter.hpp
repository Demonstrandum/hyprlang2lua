#pragma once

// Drives hyprlang over a legacy config and collects the Lua that replaces it.
//
// The option half needs no per-option code: the same table Hyprland registers with
// hyprlang (Config::Values::CONFIG_VALUES, vendored) also says what Lua type each option
// takes, so the conversion is "read every option hyprlang marked as set by the user, and
// write it into the hl.config tree with the type its descriptor already declares".
//
// The keyword half does need code, because keywords are positional mini-languages. Those
// converters live in src/handlers and follow the legacy handler bodies in
// reference/legacy/ConfigManager.cpp.

#include <hyprlang.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Document.hpp"

namespace Config::Values {
    class IValue;
}

namespace H2L {

    struct SConvertError {
        std::string file;
        int         line = 0;
        std::string message;
    };

    class CConverter {
      public:
        CConverter();
        ~CConverter();

        // parses path and fills the document. Errors are collected, not thrown.
        bool                              convert(const std::string& path);

        CDocument&                        document();
        const std::vector<SConvertError>& errors() const;

        void                              error(const std::string& message);

        // collected by the exec handlers, written out as one hyprland.start subscription
        void                              addStartupExec(const std::string& command);
        void                              addShutdownExec(const std::string& command);

        // handlers are free functions in src/handlers; they reach the active converter here
        static CConverter*                active();

        // defined in src/handlers, one translation unit per family of keywords
        void registerKeywordHandlers();
        void registerRuleHandlers();

      private:
        void registerOptions();
        void registerDeviceCategory();
        void registerHandlers();

        void emitExecs();

        // descriptor + parsed hyprlang value -> the Lua the config tree wants
        static PLuaValue luaForOption(const Config::Values::IValue* descriptor, Hyprlang::CConfigValue* value);

        // walks every registered option and emits the ones hyprlang saw in the file
        void emitOptions();
        void emitDevices();

        std::unique_ptr<Hyprlang::CConfig> m_config;
        CDocument                          m_document;
        std::vector<SConvertError>         m_errors;
        std::string                        m_path;
        std::vector<std::string>           m_startupExecs;
        std::vector<std::string>           m_shutdownExecs;
    };
}
