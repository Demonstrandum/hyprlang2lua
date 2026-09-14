// Log::CLogger definitions for a program that is not a compositor.
//
// The vendored parsers log through Log::logger. Hyprland's Logger.cpp cannot be reused
// here: it includes the event bus and the config value machinery, which pulls in the
// whole compositor. The header is reused as-is, and only the four member functions it
// declares are defined here, writing to stderr.

#include <print>

#include "debug/log/Logger.hpp"

using namespace Log;

CLogger::CLogger() : m_isTrace(false) {
    m_logger.setLogLevel(Hyprutils::CLI::LOG_DEBUG);
}

void CLogger::log(Hyprutils::CLI::eLogLevel level, const std::string_view& str) {
    if (!m_logsEnabled)
        return;

    if (level == Hyprutils::CLI::LOG_TRACE && !m_isTrace)
        return;

    // parse diagnostics are notes about the input file, so they go to stderr and leave
    // stdout carrying only the converted Lua
    std::println(stderr, "hyprlang2lua: {}", str);
}

void CLogger::initIS(const std::string_view& IS) {
    (void)IS;
}

void CLogger::initCallbacks() {
    ;
}

const std::string& CLogger::rolling() {
    static const std::string EMPTY;
    return EMPTY;
}

Hyprutils::CLI::CLogger& CLogger::hu() {
    return m_logger;
}

void CLogger::recheckCfg() {
    ;
}
