#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <hyprutils/memory/SharedPtr.hpp>

namespace H2L {

    class CLuaValue;

    using PLuaValue = Hyprutils::Memory::CSharedPointer<CLuaValue>;

    enum eLuaValueType : uint8_t {
        LUA_VALUE_RAW = 0, // verbatim Lua source, e.g. a call expression
        LUA_VALUE_STRING,
        LUA_VALUE_NUMBER,
        LUA_VALUE_BOOL,
        LUA_VALUE_ARRAY,
        LUA_VALUE_TABLE,
    };

    class CLuaValue {
      public:
        static PLuaValue        raw(std::string_view src);
        static PLuaValue        string(std::string_view s);
        static PLuaValue        number(double d);
        static PLuaValue        integer(int64_t i);
        static PLuaValue        boolean(bool b);
        static PLuaValue        array();
        static PLuaValue        table();

        CLuaValue&       push(PLuaValue value);
        CLuaValue&       set(std::string_view key, PLuaValue value);
        // "general:col.active_border" nests on both ':' and '.'
        CLuaValue&       setPath(std::string_view path, PLuaValue value);

        PLuaValue               get(std::string_view key) const;
        bool             has(std::string_view key) const;
        bool             empty() const;
        eLuaValueType    type() const;

        std::string      render(size_t indentLevel = 0) const;

      private:
        std::string                                    renderCompound(size_t indentLevel) const;

        eLuaValueType                                  m_type = LUA_VALUE_RAW;
        std::string                                    m_string;
        double                                         m_number  = 0;
        bool                                           m_integer = false;
        bool                                           m_bool    = false;
        std::vector<PLuaValue>                                m_array;
        std::vector<std::pair<std::string, PLuaValue>>        m_table;
    };

    bool        isLuaIdentifier(std::string_view s);
    std::string quoteLuaString(std::string_view s);
    std::string formatLuaNumber(double d, bool isInteger);
}
