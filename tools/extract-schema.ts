#!/usr/bin/env bun
// Derives the conversion tables from Hyprland's own sources instead of hand-writing them.
//
// Three tables come out of this:
//   data/options.json      option name -> value type        (from ConfigValues.cpp)
//   data/luaApi.json       the hl.* surface, incl. hl.dsp tree and rule/device field names
//                                                          (from LuaBindings*.cpp)
//   data/dispatchers.json  legacy dispatcher -> hl.dsp path (joined on the Config::Actions
//                                                            symbol both sides call)
//
// The join is the point: a legacy dispatcher and its Lua replacement are "the same thing"
// exactly when they invoke the same Config::Actions entry point. That relation is recorded
// in the sources, so it does not have to be invented here.

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

const REF = join(import.meta.dir, "..", "reference");
const DATA = join(import.meta.dir, "..", "data");

const read = (p: string) => readFileSync(join(REF, p), "utf8");

// ---------------------------------------------------------------- options

export type OptionType = "Bool" | "Int" | "Float" | "String" | "Gradient" | "Color" | "Vec2" | "CssGap" | "FontWeight";

function extractOptions(src: string): Record<string, OptionType> {
    const out: Record<string, OptionType> = {};
    // MS<Bool>("general:no_focus_fallback", "desc", false, {...}),  [MS == makeConfigValue]
    const re = /MS<([A-Za-z]+)>\(\s*"([^"]+)"/g;
    for (const m of src.matchAll(re)) out[m[2]!] = m[1]! as OptionType;
    return out;
}

// special categories are registered separately (device{} in hyprlang, hl.device in Lua)
function extractSpecialCategory(src: string, cat: string): Record<string, OptionType> {
    const out: Record<string, OptionType> = {};
    const re = new RegExp(`addSpecialConfigValue\\("${cat}",\\s*"([^"]+)",\\s*([^)]*)\\)`, "g");
    for (const m of src.matchAll(re)) {
        const raw = m[2]!;
        let type: OptionType = "String";
        if (/Hyprlang::INT/.test(raw)) type = "Int";
        else if (/Hyprlang::FLOAT|\{\s*-?[0-9.]+F/.test(raw)) type = "Float";
        else if (/Hyprlang::VEC2/.test(raw)) type = "Vec2";
        out[m[1]!] = type;
    }
    return out;
}

// ---------------------------------------------------------------- lua api surface

type LuaApi = {
    // top-level hl.<name> config functions
    config: string[];
    // hl.dsp.<path> -> list of dsp_ helper symbols it can push
    dsp: Record<string, string[]>;
    // dsp_ helper -> Config::Actions symbols it calls
    dspActions: Record<string, string[]>;
    // field-name whitelists for the rule/device tables
    fields: Record<string, string[]>;
};

// walks Internal::setFn / setMgrFn / lua_setfield to rebuild the nested hl table layout
function extractRegistration(src: string, fnName: string): { path: string; fn: string }[] {
    const body = sliceFn(src, `void Internal::${fnName}(`);
    const out: { path: string; fn: string }[] = [];
    // pending entries at the current nesting level, flushed when lua_setfield names the level
    const stack: { path: string; fn: string }[][] = [[]];
    for (const line of body.split("\n")) {
        if (/lua_newtable\(L\)/.test(line)) stack.push([]);
        const set = line.match(/Internal::set(?:Mgr)?Fn\(L,\s*(?:mgr,\s*)?"([^"]+)",\s*([A-Za-z0-9_]+)\)/);
        if (set) stack.at(-1)!.push({ path: set[1]!, fn: set[2]! });
        const close = line.match(/lua_setfield\(L,\s*-2,\s*"([^"]+)"\)/);
        if (close && stack.length > 1) {
            const level = stack.pop()!;
            for (const e of level) stack.at(-1)!.push({ path: `${close[1]}.${e.path}`, fn: e.fn });
        }
    }
    out.push(...stack[0]!);
    return out;
}

// hl* binding body -> which dsp_ closures it can push
function extractDspClosures(src: string, hlFn: string): string[] {
    const body = sliceFn(src, `static int ${hlFn}(lua_State* L)`);
    return [...new Set([...body.matchAll(/lua_pushcclosure\(L,\s*(dsp_[A-Za-z0-9_]+)/g)].map((m) => m[1]!))];
}

// dsp_ closure body -> which Config::Actions symbols it calls (CA:: is the alias in-tree)
function extractDspActions(src: string): Record<string, string[]> {
    const out: Record<string, string[]> = {};
    for (const m of src.matchAll(/static int (dsp_[A-Za-z0-9_]+)\(lua_State\* L\)/g)) {
        const body = sliceFn(src, m[0]!);
        const acts = [...new Set([...body.matchAll(/\b(?:CA|Actions|Config::Actions)::([a-zA-Z0-9_]+)/g)].map((x) => x[1]!))];
        out[m[1]!] = acts.filter((a) => !/^e[A-Z]/.test(a)); // drop enum types (eTogglableAction)
    }
    return out;
}

// { name, factory } descriptor tables, e.g. DEVICE_FIELDS / WINDOW_RULE_FIELDS
function extractFieldTable(src: string, tableName: string): string[] {
    const at = src.indexOf(tableName);
    if (at < 0) return [];
    const open = src.indexOf("{", at);
    const body = balanced(src, open, "{", "}");
    return [...new Set([...body.matchAll(/\{\s*"([^"]+)"/g)].map((m) => m[1]!))];
}

// ---------------------------------------------------------------- legacy dispatchers

// m_dispMap["name"] = ::fn;  then fn's body -> Config::Actions symbols
function extractLegacyDispatchers(src: string): Record<string, { fn: string; actions: string[] }> {
    const out: Record<string, { fn: string; actions: string[] }> = {};
    for (const m of src.matchAll(/m_dispMap\["([^"]+)"\]\s*=\s*(?:::([A-Za-z0-9_]+)|\[)/g)) {
        const name = m[1]!;
        const fn = m[2];
        if (!fn) {
            out[name] = { fn: "<inline>", actions: [] }; // deprecated no-ops
            continue;
        }
        const def = new RegExp(`^static \\S[^\\n]*\\b${fn}\\(`, "m").exec(src);
        const body = def ? sliceFn(src, def[0]!) : "";
        const actions = [...new Set([...body.matchAll(/\bActions::([a-zA-Z0-9_]+)/g)].map((x) => x[1]!))];
        out[name] = { fn, actions: actions.filter((a) => !/^e[A-Z]/.test(a)) };
    }
    return out;
}

// ---------------------------------------------------------------- helpers

function sliceFn(src: string, signature: string): string {
    const at = src.indexOf(signature);
    if (at < 0) return "";
    const open = src.indexOf("{", at);
    if (open < 0) return "";
    return balanced(src, open, "{", "}");
}

function balanced(src: string, openIdx: number, open: string, close: string): string {
    let depth = 0;
    for (let i = openIdx; i < src.length; i++) {
        const c = src[i]!;
        if (c === open) depth++;
        else if (c === close) {
            depth--;
            if (depth === 0) return src.slice(openIdx, i + 1);
        }
    }
    return src.slice(openIdx);
}

// ---------------------------------------------------------------- main

const configValues = read("values/ConfigValues.cpp");
const legacyMgr = read("legacy/ConfigManager.cpp");
const translator = read("legacy/DispatcherTranslator.cpp");
const luaRules = read("lua/LuaBindingsConfigRules.cpp");
const luaDisp = read("lua/LuaBindingsDispatchers.cpp");

const options = extractOptions(configValues);
const device = extractSpecialCategory(legacyMgr, "device");

const dspReg = extractRegistration(luaDisp, "registerDispatcherBindings");
const cfgReg = extractRegistration(luaRules, "registerConfigRuleBindings");
const dspActions = extractDspActions(luaDisp);

const dsp: Record<string, string[]> = {};
for (const { path, fn } of dspReg) dsp[path] = extractDspClosures(luaDisp, fn);

const luaApi: LuaApi = {
    config: cfgReg.map((e) => e.path),
    dsp,
    dspActions,
    fields: {
        device: extractFieldTable(luaRules, "DEVICE_FIELDS"),
        window_rule: extractFieldTable(luaRules, "WINDOW_RULE_FIELDS"),
        layer_rule: extractFieldTable(luaRules, "LAYER_RULE_FIELDS"),
        workspace_rule: extractFieldTable(luaRules, "WORKSPACE_RULE_FIELDS"),
        monitor: extractFieldTable(luaRules, "MONITOR_FIELDS"),
    },
};

const legacy = extractLegacyDispatchers(translator);

// the join: legacy dispatcher -> candidate hl.dsp paths, via shared Config::Actions symbol
const actionToDsp = new Map<string, string[]>();
for (const [path, closures] of Object.entries(dsp)) {
    for (const c of closures) {
        for (const a of dspActions[c] ?? []) {
            if (!actionToDsp.has(a)) actionToDsp.set(a, []);
            if (!actionToDsp.get(a)!.includes(path)) actionToDsp.get(a)!.push(path);
        }
    }
}

const dispatchers: Record<string, { actions: string[]; candidates: string[] }> = {};
for (const [name, info] of Object.entries(legacy)) {
    const candidates = [...new Set(info.actions.flatMap((a) => actionToDsp.get(a) ?? []))];
    dispatchers[name] = { actions: info.actions, candidates };
}

writeFileSync(join(DATA, "options.json"), JSON.stringify({ options, special: { device } }, null, 2) + "\n");
writeFileSync(join(DATA, "luaApi.json"), JSON.stringify(luaApi, null, 2) + "\n");
writeFileSync(join(DATA, "dispatchers.json"), JSON.stringify(dispatchers, null, 2) + "\n");

const unmapped = Object.entries(dispatchers).filter(([, d]) => d.candidates.length === 0).map(([n]) => n);
const ambiguous = Object.entries(dispatchers).filter(([, d]) => d.candidates.length > 1).map(([n]) => n);

console.log(`options:            ${Object.keys(options).length} (+${Object.keys(device).length} device)`);
console.log(`hl.* config fns:    ${luaApi.config.length}`);
console.log(`hl.dsp paths:       ${Object.keys(dsp).length}`);
console.log(`dsp_ closures:      ${Object.keys(dspActions).length}`);
console.log(`legacy dispatchers: ${Object.keys(dispatchers).length}`);
console.log(`  unique match:     ${Object.keys(dispatchers).length - unmapped.length - ambiguous.length}`);
console.log(`  ambiguous:        ${ambiguous.length}  ${ambiguous.join(" ")}`);
console.log(`  no candidate:     ${unmapped.length}  ${unmapped.join(" ")}`);
for (const [k, v] of Object.entries(luaApi.fields)) console.log(`fields.${k}: ${v.length}`);
