#include "Lua.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <set>

using namespace H2L;

static const std::set<std::string> LUA_KEYWORDS = {
    "and",  "break", "do",     "else",   "elseif", "end",  "false", "for",  "function", "goto",  "if",
    "in",   "local", "nil",    "not",    "or",     "repeat", "return", "then", "true",   "until", "while",
};

// a table renders on one line while it stays under this width
static constexpr size_t INLINE_WIDTH = 96;
static const std::string INDENT      = "    ";

bool H2L::isLuaIdentifier(const std::string& s) {
    if (s.empty())
        return false;
    if (std::isalpha(static_cast<unsigned char>(s[0])) == 0 && s[0] != '_')
        return false;
    for (const auto& c : s) {
        if (std::isalnum(static_cast<unsigned char>(c)) == 0 && c != '_')
            return false;
    }
    return !LUA_KEYWORDS.contains(s);
}

std::string H2L::quoteLuaString(const std::string& s) {
    std::string out = "\"";
    for (const auto& c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out + "\"";
}

std::string H2L::formatLuaNumber(double d, bool isInteger) {
    if (isInteger || d == std::floor(d))
        return std::format("{}", static_cast<int64_t>(d));

    auto out = std::format("{}", d);
    return out;
}

PLuaValue CLuaValue::raw(const std::string& src) {
    auto v      = std::make_shared<CLuaValue>();
    v->m_type   = LUA_VALUE_RAW;
    v->m_string = src;
    return v;
}

PLuaValue CLuaValue::string(const std::string& s) {
    auto v      = std::make_shared<CLuaValue>();
    v->m_type   = LUA_VALUE_STRING;
    v->m_string = s;
    return v;
}

PLuaValue CLuaValue::number(double d) {
    auto v       = std::make_shared<CLuaValue>();
    v->m_type    = LUA_VALUE_NUMBER;
    v->m_number  = d;
    v->m_integer = false;
    return v;
}

PLuaValue CLuaValue::integer(int64_t i) {
    auto v       = std::make_shared<CLuaValue>();
    v->m_type    = LUA_VALUE_NUMBER;
    v->m_number  = static_cast<double>(i);
    v->m_integer = true;
    return v;
}

PLuaValue CLuaValue::boolean(bool b) {
    auto v    = std::make_shared<CLuaValue>();
    v->m_type = LUA_VALUE_BOOL;
    v->m_bool = b;
    return v;
}

PLuaValue CLuaValue::array() {
    auto v    = std::make_shared<CLuaValue>();
    v->m_type = LUA_VALUE_ARRAY;
    return v;
}

PLuaValue CLuaValue::table() {
    auto v    = std::make_shared<CLuaValue>();
    v->m_type = LUA_VALUE_TABLE;
    return v;
}

eLuaValueType CLuaValue::type() const {
    return m_type;
}

CLuaValue& CLuaValue::push(PLuaValue v) {
    m_array.emplace_back(std::move(v));
    return *this;
}

CLuaValue& CLuaValue::set(const std::string& key, PLuaValue v) {
    for (auto& [k, val] : m_table) {
        if (k != key)
            continue;
        val = std::move(v);
        return *this;
    }

    m_table.emplace_back(key, std::move(v));
    return *this;
}

bool CLuaValue::has(const std::string& key) const {
    return std::ranges::any_of(m_table, [&key](const auto& e) { return e.first == key; });
}

PLuaValue CLuaValue::get(const std::string& key) const {
    for (const auto& [k, v] : m_table) {
        if (k == key)
            return v;
    }
    return nullptr;
}

bool CLuaValue::empty() const {
    return m_table.empty() && m_array.empty();
}

CLuaValue& CLuaValue::setPath(const std::string& dottedPath, PLuaValue v) {
    // hyprlang addresses options as "cat:sub:name" and, for the few grouped colors,
    // "general:col.active_border". Both separators mean the same nesting in Lua.
    std::vector<std::string> parts;
    std::string              current;
    for (const auto& c : dottedPath) {
        if (c == ':' || c == '.') {
            if (!current.empty())
                parts.emplace_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    if (!current.empty())
        parts.emplace_back(current);

    if (parts.empty())
        return *this;

    CLuaValue* node = this;
    for (size_t i = 0; i + 1 < parts.size(); ++i) {
        auto child = node->get(parts[i]);
        if (!child || child->m_type != LUA_VALUE_TABLE) {
            child = CLuaValue::table();
            node->set(parts[i], child);
        }
        node = child.get();
    }

    node->set(parts.back(), std::move(v));
    return *this;
}

std::string CLuaValue::renderInner(size_t indentLevel) const {
    std::string pad;
    for (size_t i = 0; i < indentLevel; ++i)
        pad += INDENT;
    const auto padIn = pad + INDENT;

    std::vector<std::string> parts;

    if (m_type == LUA_VALUE_ARRAY) {
        parts.reserve(m_array.size());
        for (const auto& e : m_array)
            parts.emplace_back(e->render(indentLevel + 1));
    } else {
        parts.reserve(m_table.size());
        for (const auto& [k, v] : m_table) {
            const auto KEY = isLuaIdentifier(k) ? k : std::format("[{}]", quoteLuaString(k));
            parts.emplace_back(std::format("{} = {}", KEY, v->render(indentLevel + 1)));
        }
    }

    if (parts.empty())
        return "{}";

    std::string oneLine = "{ ";
    for (size_t i = 0; i < parts.size(); ++i)
        oneLine += parts[i] + (i + 1 < parts.size() ? ", " : " ");
    oneLine += "}";

    if (oneLine.find('\n') == std::string::npos && oneLine.length() + pad.length() <= INLINE_WIDTH)
        return oneLine;

    std::string out = "{\n";
    for (const auto& p : parts)
        out += padIn + p + ",\n";
    out += pad + "}";
    return out;
}

std::string CLuaValue::render(size_t indentLevel) const {
    switch (m_type) {
        case LUA_VALUE_RAW: return m_string;
        case LUA_VALUE_STRING: return quoteLuaString(m_string);
        case LUA_VALUE_NUMBER: return formatLuaNumber(m_number, m_integer);
        case LUA_VALUE_BOOL: return m_bool ? "true" : "false";
        case LUA_VALUE_ARRAY:
        case LUA_VALUE_TABLE: return renderInner(indentLevel);
    }

    return "nil";
}
