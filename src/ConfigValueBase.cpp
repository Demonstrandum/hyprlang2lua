// CConfigValueBase definitions for a program with no running config manager.
//
// Hyprland's value descriptors derive from CConfigValueBase so that compositor code can
// read the live value of an option through CConfigValue<T>. Hyprland's ConfigValue.cpp
// implements that by asking Config::mgr() for the value pointer, which only exists inside
// a running compositor. This converter never reads live values (it reads what hyprlang
// parsed out of the file), so the binding is defined here as registry bookkeeping with no
// lookup behind it.

#include <algorithm>
#include <string>
#include <typeindex>
#include <vector>

#include "config/ConfigValue.hpp"

void local__configValuePopulate(void* const** p, void* const** hlangp, std::type_index* ti, const std::string& val) {
    (void)val;
    *p      = nullptr;
    *hlangp = nullptr;
    *ti     = std::type_index(typeid(void));
}

std::type_index local__configValueTypeIdx(const std::string& val) {
    (void)val;
    return std::type_index(typeid(void));
}

CConfigValueBase::CConfigValueBase() {
    registry().emplace_back(this);
}

CConfigValueBase::~CConfigValueBase() {
    std::erase(registry(), this);
}

void CConfigValueBase::populateFromName() {
    m_p         = nullptr;
    m_hlangp    = nullptr;
    m_typeIndex = typeid(void);
}

void CConfigValueBase::bindInternal(const std::string& val) {
    m_valueName = val;
    registry().push_back(this);
    populateFromName();
}

std::vector<CConfigValueBase*>& CConfigValueBase::registry() {
    static std::vector<CConfigValueBase*> r;
    return r;
}

void CConfigValueBase::flushCaches() {
    ;
}
