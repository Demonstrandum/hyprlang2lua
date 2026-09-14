#include "Lua.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <ranges>

#include <hyprutils/memory/SharedPtr.hpp>

using namespace H2L;
using namespace Hyprutils::Memory;

static constexpr std::string_view LUA_KEYWORDS[] = {
    "and", "break",  "do",     "else", "elseif", "end",   "false", "for",  "function", "goto",  "if",
    "in",  "local",  "nil",    "not",  "or",     "repeat", "return", "then", "true",   "until", "while",
};

static constexpr size_t           INLINE_WIDTH = 96;
static constexpr std::string_view INDENT       = "    ";

static std::string                pad(size_t level) {
    std::string out;
    out.reserve(level * INDENT.size());
    for (size_t i = 0; i < level; ++i)
        out += INDENT;
    return out;
}

bool H2L::isLuaIdentifier(std::string_view s) {
    if (s.empty())
        return false;

    if (std::isalpha(static_cast<unsigned char>(s.front())) == 0 && s.front() != '_')
        return false;

    if (!std::ranges::all_of(s, [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_'; }))
        return false;

    return !std::ranges::contains(LUA_KEYWORDS, s);
}

std::string H2L::quoteLuaString(std::string_view s) {
    std::string out = "\"";
    out.reserve(s.size() + 2);

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

    return std::format("{}", d);
}

PLuaValue CLuaValue::raw(std::string_view src) {
    auto v      = makeShared<CLuaValue>();
    v->m_type   = LUA_VALUE_RAW;
    v->m_string = src;
    return v;
}

PLuaValue CLuaValue::string(std::string_view s) {
    auto v      = makeShared<CLuaValue>();
    v->m_type   = LUA_VALUE_STRING;
    v->m_string = s;
    return v;
}

PLuaValue CLuaValue::number(double d) {
    auto v      = makeShared<CLuaValue>();
    v->m_type   = LUA_VALUE_NUMBER;
    v->m_number = d;
    return v;
}

PLuaValue CLuaValue::integer(int64_t i) {
    auto v       = makeShared<CLuaValue>();
    v->m_type    = LUA_VALUE_NUMBER;
    v->m_number  = static_cast<double>(i);
    v->m_integer = true;
    return v;
}

PLuaValue CLuaValue::boolean(bool b) {
    auto v    = makeShared<CLuaValue>();
    v->m_type = LUA_VALUE_BOOL;
    v->m_bool = b;
    return v;
}

PLuaValue CLuaValue::array() {
    auto v    = makeShared<CLuaValue>();
    v->m_type = LUA_VALUE_ARRAY;
    return v;
}

PLuaValue CLuaValue::table() {
    auto v    = makeShared<CLuaValue>();
    v->m_type = LUA_VALUE_TABLE;
    return v;
}

eLuaValueType CLuaValue::type() const {
    return m_type;
}

CLuaValue& CLuaValue::push(PLuaValue value) {
    m_array.emplace_back(std::move(value));
    return *this;
}

CLuaValue& CLuaValue::set(std::string_view key, PLuaValue value) {
    const auto IT = std::ranges::find(m_table, key, &std::pair<std::string, PLuaValue>::first);

    if (IT != m_table.end()) {
        IT->second = std::move(value);
        return *this;
    }

    m_table.emplace_back(std::string{key}, std::move(value));
    return *this;
}

PLuaValue CLuaValue::get(std::string_view key) const {
    const auto IT = std::ranges::find(m_table, key, &std::pair<std::string, PLuaValue>::first);
    return IT == m_table.end() ? PLuaValue{} : IT->second;
}

bool CLuaValue::has(std::string_view key) const {
    return std::ranges::contains(m_table, key, &std::pair<std::string, PLuaValue>::first);
}

bool CLuaValue::empty() const {
    return m_table.empty() && m_array.empty();
}

CLuaValue& CLuaValue::setPath(std::string_view path, PLuaValue value) {
    auto parts = path | std::views::split(':') | std::views::transform([](auto&& r) { return std::string_view{r}; }) |
        std::views::transform([](std::string_view s) { return s | std::views::split('.') | std::views::transform([](auto&& r) { return std::string_view{r}; }); }) |
        std::views::join;

    std::vector<std::string_view> keys;
    for (const auto& p : parts) {
        if (!p.empty())
            keys.emplace_back(p);
    }

    if (keys.empty())
        return *this;

    CLuaValue* node = this;
    for (const auto& key : keys | std::views::take(keys.size() - 1)) {
        auto child = node->get(key);
        if (!child || child->m_type != LUA_VALUE_TABLE) {
            child = CLuaValue::table();
            node->set(key, child);
        }
        node = child.get();
    }

    node->set(keys.back(), std::move(value));
    return *this;
}

std::string CLuaValue::renderCompound(size_t indentLevel) const {
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
        oneLine += std::format("{}{}", parts[i], i + 1 < parts.size() ? ", " : " ");
    oneLine += "}";

    const auto PADDING = pad(indentLevel);

    if (!oneLine.contains('\n') && oneLine.length() + PADDING.length() <= INLINE_WIDTH)
        return oneLine;

    std::string out = "{\n";
    for (const auto& p : parts)
        out += std::format("{}{}{},\n", PADDING, INDENT, p);
    out += PADDING + "}";
    return out;
}

std::string CLuaValue::render(size_t indentLevel) const {
    switch (m_type) {
        case LUA_VALUE_RAW: return m_string;
        case LUA_VALUE_STRING: return quoteLuaString(m_string);
        case LUA_VALUE_NUMBER: return formatLuaNumber(m_number, m_integer);
        case LUA_VALUE_BOOL: return m_bool ? "true" : "false";
        case LUA_VALUE_ARRAY:
        case LUA_VALUE_TABLE: return renderCompound(indentLevel);
    }

    return "nil";
}
