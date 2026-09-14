#pragma once

// Lua value model and emission. Nothing here knows about hyprlang; the converters build
// these nodes and Emitter renders them.

#include <memory>
#include <string>
#include <vector>

namespace H2L {

    class CLuaValue;
    using PLuaValue = std::shared_ptr<CLuaValue>;

    enum eLuaValueType : uint8_t {
        LUA_VALUE_RAW, // verbatim Lua source, e.g. a call expression
        LUA_VALUE_STRING,
        LUA_VALUE_NUMBER,
        LUA_VALUE_BOOL,
        LUA_VALUE_ARRAY,
        LUA_VALUE_TABLE,
    };

    class CLuaValue {
      public:
        static PLuaValue raw(const std::string& src);
        static PLuaValue string(const std::string& s);
        static PLuaValue number(double d);
        static PLuaValue integer(int64_t i);
        static PLuaValue boolean(bool b);
        static PLuaValue array();
        static PLuaValue table();

        // array building
        CLuaValue&       push(PLuaValue v);
        // table building; insertion order is kept
        CLuaValue&       set(const std::string& key, PLuaValue v);
        bool             has(const std::string& key) const;
        PLuaValue        get(const std::string& key) const;
        bool             empty() const;

        // "general:col.active_border" style paths, split on ':' and '.'
        CLuaValue&       setPath(const std::string& dottedPath, PLuaValue v);

        std::string      render(size_t indentLevel = 0) const;

        eLuaValueType    type() const;

      private:
        eLuaValueType                                m_type = LUA_VALUE_RAW;
        std::string                                  m_string; // raw / string
        double                                       m_number  = 0;
        bool                                         m_integer = false;
        bool                                         m_bool    = false;
        std::vector<PLuaValue>                       m_array;
        std::vector<std::pair<std::string, PLuaValue>> m_table;

        std::string                                  renderInner(size_t indentLevel) const;
    };

    bool        isLuaIdentifier(const std::string& s);
    std::string quoteLuaString(const std::string& s);
    std::string formatLuaNumber(double d, bool isInteger);
}
