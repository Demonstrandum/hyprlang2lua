// bind conversion.
//
// The bind line itself is parsed exactly as CConfigManager::handleBind parses it: the
// letters after "bind" are flags, and the argument count depends on which of them are set.
// The dispatcher and its argument string are then rewritten into the hl.dsp call that
// invokes the same Config::Actions entry point, per DISPATCHERS below.
//
// Flag letters map onto the option names hl.bind reads (locked, release, repeating, ...),
// which is the mapping in third_party/hyprland/src/config/lua/bindings/LuaBindingsToplevel.cpp.

#include <algorithm>
#include <cctype>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <hyprutils/string/String.hpp>
#include <hyprutils/string/VarList.hpp>
#include <hyprutils/string/VarList2.hpp>

#include "../Converter.hpp"

using namespace H2L;
using namespace Hyprutils::String;

namespace {
    struct SBindFlags {
        bool locked            = false;
        bool release           = false;
        bool repeating         = false;
        bool mouse             = false;
        bool nonConsuming      = false;
        bool autoConsuming     = false;
        bool transparent       = false;
        bool ignoreMods        = false;
        bool multiKey          = false;
        bool longPress         = false;
        bool hasDescription    = false;
        bool dontInhibit       = false;
        bool click             = false;
        bool drag              = false;
        bool submapUniversal   = false;
        bool perDevice         = false;
        bool allowInputCapture = false;
    };

    std::optional<std::string> parseFlags(std::string_view letters, SBindFlags& flags) {
        for (const auto& c : letters) {
            switch (c) {
                case 'l': flags.locked = true; break;
                case 'r': flags.release = true; break;
                case 'e': flags.repeating = true; break;
                case 'm': flags.mouse = true; break;
                case 'n': flags.nonConsuming = true; break;
                case 'a': flags.autoConsuming = true; break;
                case 't': flags.transparent = true; break;
                case 'i': flags.ignoreMods = true; break;
                case 's': flags.multiKey = true; break;
                case 'o': flags.longPress = true; break;
                case 'd': flags.hasDescription = true; break;
                case 'p': flags.dontInhibit = true; break;
                case 'c':
                    flags.click   = true;
                    flags.release = true;
                    break;
                case 'g':
                    flags.drag    = true;
                    flags.release = true;
                    break;
                case 'u': flags.submapUniversal = true; break;
                case 'k': flags.perDevice = true; break;
                case 'x': flags.allowInputCapture = true; break;
                default: return std::format("bind: invalid flag {}", c);
            }
        }

        return std::nullopt;
    }

    // "SUPER SHIFT" + "S" -> "SUPER + SHIFT + S", which is what hl.bind's parseKeyString reads
    std::string keyString(std::string_view mods, std::string_view key) {
        std::string out;

        for (const auto& mod : CVarList2(std::string{mods}, 0, ' ')) {
            if (mod.empty())
                continue;
            out += std::format("{}{}", out.empty() ? "" : " + ", mod);
        }

        if (!key.empty())
            out += std::format("{}{}", out.empty() ? "" : " + ", key);

        return out;
    }

    PLuaValue optionsTable(const SBindFlags& flags, std::string_view description, std::string_view devices) {
        auto opts = CLuaValue::table();

        const auto FLAG = [&opts](std::string_view name, bool set) {
            if (set)
                opts->set(name, CLuaValue::boolean(true));
        };

        FLAG("locked", flags.locked);
        FLAG("release", flags.release && !flags.click && !flags.drag);
        FLAG("repeating", flags.repeating);
        FLAG("mouse", flags.mouse);
        FLAG("non_consuming", flags.nonConsuming);
        FLAG("auto_consuming", flags.autoConsuming);
        FLAG("transparent", flags.transparent);
        FLAG("ignore_mods", flags.ignoreMods);
        FLAG("dont_inhibit", flags.dontInhibit);
        FLAG("long_press", flags.longPress);
        FLAG("submap_universal", flags.submapUniversal);
        FLAG("click", flags.click);
        FLAG("drag", flags.drag);
        FLAG("allow_input_capture", flags.allowInputCapture);

        if (!description.empty())
            opts->set("description", CLuaValue::string(std::string{description}));

        if (!devices.empty()) {
            const bool INCLUSIVE = devices.front() != '!';
            const auto LIST      = INCLUSIVE ? devices : devices.substr(1);

            auto       device    = CLuaValue::table();
            auto       names     = CLuaValue::array();

            for (const auto& d : CVarList2(std::string{LIST}, 0, ' ')) {
                if (!d.empty())
                    names->push(CLuaValue::string(std::string{d}));
            }

            device->set("inclusive", CLuaValue::boolean(INCLUSIVE));
            device->set("list", names);
            opts->set("device", device);
        }

        return opts;
    }

    // ------------------------------------------------------------ dispatchers
    //
    // Each entry rewrites a legacy dispatcher plus its argument string into the hl.dsp call
    // that reaches the same Config::Actions function. Dispatchers whose Lua counterpart
    // takes a table are given the field the legacy argument corresponded to.

    using DispatchFn = std::function<std::optional<std::string>(const std::string& args)>;

    std::string q(std::string_view s) {
        return quoteLuaString(std::string{s});
    }

    std::optional<std::string> directionCall(std::string_view fn, const std::string& args, std::string_view field) {
        static constexpr std::pair<char, std::string_view> DIRECTIONS[] = {{'l', "left"}, {'r', "right"}, {'u', "up"}, {'d', "down"}, {'t', "up"}, {'b', "down"}};

        if (args.empty())
            return std::nullopt;

        for (const auto& [letter, name] : DIRECTIONS) {
            if (args.front() != letter)
                continue;
            return std::format("{}({{ {} = {} }})", fn, field, q(name));
        }

        return std::nullopt;
    }

    const std::vector<std::pair<std::string, DispatchFn>>& dispatchers() {
        static const std::vector<std::pair<std::string, DispatchFn>> MAP = {
            {"exec", [](const std::string& a) { return std::format("hl.dsp.exec_cmd({})", q(a)); }},
            {"execr", [](const std::string& a) { return std::format("hl.dsp.exec_raw({})", q(a)); }},
            {"killactive", [](const std::string&) { return std::string{"hl.dsp.window.close()"}; }},
            {"closewindow", [](const std::string& a) { return std::format("hl.dsp.window.close({})", a.empty() ? "" : q(a)); }},
            {"forcekillactive", [](const std::string&) { return std::string{"hl.dsp.window.kill()"}; }},
            {"killwindow", [](const std::string& a) { return std::format("hl.dsp.window.kill({})", a.empty() ? "" : q(a)); }},
            {"exit", [](const std::string&) { return std::string{"hl.dsp.exit()"}; }},
            {"togglefloating", [](const std::string&) { return std::string{R"(hl.dsp.window.float({ action = "toggle" }))"}; }},
            {"setfloating", [](const std::string&) { return std::string{R"(hl.dsp.window.float({ action = "set" }))"}; }},
            {"settiled", [](const std::string&) { return std::string{R"(hl.dsp.window.float({ action = "unset" }))"}; }},
            {"pseudo", [](const std::string&) { return std::string{"hl.dsp.window.pseudo()"}; }},
            {"pin", [](const std::string&) { return std::string{"hl.dsp.window.pin()"}; }},
            {"centerwindow", [](const std::string&) { return std::string{"hl.dsp.window.center()"}; }},
            {"bringactivetotop", [](const std::string&) { return std::string{"hl.dsp.window.bring_to_top()"}; }},
            {"toggleswallow", [](const std::string&) { return std::string{"hl.dsp.window.toggle_swallow()"}; }},
            {"tagwindow", [](const std::string& a) { return std::format("hl.dsp.window.tag({})", q(a)); }},
            {"cyclenext", [](const std::string& a) { return std::format("hl.dsp.window.cycle_next({})", a.empty() ? "" : q(a)); }},
            {"workspace", [](const std::string& a) { return std::format("hl.dsp.focus({{ workspace = {} }})", q(a)); }},
            {"focusworkspaceoncurrentmonitor", [](const std::string& a) { return std::format("hl.dsp.focus({{ workspace = {}, current_monitor = true }})", q(a)); }},
            {"movetoworkspace", [](const std::string& a) { return std::format("hl.dsp.window.move({{ workspace = {} }})", q(a)); }},
            {"movetoworkspacesilent", [](const std::string& a) { return std::format("hl.dsp.window.move({{ workspace = {}, follow = false }})", q(a)); }},
            {"togglespecialworkspace", [](const std::string& a) { return std::format("hl.dsp.workspace.toggle_special({})", a.empty() ? q("special") : q(a)); }},
            {"renameworkspace", [](const std::string& a) { return std::format("hl.dsp.workspace.rename({})", q(a)); }},
            {"swapactiveworkspaces", [](const std::string& a) { return std::format("hl.dsp.workspace.swap_monitors({})", q(a)); }},
            {"movecurrentworkspacetomonitor", [](const std::string& a) { return std::format("hl.dsp.workspace.move({{ monitor = {} }})", q(a)); }},
            {"moveworkspacetomonitor", [](const std::string& a) { return std::format("hl.dsp.workspace.move({{ target = {} }})", q(a)); }},
            {"focusmonitor", [](const std::string& a) { return std::format("hl.dsp.focus({{ monitor = {} }})", q(a)); }},
            {"focuswindow", [](const std::string& a) { return std::format("hl.dsp.focus({{ window = {} }})", q(a)); }},
            {"focusurgentorlast", [](const std::string&) { return std::string{R"(hl.dsp.focus({ urgent_or_last = true }))"}; }},
            {"focuscurrentorlast", [](const std::string&) { return std::string{R"(hl.dsp.focus({ current_or_last = true }))"}; }},
            {"movefocus", [](const std::string& a) { return directionCall("hl.dsp.focus", a, "direction"); }},
            {"movewindow", [](const std::string& a) { return directionCall("hl.dsp.window.move", a, "direction"); }},
            {"movewindoworgroup", [](const std::string& a) { return directionCall("hl.dsp.window.move", a, "direction"); }},
            {"swapwindow", [](const std::string& a) { return directionCall("hl.dsp.window.swap", a, "direction"); }},
            {"moveintogroup", [](const std::string& a) { return directionCall("hl.dsp.window.move", a, "into_group"); }},
            {"moveintoorcreategroup", [](const std::string& a) { return directionCall("hl.dsp.window.move", a, "into_or_create_group"); }},
            {"moveoutofgroup", [](const std::string&) { return std::string{"hl.dsp.window.move({ out_of_group = true })"}; }},
            {"togglegroup", [](const std::string&) { return std::string{"hl.dsp.group.toggle()"}; }},
            {"lockgroups", [](const std::string& a) { return std::format("hl.dsp.group.lock({})", q(a)); }},
            {"lockactivegroup", [](const std::string& a) { return std::format("hl.dsp.group.lock_active({})", q(a)); }},
            {"denywindowfromgroup", [](const std::string& a) { return std::format("hl.dsp.window.deny_from_group({})", q(a)); }},
            {"changegroupactive", [](const std::string& a) { return a == "b" || a == "prev" ? std::string{"hl.dsp.group.prev()"} : std::string{"hl.dsp.group.next()"}; }},
            {"movegroupwindow", [](const std::string& a) { return std::format("hl.dsp.group.move_window({})", a == "b" ? "true" : "false"); }},
            {"submap", [](const std::string& a) { return std::format("hl.dsp.submap({})", q(a)); }},
            {"pass", [](const std::string& a) { return std::format("hl.dsp.pass({})", q(a)); }},
            {"sendshortcut", [](const std::string& a) { return std::format("hl.dsp.send_shortcut({})", q(a)); }},
            {"sendkeystate", [](const std::string& a) { return std::format("hl.dsp.send_key_state({})", q(a)); }},
            {"layoutmsg", [](const std::string& a) { return std::format("hl.dsp.layout({})", q(a)); }},
            {"dpms", [](const std::string& a) { return std::format("hl.dsp.dpms({})", q(a)); }},
            {"event", [](const std::string& a) { return std::format("hl.dsp.event({})", q(a)); }},
            {"global", [](const std::string& a) { return std::format("hl.dsp.global({})", q(a)); }},
            {"setprop", [](const std::string& a) { return std::format("hl.dsp.window.set_prop({})", q(a)); }},
            {"alterzorder", [](const std::string& a) { return std::format("hl.dsp.window.alter_zorder({})", q(a)); }},
            {"forcerendererreload", [](const std::string&) { return std::string{"hl.dsp.force_renderer_reload()"}; }},
            {"forceidle", [](const std::string&) { return std::string{"hl.dsp.force_idle()"}; }},
            {"releaseinputcapture", [](const std::string&) { return std::string{"hl.dsp.release_input_capture()"}; }},
            {"movecursortocorner", [](const std::string& a) { return std::format("hl.dsp.cursor.move_to_corner({})", a.empty() ? "0" : a); }},
            {"movecursor", [](const std::string& a) { return std::format("hl.dsp.cursor.move({})", q(a)); }},
            {"signalwindow", [](const std::string& a) { return std::format("hl.dsp.window.signal({})", q(a)); }},
            {"mouse", [](const std::string& a) { return a.contains("resizewindow") ? std::string{"hl.dsp.window.resize()"} : std::string{"hl.dsp.window.drag()"}; }},
            {"fullscreen",
             [](const std::string& a) {
                 return std::format(R"(hl.dsp.window.fullscreen({{ mode = "{}" }}))", a == "1" ? "maximized" : "fullscreen");
             }},
            {"resizeactive", [](const std::string& a) { return std::format("hl.dsp.window.resize({{ delta = {} }})", q(a)); }},
            {"moveactive", [](const std::string& a) { return std::format("hl.dsp.window.move({{ delta = {} }})", q(a)); }},
            {"resizewindowpixel", [](const std::string& a) { return std::format("hl.dsp.window.resize({{ pixel = {} }})", q(a)); }},
            {"movewindowpixel", [](const std::string& a) { return std::format("hl.dsp.window.move({{ pixel = {} }})", q(a)); }},
            {"swapnext", [](const std::string& a) { return std::format("hl.dsp.window.swap({{ next = {} }})", a == "prev" ? "false" : "true"); }},
        };

        return MAP;
    }

    std::optional<std::string> dispatchCall(const std::string& dispatcher, const std::string& args) {
        for (const auto& [name, fn] : dispatchers()) {
            if (name != dispatcher)
                continue;
            return fn(args);
        }

        return std::nullopt;
    }
}

static Hyprlang::CParseResult handleBind(const char* command, const char* value) {
    Hyprlang::CParseResult result;

    auto*                  converter = CConverter::active();
    if (!converter)
        return result;

    SBindFlags        flags;
    const std::string KEYWORD = command;

    if (const auto ERROR = parseFlags(std::string_view{KEYWORD}.substr(4), flags)) {
        result.setError(ERROR->c_str());
        return result;
    }

    const size_t ARG_COUNT     = (flags.hasDescription ? 5U : 4U) + (flags.perDevice ? 1U : 0U);
    const auto   ARGS          = CVarList(value, ARG_COUNT);

    const size_t DESCR_OFFSET  = flags.hasDescription ? 1U : 0U;
    const size_t DEVICE_OFFSET = flags.perDevice ? 1U : 0U;

    if (ARGS.size() < 3) {
        result.setError("bind: too few args");
        return result;
    }

    const auto MODS        = ARGS[0];
    const auto KEY         = flags.multiKey ? "" : ARGS[1];
    const auto DEVICES     = flags.perDevice ? ARGS[2] : "";
    const auto DESCRIPTION = flags.hasDescription ? ARGS[2 + DEVICE_OFFSET] : "";

    auto       dispatcher    = ARGS[2 + DESCR_OFFSET + DEVICE_OFFSET];
    const auto DISPATCH_ARGS = flags.mouse ? dispatcher : ARGS[3 + DESCR_OFFSET + DEVICE_OFFSET];

    if (flags.mouse)
        dispatcher = "mouse";

    std::ranges::transform(dispatcher, dispatcher.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const auto KEYS = flags.multiKey ? keyString(MODS, ARGS[1]) : keyString(MODS, KEY);
    const auto CALL = dispatchCall(dispatcher, DISPATCH_ARGS);

    if (!CALL) {
        converter->document().addWarning(std::format("{} = {}: dispatcher \"{}\" has no known Lua equivalent", KEYWORD, value, dispatcher));
        converter->addBind(std::format("-- {} = {}", KEYWORD, value));
        return result;
    }

    if (flags.multiKey)
        converter->document().addWarning(std::format("{} = {}: multi-key binds are written as a single key string; check the result", KEYWORD, value));

    const auto OPTS = optionsTable(flags, DESCRIPTION, DEVICES);

    const auto STATEMENT = OPTS->empty() ? std::format("hl.bind({}, {})", quoteLuaString(KEYS), *CALL) :
                                           std::format("hl.bind({}, {}, {})", quoteLuaString(KEYS), *CALL, OPTS->render());

    converter->addBind(STATEMENT);

    return result;
}

static Hyprlang::CParseResult handleUnbind(const char*, const char* value) {
    auto* converter = CConverter::active();
    if (!converter)
        return {};

    const auto ARGS = CVarList(value, 2);
    converter->addBind(std::format("hl.unbind({})", quoteLuaString(keyString(ARGS[0], ARGS[1]))));
    return {};
}

static Hyprlang::CParseResult handleSubmap(const char*, const char* value) {
    auto* converter = CConverter::active();
    if (!converter)
        return {};

    const auto DATA = CVarList2(std::string{value});
    const auto NAME = std::string{DATA[0]};

    // binds between `submap = name` and `submap = reset` belong to that submap, which the
    // Lua config expresses as a hl.define_submap block rather than a mode switch
    converter->setSubmap(NAME == "reset" ? "" : NAME, std::string{DATA[1]});
    return {};
}

void CConverter::registerBindHandlers() {
    m_config->registerHandler(&::handleBind, "bind", {true});
    m_config->registerHandler(&::handleUnbind, "unbind", {false});
    m_config->registerHandler(&::handleSubmap, "submap", {false});
}
