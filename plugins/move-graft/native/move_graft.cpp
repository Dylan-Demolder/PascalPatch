// move-graft: gives a fighter another fighter's special moves (PascalPatch native plugin).
//
// A special move is the donor's own game code: an entry function in the per-kind special
// tables (ftData_SpecialN/S/Hi/Lw and their air versions) and a run of motion states whose
// callbacks drive it. That code asks for its states by the donor's numbers (Fox's Firefox is
// 353-359, Luigi's Cyclone 357-358), reads the donor's special attributes through
// fp->dat_attrs and sometimes branches on fp->kind. The host fighter has its own states at
// those numbers (Jigglypuff's Rollout sits at 346-362), so the donor code cannot simply be
// pointed at.
//
// So every graft gets its own motion-state table in guest memory: the donor's entries for the
// grafted states, animation ids moved to where the character build put the borrowed
// animations in the host's action table, and every callback wrapped. A wrapper swaps the
// fighter's motion-state table (fp+0x20), special attributes (fp+0x2D4) and, if asked, kind
// (fp+0x04) to the donor's, runs the donor callback, and puts the host's back. The host's
// special entry for the slot points at a wrapper around the donor's entry function. Outside
// a grafted move the fighter is untouched, so its own moves (Jigglypuff's jumps and Rollout)
// keep working.
//
// Config (move-graft.json beside the DLL), written by PascalPatch from the character build:
// {
//   "grafts": [
//     { "fighter": 15, "slot": "hi", "donor": 1, "states": [353, 359],
//       "anims": {"353": 327, ...}, "attrs": "<hex>", "kind": "host" }
//   ]
// }
// fighter/donor are internal kinds (Fox 1, Jigglypuff 15, Luigi 17); states is an inclusive
// range; anims maps a donor state to an action index in the host's own action table; attrs
// is the donor's special attribute block (from its PlXx.dat); kind "donor" runs the donor code
// with the donor's kind, for code that picks articles or effects by kind.
// A graft can name a "character" instead of a "fighter": a new fighter added by the
// extra-fighters plugin, which plays as a different spare kind from match to match. Its graft
// is prepared at load and attached to a kind when extra-fighters calls mm_graft_apply
// (exported below) after making that kind the character's for the match.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/plugin.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

const pp_host* H = nullptr;

// NTSC 1.02 addresses (decomp: src/melee/ft/ftdata.c, fighter.c).
constexpr uint32_t CHARACTER_STATE_TABLES = 0x803C12E0;   // MotionState* per kind
constexpr uint32_t STATE_BASE = 341;                        // fp->x18: first fighter-specific state
constexpr uint32_t STATE_SIZE = 0x20;
constexpr uint32_t FP_KIND = 0x04, FP_STATES = 0x20, FP_ATTRS = 0x2D4;
constexpr int KIND_COUNT = 33;

struct SlotTables { const char* name; uint32_t ground, air; };
const SlotTables SLOTS[] = {
  {"n", 0x803C167C, 0x803C15F8},
  {"s", 0x803C13E8, 0x803C1574},
  {"hi", 0x803C1784, 0x803C146C},
  {"lw", 0x803C1700, 0x803C14F0},
};

void log(const std::string& s) { H->log("move-graft", s.c_str()); }

struct Graft {
  int fighter = -1, donor = -1;
  std::string character;     // set instead of fighter: attached by mm_graft_apply
  uint32_t ground = 0, air = 0;   // the wrapped donor entry functions for the slot
  std::string slot;
  uint32_t first = 0, last = 0;
  std::map<uint32_t, uint32_t> anims;
  std::vector<uint8_t> attrs;
  bool donor_kind = false;
  uint32_t table = 0;        // guest address of this graft's motion-state table (indexed from 341)
  uint32_t attrs_addr = 0;   // guest copy of the donor attributes, 0 = keep the host's
};

struct Call { Graft* g; uint32_t target; };
std::vector<std::unique_ptr<Graft>> g_grafts;
std::vector<std::unique_ptr<Call>> g_calls;
bool g_trace = false;   // "trace": true logs each change of motion state inside grafted code
uint32_t g_last_state = 0;

// Runs a donor function with the donor's tables in place. r3 is the fighter's GObj for every
// callback and entry function the tables hold.
void run_as_donor(pp_cpu* cpu, void* user) {
  const Call* c = static_cast<const Call*>(user);
  uint32_t gobj = H->reg(cpu, 3);
  uint32_t fp = gobj ? H->rd32(gobj + 0x2C) : 0;
  if (!fp) { H->call(cpu, c->target); return; }
  uint32_t states = H->rd32(fp + FP_STATES), attrs = H->rd32(fp + FP_ATTRS), kind = H->rd32(fp + FP_KIND);
  H->wr32(fp + FP_STATES, c->g->table);
  if (c->g->attrs_addr) H->wr32(fp + FP_ATTRS, c->g->attrs_addr);
  if (c->g->donor_kind) H->wr32(fp + FP_KIND, (uint32_t)c->g->donor);
  H->call(cpu, c->target);
  // The fighter object can be gone (a KO inside the callback frees nothing synchronously, but
  // be conservative): only restore what still looks like ours.
  if (H->rd32(fp + FP_STATES) == c->g->table) H->wr32(fp + FP_STATES, states);
  if (c->g->attrs_addr && H->rd32(fp + FP_ATTRS) == c->g->attrs_addr) H->wr32(fp + FP_ATTRS, attrs);
  if (c->g->donor_kind) H->wr32(fp + FP_KIND, kind);
  if (g_trace) {
    uint32_t ms = H->rd32(fp + 0x10);
    if (ms != g_last_state) {
      char msg[160];
      std::snprintf(msg, sizeof msg, "fighter %08X state %u -> %u via %08X at (%.1f, %.1f) %s", fp, g_last_state, ms, c->target,
                    H->rdf32(fp + 0xB0), H->rdf32(fp + 0xB4), H->rd32(fp + 0xE0) ? "air" : "ground");
      log(msg);
      g_last_state = ms;
    }
  }
}

uint32_t wrap(Graft* g, uint32_t target) {
  if (!target) return 0;
  g_calls.push_back(std::make_unique<Call>(Call{g, target}));
  uint32_t t = H->trampoline(run_as_donor, g_calls.back().get());
  if (!t) log("out of trampolines");
  return t;
}

bool install(Graft* g) {
  const SlotTables* slot = nullptr;
  for (auto& s : SLOTS) if (g->slot == s.name) slot = &s;
  if (!slot || (g->character.empty() && (g->fighter < 0 || g->fighter >= KIND_COUNT)) || g->donor < 0 || g->donor >= KIND_COUNT
      || g->first < STATE_BASE || g->last < g->first) {
    log("skipping a graft with a bad slot, kind or state range");
    return false;
  }
  uint32_t donor_table = H->rd32(CHARACTER_STATE_TABLES + 4 * g->donor);
  uint32_t count = g->last - STATE_BASE + 1;
  g->table = H->guest_alloc(count * STATE_SIZE);
  if (!g->table || !donor_table) { log("no guest memory for the state table"); return false; }
  for (uint32_t ms = g->first; ms <= g->last; ++ms) {
    uint32_t src = donor_table + (ms - STATE_BASE) * STATE_SIZE, dst = g->table + (ms - STATE_BASE) * STATE_SIZE;
    uint32_t anim = H->rd32(src);
    auto a = g->anims.find(ms);
    H->wr32(dst + 0x00, a != g->anims.end() ? a->second : anim);
    H->wr32(dst + 0x04, H->rd32(src + 0x04));
    H->wr32(dst + 0x08, H->rd32(src + 0x08));
    for (uint32_t cb = 0x0C; cb < 0x20; cb += 4) H->wr32(dst + cb, wrap(g, H->rd32(src + cb)));
  }
  if (!g->attrs.empty()) {
    g->attrs_addr = H->guest_alloc((uint32_t)g->attrs.size());
    if (!g->attrs_addr) { log("no guest memory for the attribute block"); return false; }
    for (size_t i = 0; i < g->attrs.size(); ++i) H->wr8(g->attrs_addr + (uint32_t)i, g->attrs[i]);
  }
  g->ground = wrap(g, H->rd32(slot->ground + 4 * g->donor));
  g->air = wrap(g, H->rd32(slot->air + 4 * g->donor));
  if (g->character.empty()) {
    H->wr32(slot->ground + 4 * g->fighter, g->ground);
    H->wr32(slot->air + 4 * g->fighter, g->air);
  }
  char msg[200];
  std::snprintf(msg, sizeof msg, "%s%s %s special <- kind %d states %u-%u (table %08X, %zu attr bytes%s)",
                g->character.empty() ? "kind " : "", g->character.empty() ? std::to_string(g->fighter).c_str() : g->character.c_str(),
                g->slot.c_str(), g->donor, g->first, g->last, g->table, g->attrs.size(), g->donor_kind ? ", donor kind" : "");
  log(msg);
  return true;
}

const SlotTables* slot_of(const std::string& name) {
  for (auto& s : SLOTS) if (name == s.name) return &s;
  return nullptr;
}

std::vector<uint8_t> unhex(const std::string& s) {
  std::vector<uint8_t> out;
  for (size_t i = 0; i + 1 < s.size(); i += 2) out.push_back((uint8_t)std::stoul(s.substr(i, 2), nullptr, 16));
  return out;
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char* config_path) {
  if (!host || host->abi != PP_PLUGIN_ABI || !PP_HOST_HAS(host, guest_alloc)) return 1;
  H = host;
  if (!config_path) { log("no move-graft.json; nothing to do"); return 0; }
  nlohmann::json cfg;
  try {
    std::ifstream f(config_path);
    cfg = nlohmann::json::parse(f);
    g_trace = cfg.value("trace", false);
    for (auto& j : cfg.at("grafts")) {
      auto g = std::make_unique<Graft>();
      g->character = j.value("character", std::string());
      g->fighter = g->character.empty() ? j.at("fighter").get<int>() : -1;
      g->donor = j.at("donor").get<int>();
      g->slot = j.at("slot").get<std::string>();
      auto range = j.at("states");
      g->first = range.at(0).get<uint32_t>();
      g->last = range.at(1).get<uint32_t>();
      if (j.find("anims") != j.end())
        for (auto it = j["anims"].begin(); it != j["anims"].end(); ++it)
          g->anims[(uint32_t)std::stoul(it.key())] = it.value().get<uint32_t>();
      if (j.find("attrs") != j.end()) g->attrs = unhex(j["attrs"].get<std::string>());
      g->donor_kind = j.value("kind", std::string("host")) == "donor";
      g_grafts.push_back(std::move(g));
    }
  } catch (const std::exception& e) {
    log(std::string("bad config: ") + e.what());
    return 2;
  }
  int ok = 0;
  for (auto& g : g_grafts) {
    bool good = install(g.get());
    ok += good;
    if (!good) g->table = 0;
  }
  char msg[64];
  std::snprintf(msg, sizeof msg, "%d of %zu grafts installed", ok, g_grafts.size());
  log(msg);
  return 0;
}

// Attaches a character's grafts to the fighter kind it plays as this match (called by the
// extra-fighters plugin once the kind's tables are its base fighter's). Returns how many.
extern "C" __declspec(dllexport) int mm_graft_apply(const char* character, int kind) {
  if (!H || !character || kind < 0 || kind >= KIND_COUNT) return 0;
  int n = 0;
  for (auto& g : g_grafts) {
    const SlotTables* slot = slot_of(g->slot);
    if (!slot || !g->table || g->character != character) continue;
    H->wr32(slot->ground + 4 * kind, g->ground);
    H->wr32(slot->air + 4 * kind, g->air);
    ++n;
  }
  return n;
}
