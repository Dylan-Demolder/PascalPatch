// extra-fighters: adds new fighters to the game (PascalPatch native plugin).
//
// Every fighter the game can build is a fighter kind, and everything that makes a fighter a
// fighter is looked up per kind in tables in writable RAM (src/melee/ft/ftdata.c): the file
// names, the on-load/on-death hooks, the special-move entries, the motion-state tables, the
// item callbacks. Melee has 33 kinds, and three of them are never used in VS mode: the male and
// female Wireframes and the Sandbag. For each match, every new fighter a player picked is lent
// one of those kinds: the kind becomes a copy of the new fighter's base fighter (its row in
// every table), except for the file names, which point at the new fighter's own files on the
// disc (PlX0.dat, PlX0Nr.dat, PlX0AJ.dat, added by PascalPatch). So the base fighter stays in the
// game untouched, any number of new fighters can be on the select screen, and up to three
// different ones can play in one match. The game drops a kind's loaded files at the start of
// every match (Fighter_800679B0), so a kind can hold a different fighter each match; while no
// new fighter holds it, a kind has its own rows back (the Wireframes of Multi-Man Melee, the
// Sandbag of the Home-Run Contest).
//
// Config (extra-fighters.json beside the DLL), written by PascalPatch:
// {
//   "fighters": [
//     { "id": "chungus", "name": "Chungus", "base": 15, "base_ckind": 15,
//       "dat": "PlX0.dat", "costume": "PlX0Nr.dat", "anim": "PlX0AJ.dat",
//       "portrait": "chungus.portrait", "icon": "chungus.icon" }
//   ],
//   "slots": [
//     { "id": "hulk", "ckind": 1, "name": "Hulk", "portrait": "hulk.portrait", "icon": "hulk.icon" }
//   ]
// }
// base is the base fighter's internal kind (Jigglypuff 15) and base_ckind its CSS kind; a
// file left out is the base fighter's own. "slots" dresses fighters that replace an existing
// one (their files are already on the disc): the CSS kind's name, door portrait and grid
// icon. A portrait file is the 136x188 door image as the GameCube reads it (RGB5A3, 4x4
// tiles, big-endian), written by PascalPatch from the Character Studio render or photo; an icon
// file is the 64x56 grid icon in the same format. Names take ASCII letters, digits and spaces.
// Grafted specials (the move-graft plugin) follow the fighter to whichever kind it gets.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "pascalpatch/plugin.h"

#include <nlohmann/json.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

const pp_host* H = nullptr;

void log(const std::string& s) { H->log("extra-fighters", s.c_str()); }

constexpr int KIND_COUNT = 33;
// The kinds new fighters borrow (src/melee/ft/forward.h). The Hands and Giga Bowser are left
// alone: the game's own code checks for them all over (boss logic, the CPU, throws).
const int SPARE_KINDS[] = {0x1E, 0x1D, 0x20};   // female Wireframe, male Wireframe, Sandbag
constexpr int SPARES = sizeof SPARE_KINDS / sizeof SPARE_KINDS[0];

// Per-kind tables (NTSC 1.02, config/GALE01/symbols.txt), copied row by row from the base.
struct Table { uint32_t addr; uint32_t stride; };
const Table COPIED[] = {
  {0x803C0FC8, 8},  // ftData_Table_Unk0
  {0x803C10D0, 4},  // ftData_Table_Unk1
  {0x803C1154, 4},  // ftData_OnLoad
  {0x803C11D8, 4},  // ftData_OnDeath
  {0x803C125C, 4},  // ftData_OnUserDataRemove
  {0x803C12E0, 4},  // ftData_CharacterStateTables
  {0x803C1364, 4},  // ftData_UnkMotionStates0
  {0x803C13E8, 4},  // ftData_SpecialS
  {0x803C146C, 4},  // ftData_SpecialAirHi
  {0x803C14F0, 4},  // ftData_SpecialAirLw
  {0x803C1574, 4},  // ftData_SpecialAirS
  {0x803C15F8, 4},  // ftData_SpecialAirN
  {0x803C167C, 4},  // ftData_SpecialN
  {0x803C1700, 4},  // ftData_SpecialLw
  {0x803C1784, 4},  // ftData_SpecialHi
  {0x803C1808, 4},  // ftData_OnAbsorb
  {0x803C188C, 4},  // ftData_OnItemPickupExt
  {0x803C1910, 4},  // ftData_OnItemInvisible
  {0x803C1994, 4},  // ftData_OnItemVisible
  {0x803C1A18, 4},  // ftData_OnItemDropExt
  {0x803C1A9C, 4},  // ftData_OnItemPickup
  {0x803C1B20, 4},  // ftData_OnItemDrop
  {0x803C1BA4, 4},  // ftData_UnkMotionStates1
  {0x803C1C28, 4},  // ftData_UnkMotionStates2
  {0x803C1CAC, 4},  // ftData_OnKnockbackEnter
  {0x803C1D30, 4},  // ftData_OnKnockbackExit
  {0x803C1DB4, 4},  // ftData_UnkMotionStates3
  {0x803C1E38, 4},  // ftData_UnkMotionStates4
  {0x803C1EBC, 4},  // ftKindCalcIndiviParamTable
  {0x803C1F40, 8},  // ftData_803C1F40: {dat file name, data symbol}
  {0x803C2048, 4},  // ftData_UnkMotionStates5
  {0x803C20CC, 4},  // ftData_UnkMtxFunc0
  {0x803C2150, 8},  // ftData_UnkIntBoolFunc0
  {0x803C2258, 8},  // ftData_UnkCallbackPairs0
  {0x803C23E4, 4},  // ftData_803C23E4: animation file name
  {0x803C2468, 4},  // ftData_803C2468 (demo motion symbol names)
  {0x803C24EC, 4},  // ftData_803C24EC (motion file name getter)
  {0x803C2570, 4},  // ftData_UnkDemoCallbacks0
  {0x803C25F4, 8},  // ftData_UnkIntPairs
  {0x803C26FC, 1},  // ftData_UnkBytePerCharacter
  {0x803C9FC8, 4},  // ftKb_Init_803C9FC8 (Kirby's copy hat per kind)
  {0x803CA9D0, 8},  // ftKb_Init_803CA9D0 (Kirby's copy name per kind)
  {0x803CB46C, 1},  // ftKb_Init_803CB46C
};
constexpr uint32_t FILE_NAMES = 0x803C1F40;       // {dat file name, data symbol} per kind
constexpr uint32_t COSTUME_STRINGS = 0x803C2360;  // Fighter_CostumeStrings[] per kind
constexpr uint32_t ANIM_FILES = 0x803C23E4;       // animation file name per kind
// Per-kind tables that live in PlCo.dat, reached through pointers the game fills in when it
// loads that file (Fighter_LoadCommonData, at the start of every match, in the same frame the
// fighters are built): the skeleton part map and the part flags.
constexpr uint32_t LOAD_COMMON_DATA = 0x80067ABC;
constexpr uint32_t PLCO_TABLES[] = {
  0x804D6544,  // ftPartsTable: FighterPartsTable* per kind
  0x804D6540,  // Fighter_804D6540: per-kind part flags
};
constexpr int PLCO_COUNT = sizeof PLCO_TABLES / sizeof PLCO_TABLES[0];

struct Fighter {
  std::string id;
  int base = -1, base_ckind = -1, proxy = -1;   // proxy: the base fighter's CSS icon entry
  uint32_t name = 0;                            // Shift-JIS name in guest memory, or 0
  uint32_t dat = 0, costume = 0, anim = 0;      // file names in guest memory, 0 = the base's
  int portrait = -1, icon = -1, stock = -1;     // index into g_images
  int page = 0, spot = -1;                      // where on the select screen (see layout())
};
std::vector<Fighter> g_fighters;

// A spare kind as the game has it, to put back when no new fighter holds it.
struct Spare { int kind; int holder = -1; std::vector<uint8_t> rows; uint8_t costume[12]; };
Spare g_spares[SPARES];

std::vector<uint8_t> save_rows(int kind) {
  std::vector<uint8_t> out;
  for (const Table& t : COPIED)
    for (uint32_t b = 0; b < t.stride; ++b) out.push_back(H->rd8(t.addr + kind * t.stride + b));
  return out;
}

void load_rows(int kind, const std::vector<uint8_t>& rows) {
  size_t i = 0;
  for (const Table& t : COPIED)
    for (uint32_t b = 0; b < t.stride; ++b) H->wr8(t.addr + kind * t.stride + b, rows[i++]);
}

// move-graft's mm_graft_apply(character, kind), if that plugin is loaded.
using GraftApply = int (*)(const char*, int);
GraftApply graft_apply() {
  static GraftApply fn = nullptr;
  static bool looked = false;
  if (!looked) {
    looked = true;
    if (HMODULE m = GetModuleHandleA("move-graft.dll")) fn = (GraftApply)(void*)GetProcAddress(m, "mm_graft_apply");
  }
  return fn;
}

// Makes the spare kind a copy of fighter f's base, with f's files, or (f < 0) itself again.
void lend(Spare& s, int f) {
  if (s.holder == f) return;
  s.holder = f;
  uint32_t costume = H->rd32(COSTUME_STRINGS + s.kind * 4);   // its first costume's strings
  if (f < 0) {
    load_rows(s.kind, s.rows);
    for (uint32_t b = 0; b < 12; ++b) H->wr8(costume + b, s.costume[b]);
    return;
  }
  const Fighter& x = g_fighters[f];
  load_rows(s.kind, save_rows(x.base));
  uint32_t theirs = H->rd32(COSTUME_STRINGS + x.base * 4);
  for (uint32_t b = 0; b < 12; b += 4) H->wr32(costume + b, H->rd32(theirs + b));   // file, joint, matanim
  if (x.dat) H->wr32(FILE_NAMES + s.kind * 8, x.dat);
  if (x.costume) H->wr32(costume, x.costume);
  if (x.anim) H->wr32(ANIM_FILES + s.kind * 4, x.anim);
  int grafts = graft_apply() ? graft_apply()(x.id.c_str(), s.kind) : 0;
  char msg[160];
  std::snprintf(msg, sizeof msg, "%s plays as kind %d this match (a copy of kind %d%s)", x.id.c_str(), s.kind, x.base,
                grafts ? " with its grafted specials" : "");
  log(msg);
}

int kind_of(int f) {
  for (const Spare& s : g_spares) if (f >= 0 && s.holder == f) return s.kind;
  return -1;
}

// PlCo.dat's per-kind rows: the base's for a lent kind, the kind's own otherwise.
uint32_t g_load_common = 0;
uint32_t g_plco_table[PLCO_COUNT];
uint32_t g_plco_own[PLCO_COUNT][SPARES];

void load_common_data(pp_cpu* cpu, void*) {
  H->call(cpu, g_load_common);
  for (int t = 0; t < PLCO_COUNT; ++t) {
    uint32_t table = H->rd32(PLCO_TABLES[t]);
    if (table < 0x80000000u || table >= 0x81800000u) continue;
    if (table != g_plco_table[t]) {   // a new copy of the file: its rows are the game's own
      g_plco_table[t] = table;
      for (int i = 0; i < SPARES; ++i) g_plco_own[t][i] = H->rd32(table + g_spares[i].kind * 4);
    }
    for (int i = 0; i < SPARES; ++i) {
      const Spare& s = g_spares[i];
      H->wr32(table + s.kind * 4, s.holder >= 0 ? H->rd32(table + g_fighters[s.holder].base * 4) : g_plco_own[t][i]);
    }
  }
}

// ---- character select screen ----
// The CSS keeps 25 icons (mnCharSel icons[], 0x19 doubles as "no icon") and every loop stops
// there, so new fighters get hit regions of their own instead of new table entries. While the
// game runs a cursor whose token sits in a new fighter's region, the base fighter's icon entry
// (the proxy) takes the region's bounds for the length of that call, so the CSS only ever sees
// the base fighter's CSS kind; the plugin remembers which doors picked the new fighter, and at
// match load those players are built from the kind the new fighter was lent.
//
// Layout: with one or two new fighters, they take the two free spaces of the bottom row (right
// of Roy, left of Pichu). With more, the select screen has pages: right of Roy is a page arrow
// (A flips to the next page), the first new fighter stays left of Pichu, and every further page
// shows up to 25 new fighters in the places of the game's icons, which are hidden and cannot be
// picked there. A fighter picked on another page stays picked while its token is not moved.
constexpr uint32_t CURSOR_THINK = 0x802602A0;   // mnCharSel_CursorThink (GObj proc)
constexpr uint32_t DOOR_UPDATE = 0x8025DB34;    // mnCharSel_8025DB34(door): name plate, portrait
constexpr uint32_t TOKENS = 0x804A0BD0;         // CSSCharModel* per door; x at +8, y at +0xC
constexpr uint32_t DOORS = 0x803F0DFC;          // mnCharSel doors, 0x24 bytes each
constexpr uint32_t DOOR_SIZE = 0x24, DOOR_SEL_ICON = 0x0E;
constexpr uint32_t CSS_ICONS = 0x803F0B24;      // mnCharSel icons[25], 0x1C bytes each
constexpr uint32_t CSS_ICON_SIZE = 0x1C;
constexpr int ICON_COUNT = 25;
constexpr uint32_t ICON_STATE = 0x02;           // 0 locked, 1 unlocked, 2 unlocked and shown
constexpr uint32_t ICON_BOUNDS = 0x0C;          // bound_l, bound_r, bound_u, bound_d
constexpr uint32_t CURSOR_PORT = 0x04, CURSOR_X = 0x0C, CURSOR_Y = 0x10;
constexpr float TOKEN_DX = 2.7f, TOKEN_DY = -2.0f;   // a held token's place relative to the hand
constexpr uint32_t PADS = 0x804C20BC;           // HSD_PadCopyStatus[4], 0x44 bytes each
constexpr uint32_t PAD_SIZE = 0x44, PAD_TRIGGER = 0x08, PAD_A = 0x100;
constexpr uint32_t NAMES_US = 0x803D4FDC, NAMES_JP = 0x803D4D74;   // char* per CSS kind
// Match load (pl/player.c, lb/lbdvd.c).
constexpr uint32_t FT_MAPPING = 0x803BCDE0;     // ftMapping_list: {internal id, extra id, flag} per CSS kind
constexpr uint32_t PLAYER_CREATE = 0x80031AD0;  // Player_80031AD0(slot): builds the slot's fighter
constexpr uint32_t PRELOAD_CKIND = 0x80031CB0;  // Player_80031CB0(ckind, color): queues its files
constexpr uint32_t FT_PRELOAD = 0x800855C8;     // ftData_800855C8(kind, color)
constexpr uint32_t SET_COSTUME = 0x80033208;    // Player_SetCostumeId(slot, costume)
struct Region { float l, r, u, d; };
const Region RIGHT_OF_ROY = {24.4f, 30.2f, 6.0f, -1.0f};
const Region LEFT_OF_PICHU = {-30.0f, -23.4f, 6.0f, -1.0f};
// Columns of the icon grid (the top row's nine icons are columns 0-8; the bottom row's seven
// are 1-7): where the free spaces' icons are drawn.
constexpr int COLUMN_RIGHT_OF_ROY = 8, COLUMN_LEFT_OF_PICHU = 0;
constexpr int SPOT_RIGHT_OF_ROY = -1, SPOT_LEFT_OF_PICHU = -2;   // Fighter::spot outside the grid
int g_pages = 1, g_page = 0;
bool g_arrow = false;                // right of Roy is the page arrow
Region g_grid[ICON_COUNT];           // the game's icons' hit regions, read once
int g_pick[4] = {-1, -1, -1, -1};    // per door (= player slot): index into g_fighters, or -1
float g_pick_at[4][2];               // where its token was put down
uint32_t g_cursor_think = 0, g_door_update = 0, g_player_create = 0, g_preload_ckind = 0;

Region region_of(const Fighter& f) {
  if (f.spot == SPOT_RIGHT_OF_ROY) return RIGHT_OF_ROY;
  if (f.spot == SPOT_LEFT_OF_PICHU) return LEFT_OF_PICHU;
  return g_grid[f.spot];
}

bool inside(const Region& b, float x, float y) { return x > b.l && x < b.r && y < b.u && y > b.d; }

// The new fighter on the page shown whose region holds (x, y), or -1.
int fighter_at(float x, float y) {
  for (size_t i = 0; i < g_fighters.size(); ++i)
    if (g_fighters[i].page == g_page && g_fighters[i].proxy >= 0 && inside(region_of(g_fighters[i]), x, y)) return (int)i;
  return -1;
}

bool token_at(int door, float& x, float& y) {
  uint32_t token = H->rd32(TOKENS + door * 4);
  if (!token) return false;
  x = H->rdf32(token + 8); y = H->rdf32(token + 0xC);
  return true;
}

int token_fighter(int door) {
  float x, y;
  return token_at(door, x, y) ? fighter_at(x, y) : -1;
}

// Which new fighter the door has picked, from the CSS state as it is now.
int door_pick(int door) {
  float x, y;
  if (!token_at(door, x, y)) return -1;
  uint8_t sel = H->rd8(DOORS + door * DOOR_SIZE + DOOR_SEL_ICON);
  int c = fighter_at(x, y);
  if (c >= 0) return sel == g_fighters[c].proxy ? c : -1;
  // Picked on another page: kept while the token lies where it was put down.
  int was = g_pick[door];
  return was >= 0 && sel == g_fighters[was].proxy && x == g_pick_at[door][0] && y == g_pick_at[door][1] ? was : -1;
}

// Lends the spare kinds to the new fighters the doors have picked: a fighter keeps the kind it
// has, a kind no pick needs goes back to itself. A pick beyond the spare kinds is dropped (the
// player gets the base fighter).
void lend_kinds() {
  bool wanted[SPARES] = {};
  for (int d = 0; d < 4; ++d)
    for (int i = 0; i < SPARES; ++i) if (g_pick[d] >= 0 && g_spares[i].holder == g_pick[d]) wanted[i] = true;
  for (int i = 0; i < SPARES; ++i) if (!wanted[i]) lend(g_spares[i], -1);
  for (int d = 0; d < 4; ++d) {
    int f = g_pick[d];
    if (f < 0 || kind_of(f) >= 0) continue;
    Spare* free_kind = nullptr;
    for (Spare& s : g_spares) if (s.holder < 0 && !free_kind) free_kind = &s;
    if (free_kind) { lend(*free_kind, f); continue; }
    log(g_fighters[f].id + ": three new fighters are picked already; this player gets the base fighter");
    g_pick[d] = -1;
  }
}

void dress_css(pp_cpu* cpu);
void show_page(pp_cpu* cpu);
bool in_css_vs();

void cursor_think(pp_cpu* cpu, void*) {
  uint32_t gobj = H->reg(cpu, 3), cursor = H->rd32(gobj + 0x2C);   // gobj->user_data: CSSCursorData
  dress_css(cpu);   // calls into the game: the cursor's own call gets its argument back below
  H->set_reg(cpu, 3, gobj);
  bool vs = in_css_vs();
  // A held token belongs to door x6; otherwise the cursor's own port.
  uint8_t door = H->rd8(cursor + 5) == 1 ? H->rd8(cursor + 6) : H->rd8(cursor + 4);
  int c = vs && door < 4 ? token_fighter(door) : -1;
  // The game tests icons with the token's position, which rides at this offset from the hand.
  float cx = H->rdf32(cursor + CURSOR_X) + TOKEN_DX, cy = H->rdf32(cursor + CURSOR_Y) + TOKEN_DY;
  bool on_arrow = vs && g_arrow && inside(RIGHT_OF_ROY, cx, cy);
  bool token_on_arrow = false;
  { float x, y; token_on_arrow = vs && g_arrow && door < 4 && token_at(door, x, y) && inside(RIGHT_OF_ROY, x, y); }
  bool touch = c >= 0 || g_page > 0 || token_on_arrow;
  uint8_t saved[ICON_COUNT * CSS_ICON_SIZE];
  if (touch) {
    for (uint32_t i = 0; i < sizeof saved; ++i) saved[i] = H->rd8(CSS_ICONS + i);
    for (int i = 0; i < ICON_COUNT; ++i) {
      uint32_t state = CSS_ICONS + i * CSS_ICON_SIZE + ICON_STATE;
      // The free spaces are also the game's hidden "random fighter" spots, live once every
      // icon is unlocked and shown (state 2); "unlocked, not shown" (1) keeps an icon
      // selectable but turns the random pick off. On a page of new fighters the game's own
      // icons cannot be picked (0).
      if (g_page > 0) H->wr8(state, 0);
      else if (H->rd8(state) == 2) H->wr8(state, 1);
    }
    if (c >= 0) {
      uint32_t icon = CSS_ICONS + g_fighters[c].proxy * CSS_ICON_SIZE;
      Region b = region_of(g_fighters[c]);
      H->wrf32(icon + ICON_BOUNDS, b.l); H->wrf32(icon + ICON_BOUNDS + 4, b.r);
      H->wrf32(icon + ICON_BOUNDS + 8, b.u); H->wrf32(icon + ICON_BOUNDS + 12, b.d);
      H->wr8(icon + ICON_STATE, 1);
    }
  }
  H->call(cpu, g_cursor_think);
  if (touch) for (uint32_t i = 0; i < sizeof saved; ++i) H->wr8(CSS_ICONS + i, saved[i]);
  uint8_t port = H->rd8(cursor + CURSOR_PORT);
  if (on_arrow && port < 4 && (H->rd32(PADS + port * PAD_SIZE + PAD_TRIGGER) & PAD_A)) {
    g_page = (g_page + 1) % g_pages;
    char msg[64]; std::snprintf(msg, sizeof msg, "select screen page %d of %d", g_page + 1, g_pages);
    log(msg);
  }
  show_page(cpu);
}

// ---- pictures ----
// A door's portrait is a 136x188 paletted (CI8) texture that a texture animation picks per
// fighter and costume whenever the door updates (mnCharSel_8025D5AC). Right after that, a door
// showing a fighter with its own portrait gets that instead: an RGB5A3 image (no palette, so
// the door's palette is ignored) in the CSS scene's heap, set as the image of the door's
// portrait textures. The next update of the door animates the game's own image back.
constexpr uint32_t DOOR_PORTRAIT = 0x8025D5AC;  // mnCharSel_8025D5AC(door, frame, hidden)
constexpr uint32_t CSS_ROOT = 0x804D6CC0;       // the CSS menu's root JObj
constexpr uint32_t CSS_SETUPS = 0x804D6CF8;     // bumped every time the CSS scene is set up
constexpr uint32_t CSS_1P = 0x804D6CF5;         // 1 on the 1P-mode CSS (other portrait joints)
constexpr uint32_t DOOR_COSTUME_JOINT = 0x01;   // CSSDoor.costume_joint: the portrait's JObj
constexpr uint32_t JOBJ_BY_INDEX = 0x80011E24;  // lb_80011E24(root, &jobj, index, -1)
constexpr uint32_t MEM_ALLOC = 0x8037F1E4;      // HSD_MemAlloc(size), from the current scene's heap
constexpr uint32_t JOBJ_NO_DOBJ = 0x4020;       // JOBJ_SPLINE | JOBJ_PTCL: u is not a DObj
constexpr uint32_t TOBJ_IMAGEDESC = 0x58, TOBJ_AOBJ = 0x64;
constexpr uint16_t PORTRAIT_W = 136, PORTRAIT_H = 188;
constexpr uint16_t ICON_W = 64, ICON_H = 56;     // a grid icon's fighter image (CI8 in the game)
constexpr uint32_t GX_TF_RGB5A3 = 5, GX_TF_CI8 = 9;
constexpr int PLAYABLE_CKINDS = 0x1A;
// An RGB5A3 texture for the CSS: the file's bytes, and its image descriptor in this CSS
// scene's heap (freed with the scene), made on first use in each scene.
struct Image { std::vector<uint8_t> data; uint16_t w = 0, h = 0; uint32_t desc = 0; uint64_t session = ~0ull; };
std::vector<Image> g_images;
int g_slot_portrait[PLAYABLE_CKINDS];   // per CSS kind (replaced slots): index into g_images, or -1
int g_slot_icon[PLAYABLE_CKINDS];
int g_slot_stock[PLAYABLE_CKINDS];
std::vector<int> g_arrow_images;        // the page arrow, one per page shown
uint32_t g_door_portrait = 0, g_scratch = 0;

bool in_css_vs() { return H->rd32(CSS_ROOT) && H->rd8(CSS_1P) != 1; }

// An image file: the texture as the GameCube reads it (RGB5A3, 4x4 tiles, big-endian).
int load_image(const std::string& dir, const std::string& file, uint16_t w, uint16_t h, const std::string& who) {
  std::ifstream in(dir + "/" + file, std::ios::binary);
  std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (data.size() != (size_t)w * h * 2) {
    char msg[160]; std::snprintf(msg, sizeof msg, "%s: %s is not a %ux%u RGB5A3 image", who.c_str(), file.c_str(), w, h);
    log(msg); return -1;
  }
  g_images.push_back({std::move(data), w, h});
  return (int)g_images.size() - 1;
}

uint64_t css_session() { return ((uint64_t)H->rd8(CSS_SETUPS) << 32) | H->rd32(CSS_ROOT); }

uint32_t heap_alloc(pp_cpu* cpu, uint32_t size) {
  H->set_reg(cpu, 3, size);
  H->call(cpu, MEM_ALLOC);
  return H->reg(cpu, 3);
}

uint32_t image_desc(pp_cpu* cpu, Image& p) {
  uint64_t session = css_session();
  if (p.desc && p.session == session) return p.desc;
  uint32_t bytes = (uint32_t)p.data.size(), block = heap_alloc(cpu, bytes + 64);
  if (!block) { log("no heap left on the character select screen for an image"); return 0; }
  uint32_t desc = (block + 31) & ~31u, image = desc + 32;   // GX textures are 32-byte aligned
  const uint8_t* b = p.data.data();
  for (uint32_t i = 0; i < bytes; i += 4)
    H->wr32(image + i, (uint32_t)b[i] << 24 | (uint32_t)b[i + 1] << 16 | (uint32_t)b[i + 2] << 8 | b[i + 3]);
  // HSD_ImageDesc: image_ptr, width, height, format, mipmap, minLOD, maxLOD
  H->wr32(desc, image); H->wr16(desc + 4, p.w); H->wr16(desc + 6, p.h);
  H->wr32(desc + 8, GX_TF_RGB5A3); H->wr32(desc + 12, 0); H->wrf32(desc + 16, 0); H->wrf32(desc + 20, 0);
  p.desc = desc; p.session = session;
  return desc;
}

// Every texture under `jobj` of the image's size (and, if given, format) shows `desc`. With
// `still`, the texture's animation is dropped so no later frame puts the game's image back.
void set_images(uint32_t jobj, uint32_t desc, int depth, uint32_t format = ~0u, bool still = false) {
  if (!jobj || depth > 8) return;
  uint16_t w = H->rd16(desc + 4), h = H->rd16(desc + 6);
  if (!(H->rd32(jobj + 0x14) & JOBJ_NO_DOBJ))
    for (uint32_t d = H->rd32(jobj + 0x18); d; d = H->rd32(d + 4)) {      // DObj list
      uint32_t m = H->rd32(d + 8);                                          // MObj
      for (uint32_t t = m ? H->rd32(m + 8) : 0; t; t = H->rd32(t + 8)) {    // TObj list
        uint32_t id = H->rd32(t + TOBJ_IMAGEDESC);
        if (!id || H->rd16(id + 4) != w || H->rd16(id + 6) != h || (format != ~0u && H->rd32(id + 8) != format)) continue;
        H->wr32(t + TOBJ_IMAGEDESC, desc);
        if (still) H->wr32(t + TOBJ_AOBJ, 0);
      }
    }
  for (uint32_t c = H->rd32(jobj + 0x10); c; c = H->rd32(c + 8)) set_images(c, desc, depth + 1, format, still);
}

uint32_t jobj_by_index(pp_cpu* cpu, uint32_t index) {
  if (!(g_scratch || (g_scratch = H->guest_alloc(32)))) return 0;
  H->wr32(g_scratch, 0);
  H->set_reg(cpu, 3, H->rd32(CSS_ROOT)); H->set_reg(cpu, 4, g_scratch);
  H->set_reg(cpu, 5, index); H->set_reg(cpu, 6, 0xFFFFFFFFu);
  H->call(cpu, JOBJ_BY_INDEX);
  return H->rd32(g_scratch);
}

void door_portrait(pp_cpu* cpu, void*) {
  uint32_t door = H->reg(cpu, 3), hidden = H->reg(cpu, 5) & 0xFF;
  H->call(cpu, g_door_portrait);
  if (door >= 4 || hidden || H->rd8(CSS_1P) == 1) return;
  int p = -1;
  if (g_pick[door] >= 0) {
    p = g_fighters[g_pick[door]].portrait;
  } else {
    uint8_t sel = H->rd8(DOORS + door * DOOR_SIZE + DOOR_SEL_ICON);
    uint8_t ckind = sel < ICON_COUNT ? H->rd8(CSS_ICONS + sel * CSS_ICON_SIZE + 1) : 0xFF;
    if (ckind < PLAYABLE_CKINDS) p = g_slot_portrait[ckind];
  }
  if (p < 0) return;
  uint32_t desc = image_desc(cpu, g_images[p]);
  if (desc) set_images(jobj_by_index(cpu, H->rd8(DOORS + door * DOOR_SIZE + DOOR_COSTUME_JOINT)), desc, 0);
}

// The page arrow's picture: a chevron on the icons' dark ground, with a dot per page (the one
// shown filled), drawn as a 64x56 RGB5A3 image.
int arrow_image(int page, int pages) {
  Image img; img.w = ICON_W; img.h = ICON_H; img.data.resize(ICON_W * ICON_H * 2);
  for (int y = 0; y < ICON_H; ++y)
    for (int x = 0; x < ICON_W; ++x) {
      float fx = (float)x, fy = (float)y;
      int r = 20 + y / 3, g = 24 + y / 3, b = 40 + y / 2;             // the ground, lighter lower down
      // A right-pointing chevron: two thick strokes meeting at (40, 24).
      float t = std::fabs(fy - 24.0f), along = fx - (40.0f - t * 0.9f);
      if (fy < 44 && along > -7.0f && along < 1.5f && t < 16) { r = 250; g = 250; b = 250; }
      // Page dots along the bottom.
      for (int p = 0; p < pages; ++p) {
        float dx = fx - (32.0f + (p - (pages - 1) / 2.0f) * 8.0f), dy = fy - 49.0f;
        float d2 = dx * dx + dy * dy;
        if (d2 < 7.0f && (p == page || d2 > 3.0f)) { r = p == page ? 255 : 170; g = p == page ? 210 : 170; b = p == page ? 60 : 170; }
      }
      uint16_t v = (uint16_t)(0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3));
      int tile = (y / 4) * (ICON_W / 4) + x / 4, at = (tile * 16 + (y % 4) * 4 + x % 4) * 2;
      img.data[at] = (uint8_t)(v >> 8); img.data[at + 1] = (uint8_t)v;
    }
  g_images.push_back(std::move(img));
  return (int)g_images.size() - 1;
}

// ---- grid icons ----
// Each grid icon is a JObj with two textures: the shared frame (I4) and the fighter's 64x56
// paletted (CI8) image. A replaced slot's icon gets its RGB5A3 image in place of the CI8 one;
// a new fighter gets an icon of its own: a copy of its base fighter's icon joint (HSD_Joint,
// from the CSS archive) loaded as a new JObj where its region is. The game finds the menu's
// joints by their depth-first index (lb_80011E24), so new icons hang off the menu's root as its
// last children, after every joint the game knows, with their placement carried over into the
// root's space. All of it happens once per CSS scene, on the first cursor update.
constexpr uint32_t CSS_MODELS = 0x804D6CD8;     // MnSelectChrModels*: the CSS archive's models
constexpr uint32_t MODELS_MENU = 0x30;          // StaticModelDesc menu (the VS-mode CSS)
constexpr uint32_t JOINT_SIZE = 0x40, JOINT_FLAGS = 0x04, JOINT_CHILD = 0x08, JOINT_NEXT = 0x0C;
constexpr uint32_t JOINT_SCALE = 0x20, JOINT_POSITION = 0x2C;
constexpr uint32_t JOBJ_FLAGS = 0x14, JOBJ_CHILD = 0x10, JOBJ_NEXT = 0x08, JOBJ_PARENT = 0x0C;
constexpr uint32_t JOBJ_SCALE = 0x2C, JOBJ_POSITION = 0x38;
constexpr uint32_t JOBJ_INSTANCE = 0x1000, JOBJ_HIDDEN = 0x10;
constexpr uint32_t JOBJ_LOAD_JOINT = 0x80370E44;   // HSD_JObjLoadJoint(joint)
constexpr uint32_t JOBJ_ADD_CHILD = 0x803717A8;    // HSD_JObjAddChild(parent, child)
constexpr int CK_GAMEWATCH = 3, CK_MARTH = 9, CK_PIKACHU = 13;
uint64_t g_css_session = ~0ull;
uint32_t g_icon_jobj[ICON_COUNT];     // the game's icons this scene
std::vector<uint32_t> g_fighter_jobj; // per new fighter, 0 if it has none
uint32_t g_arrow_jobj = 0;
int g_page_shown = -1;
std::map<uint32_t, uint32_t> g_hidden;   // JObj -> its flags before the plugin hid it

// The joint lb_80011E24 numbers `target` (depth first: a joint, its children, then its next).
uint32_t joint_by_index(uint32_t j, int& cur, int target, int depth = 0) {
  for (; j && depth < 16; j = H->rd32(j + JOINT_NEXT)) {
    if (cur++ == target) return j;
    if (!(H->rd32(j + JOINT_FLAGS) & JOBJ_INSTANCE))
      if (uint32_t found = joint_by_index(H->rd32(j + JOINT_CHILD), cur, target, depth + 1)) return found;
  }
  return 0;
}

int icon_index(int ckind) {
  for (int i = 0; i < ICON_COUNT; ++i) if (H->rd8(CSS_ICONS + i * CSS_ICON_SIZE + 1) == ckind) return i;
  return -1;
}

// Hides (or shows again) a JObj and everything under it; drawing checks each JObj's own flag.
void set_hidden(uint32_t j, bool hide, int depth = 0) {
  if (!j || depth > 12) return;
  uint32_t flags = H->rd32(j + JOBJ_FLAGS);
  if (hide) {
    if (!g_hidden.count(j)) g_hidden[j] = flags;
    H->wr32(j + JOBJ_FLAGS, flags | JOBJ_HIDDEN);
  } else if (g_hidden.count(j)) {
    H->wr32(j + JOBJ_FLAGS, (flags & ~JOBJ_HIDDEN) | (g_hidden[j] & JOBJ_HIDDEN));
    g_hidden.erase(j);
  }
  for (uint32_t c = H->rd32(j + JOBJ_CHILD); c; c = H->rd32(c + JOBJ_NEXT)) set_hidden(c, hide, depth + 1);
}

// A copy of the icon joint of CSS icon `like`, loaded as a new JObj under the menu's root at
// the place of `at` (a JObj of the icon grid) moved `dx` along its row, showing image `img`.
uint32_t add_icon(pp_cpu* cpu, int like, uint32_t at, float dx, int img) {
  uint32_t models = H->rd32(CSS_MODELS), root = models ? H->rd32(models + MODELS_MENU) : 0;
  uint32_t menu = H->rd32(CSS_ROOT);
  int cur = 0;
  uint32_t base = root && at ? joint_by_index(root, cur, H->rd8(CSS_ICONS + like * CSS_ICON_SIZE + 4)) : 0;
  uint32_t copy = base ? heap_alloc(cpu, JOINT_SIZE) : 0;
  if (!copy) return 0;
  for (uint32_t o = 0; o < JOINT_SIZE; o += 4) H->wr32(copy + o, H->rd32(base + o));
  H->wr32(copy + JOINT_NEXT, 0);   // just this icon, not the joints after it
  H->wr32(copy + JOINT_FLAGS, H->rd32(copy + JOINT_FLAGS) & ~JOBJ_HIDDEN);
  // The grid JObj's placement, moved along the row, then through its parents to the root.
  float pos[3] = {H->rdf32(at + JOBJ_POSITION) + dx, H->rdf32(at + JOBJ_POSITION + 4), H->rdf32(at + JOBJ_POSITION + 8)};
  float scale[3] = {H->rdf32(at + JOBJ_SCALE), H->rdf32(at + JOBJ_SCALE + 4), H->rdf32(at + JOBJ_SCALE + 8)};
  for (uint32_t j = H->rd32(at + JOBJ_PARENT); j && j != menu; j = H->rd32(j + JOBJ_PARENT))
    for (uint32_t k = 0; k < 3; ++k) {
      pos[k] = H->rdf32(j + JOBJ_POSITION + k * 4) + H->rdf32(j + JOBJ_SCALE + k * 4) * pos[k];
      scale[k] *= H->rdf32(j + JOBJ_SCALE + k * 4);
    }
  for (uint32_t k = 0; k < 3; ++k) {
    H->wrf32(copy + JOINT_POSITION + k * 4, pos[k]);
    H->wrf32(copy + JOINT_SCALE + k * 4, scale[k]);
  }
  H->set_reg(cpu, 3, copy);
  H->call(cpu, JOBJ_LOAD_JOINT);
  uint32_t jobj = H->reg(cpu, 3);
  if (!jobj) return 0;
  H->set_reg(cpu, 3, menu); H->set_reg(cpu, 4, jobj);
  H->call(cpu, JOBJ_ADD_CHILD);
  if (img >= 0)
    if (uint32_t desc = image_desc(cpu, g_images[img])) set_images(jobj, desc, 0, GX_TF_CI8, true);
  return jobj;
}

void dress_css(pp_cpu* cpu) {
  uint64_t session = css_session();
  if (session == g_css_session || !H->rd32(CSS_ROOT)) return;
  g_css_session = session;
  g_page = 0; g_page_shown = -1; g_hidden.clear();
  g_fighter_jobj.assign(g_fighters.size(), 0); g_arrow_jobj = 0;
  bool one_p = H->rd8(CSS_1P) == 1;
  uint32_t joint_id = one_p ? 5 : 4;   // CSSIcon joint_id_1p / joint_id_vs
  for (int i = 0; i < ICON_COUNT; ++i) {
    uint32_t icon = CSS_ICONS + i * CSS_ICON_SIZE;
    g_icon_jobj[i] = jobj_by_index(cpu, H->rd8(icon + joint_id));
    uint8_t ckind = H->rd8(icon + 1);
    if (ckind >= PLAYABLE_CKINDS || g_slot_icon[ckind] < 0) continue;
    if (uint32_t desc = image_desc(cpu, g_images[g_slot_icon[ckind]])) set_images(g_icon_jobj[i], desc, 0, GX_TF_CI8, true);
  }
  if (one_p || g_fighters.empty()) return;
  // The bottom row: Pikachu is column 2, and Game & Watch -> Marth is one column step.
  int pika = icon_index(CK_PIKACHU), gw = icon_index(CK_GAMEWATCH), marth = icon_index(CK_MARTH);
  if (pika < 0 || gw < 0 || marth < 0 || !g_icon_jobj[pika] || !g_icon_jobj[gw] || !g_icon_jobj[marth] || !H->rd32(g_icon_jobj[pika] + JOBJ_PARENT)) {
    log("the icon grid is not as expected; new fighters get no icons");
    return;
  }
  float step = H->rdf32(g_icon_jobj[marth] + JOBJ_POSITION) - H->rdf32(g_icon_jobj[gw] + JOBJ_POSITION);
  for (size_t i = 0; i < g_fighters.size(); ++i) {
    const Fighter& f = g_fighters[i];
    if (f.proxy < 0) continue;
    uint32_t at = f.spot >= 0 ? g_icon_jobj[f.spot] : g_icon_jobj[pika];
    float dx = f.spot == SPOT_RIGHT_OF_ROY ? (COLUMN_RIGHT_OF_ROY - 2) * step : f.spot == SPOT_LEFT_OF_PICHU ? (COLUMN_LEFT_OF_PICHU - 2) * step : 0;
    g_fighter_jobj[i] = add_icon(cpu, f.proxy, at, dx, f.icon);
    if (!g_fighter_jobj[i]) log(f.id + ": cannot copy the base fighter's icon; it gets no icon");
  }
  if (g_arrow) g_arrow_jobj = add_icon(cpu, pika, g_icon_jobj[pika], (COLUMN_RIGHT_OF_ROY - 2) * step, -1);
}

// Shows the page's icons: the game's own on the first page, the new fighters' on theirs.
void show_page(pp_cpu* cpu) {
  if (!in_css_vs() || g_page == g_page_shown) {
    if (g_page > 0) for (uint32_t j : g_icon_jobj) set_hidden(j, true);   // keep them hidden
    return;
  }
  g_page_shown = g_page;
  for (uint32_t j : g_icon_jobj) set_hidden(j, g_page > 0);
  for (size_t i = 0; i < g_fighter_jobj.size(); ++i) set_hidden(g_fighter_jobj[i], g_fighters[i].page != g_page);
  if (g_arrow_jobj && g_page < (int)g_arrow_images.size())
    if (uint32_t desc = image_desc(cpu, g_images[g_arrow_images[g_page]])) {
      set_images(g_arrow_jobj, desc, 0, GX_TF_CI8, true);     // the copied icon's own picture
      set_images(g_arrow_jobj, desc, 0, GX_TF_RGB5A3, true);  // the previous page's arrow
    }
}

// Runs whenever a door's selection changes: records the pick, lends the kinds, and the door's
// name plate shows the new fighter's name while it is picked.
void door_update(pp_cpu* cpu, void*) {
  int door = (int)(H->reg(cpu, 3) & 0xFF);
  if (door < 4) {
    int c = in_css_vs() ? door_pick(door) : -1;
    float x = 0, y = 0;
    if (c >= 0 && token_at(door, x, y)) { g_pick_at[door][0] = x; g_pick_at[door][1] = y; }
    g_pick[door] = c;
    lend_kinds();
  }
  int c = door < 4 ? g_pick[door] : -1;
  uint32_t slot_us = 0, slot_jp = 0, us = 0, jp = 0;
  if (c >= 0 && g_fighters[c].name) {
    slot_us = NAMES_US + g_fighters[c].base_ckind * 4; slot_jp = NAMES_JP + g_fighters[c].base_ckind * 4;
    us = H->rd32(slot_us); jp = H->rd32(slot_jp);
    H->wr32(slot_us, g_fighters[c].name); H->wr32(slot_jp, g_fighters[c].name);
  }
  H->call(cpu, g_door_update);
  if (slot_us) { H->wr32(slot_us, us); H->wr32(slot_jp, jp); }
}

// A player that picked a new fighter is built from the kind it was lent: the base's CSS kind
// maps to that kind for the length of the call. New fighters come with one costume, so the
// player wears that one whatever color was picked.
void player_create(pp_cpu* cpu, void*) {
  uint32_t slot = H->reg(cpu, 3);
  int c = slot < 4 ? g_pick[slot] : -1;
  int kind = kind_of(c);
  if (kind < 0) c = -1;
  if (c >= 0) {
    H->set_reg(cpu, 4, 0);
    H->call(cpu, SET_COSTUME);
    H->set_reg(cpu, 3, slot);
  }
  uint32_t row = c >= 0 ? FT_MAPPING + g_fighters[c].base_ckind * 3 : 0;
  uint8_t was = row ? H->rd8(row) : 0;
  if (row) H->wr8(row, (uint8_t)kind);
  H->call(cpu, g_player_create);
  if (row) H->wr8(row, was);
}

// The scene preload queues each player's files by CSS kind; add the picked new fighters' files.
void preload_ckind(pp_cpu* cpu, void*) {
  int ckind = (int)H->reg(cpu, 3);
  H->call(cpu, g_preload_ckind);
  for (const Spare& s : g_spares) {
    if (s.holder < 0 || g_fighters[s.holder].base_ckind != ckind) continue;
    H->set_reg(cpu, 3, (uint32_t)s.kind); H->set_reg(cpu, 4, 0);   // its one costume
    H->call(cpu, FT_PRELOAD);
  }
}

// The stock icons above each player's damage (ifStock_802F8298, a GObj proc run every frame
// for each player's stock display): the game picks the icon by animating each stock's texture
// to the player's CSS kind and costume, which is the base fighter's for a new fighter. After
// it runs, a new fighter's (or replaced slot's) stocks show its own picture and lose the
// texture animation that would put the base's back. The display is rebuilt every match.
constexpr uint32_t IF_STOCK = 0x802F8298;
constexpr uint32_t STOCK_PLAYERS = 0x804A1378 + 0x08;   // ifStock_804A1378.player[6], 0x50 bytes each
constexpr uint32_t STOCK_PLAYER_SIZE = 0x50, STOCK_JOBJS = 0x04;   // x4[8]: [1..5] the stocks
constexpr uint32_t GOBJ_USER_DATA = 0x2C;
constexpr uint32_t PLAYER_CKIND = 0x80032330;           // Player_GetPlayerCharacter(slot)
constexpr uint16_t STOCK_W = 24, STOCK_H = 24;
uint32_t g_if_stock = 0;

// A stock picture lives as long as the game: one descriptor in memory the game never frees.
uint32_t stock_desc(Image& p) {
  if (p.desc) return p.desc;
  uint32_t bytes = (uint32_t)p.data.size(), block = H->guest_alloc(bytes + 64);
  if (!block) return 0;
  uint32_t desc = (block + 31) & ~31u, image = desc + 32;
  const uint8_t* b = p.data.data();
  for (uint32_t i = 0; i < bytes; i += 4)
    H->wr32(image + i, (uint32_t)b[i] << 24 | (uint32_t)b[i + 1] << 16 | (uint32_t)b[i + 2] << 8 | b[i + 3]);
  H->wr32(desc, image); H->wr16(desc + 4, p.w); H->wr16(desc + 6, p.h);
  H->wr32(desc + 8, GX_TF_RGB5A3); H->wr32(desc + 12, 0); H->wrf32(desc + 16, 0); H->wrf32(desc + 20, 0);
  p.desc = desc;
  return desc;
}

void if_stock(pp_cpu* cpu, void*) {
  uint32_t gobj = H->reg(cpu, 3);
  H->call(cpu, g_if_stock);
  uint32_t ud = H->rd32(gobj + GOBJ_USER_DATA);
  uint32_t player = ud ? H->rd8(ud) : 99;
  if (player >= 6) return;
  int img = -1;
  int c = player < 4 && kind_of(g_pick[player]) >= 0 ? g_pick[player] : -1;
  if (c >= 0) img = g_fighters[c].stock;
  else {
    H->set_reg(cpu, 3, player);
    H->call(cpu, PLAYER_CKIND);
    uint32_t ckind = H->reg(cpu, 3);
    if (ckind < PLAYABLE_CKINDS) img = g_slot_stock[ckind];
  }
  H->set_reg(cpu, 3, gobj);
  if (img < 0) return;
  uint32_t desc = stock_desc(g_images[img]);
  if (!desc) return;
  static bool told = false;
  for (int i = 1; i <= 5; ++i) {
    uint32_t j = H->rd32(STOCK_PLAYERS + player * STOCK_PLAYER_SIZE + STOCK_JOBJS + 4 * i);
    if (!j || (H->rd32(j + 0x14) & JOBJ_NO_DOBJ)) continue;
    uint32_t d = H->rd32(j + 0x18), m = d ? H->rd32(d + 8) : 0, t = m ? H->rd32(m + 8) : 0;
    if (!t) continue;
    if (!told) {
      told = true;
      uint32_t id = H->rd32(t + TOBJ_IMAGEDESC);
      char msg[128];
      std::snprintf(msg, sizeof msg, "stock icons: the game's are %ux%u (format %u); player %u shows its own",
                    id ? H->rd16(id + 4) : 0, id ? H->rd16(id + 6) : 0, id ? H->rd32(id + 8) : 0, player);
      log(msg);
    }
    H->wr32(t + TOBJ_IMAGEDESC, desc);
    H->wr32(t + TOBJ_AOBJ, 0);
  }
}

// The game's names are full-width Shift-JIS; ASCII letters, digits and spaces map directly
// (dashes and underscores become spaces).
uint32_t sjis_name(const std::string& ascii) {
  std::string out;
  for (unsigned char ch : ascii) {
    int code = 0;
    if (ch >= 'A' && ch <= 'Z') code = 0x8260 + (ch - 'A');
    else if (ch >= 'a' && ch <= 'z') code = 0x8281 + (ch - 'a');
    else if (ch >= '0' && ch <= '9') code = 0x824F + (ch - '0');
    if (code) { out += (char)(code >> 8); out += (char)(code & 0xFF); }
    else out += ch == ' ' || ch == '-' || ch == '_' ? ' ' : '?';
  }
  uint32_t a = H->guest_alloc((uint32_t)out.size() + 1);
  for (size_t i = 0; a && i <= out.size(); ++i) H->wr8(a + (uint32_t)i, i < out.size() ? (uint8_t)out[i] : 0);
  return a;
}

// A disc file name in guest memory (the game's loaders take a char*).
uint32_t guest_string(const std::string& s) {
  uint32_t a = H->guest_alloc((uint32_t)s.size() + 1);
  for (size_t i = 0; a && i <= s.size(); ++i) H->wr8(a + (uint32_t)i, i < s.size() ? (uint8_t)s[i] : 0);
  return a;
}

std::string g_dir;   // the config's folder: picture files are named relative to it

int image_of(const nlohmann::json& f, const char* key, uint16_t w, uint16_t h, const std::string& who) {
  std::string file = f.value(key, std::string());
  return file.empty() ? -1 : load_image(g_dir, file, w, h, who);
}

// A fighter slot replaced by a new character (the files on the disc already are the new
// character): its CSS name everywhere, and its pictures on the doors and the grid.
bool install_slot(const nlohmann::json& s) {
  int ckind = s.value("ckind", -1);
  std::string who = s.value("id", std::string("slot"));
  if (ckind < 0 || ckind >= PLAYABLE_CKINDS) { log(who + ": ckind must be a playable CSS kind (0-25)"); return false; }
  std::string name = s.value("name", std::string());
  if (!name.empty())
    if (uint32_t sj = sjis_name(name)) { H->wr32(NAMES_US + ckind * 4, sj); H->wr32(NAMES_JP + ckind * 4, sj); }
  g_slot_portrait[ckind] = image_of(s, "portrait", PORTRAIT_W, PORTRAIT_H, who);
  g_slot_icon[ckind] = image_of(s, "icon", ICON_W, ICON_H, who);
  g_slot_stock[ckind] = image_of(s, "stock", STOCK_W, STOCK_H, who);
  char msg[160];
  std::snprintf(msg, sizeof msg, "%s: CSS kind %d shows as %s%s%s", who.c_str(), ckind, name.empty() ? "(its own name)" : name.c_str(),
                g_slot_portrait[ckind] >= 0 ? " with its own portrait" : "", g_slot_icon[ckind] >= 0 ? " and icon" : "");
  log(msg);
  return true;
}

bool install(const nlohmann::json& j) {
  Fighter f;
  f.id = j.value("id", std::string("?"));
  f.base = j.value("base", -1);
  f.base_ckind = j.value("base_ckind", -1);
  if (f.base < 0 || f.base >= 0x1B || f.base_ckind < 0 || f.base_ckind >= PLAYABLE_CKINDS) {
    log(f.id + ": base must be a playable fighter kind and base_ckind its CSS kind");
    return false;
  }
  f.proxy = icon_index(f.base_ckind);
  if (f.proxy < 0) { log(f.id + ": its base fighter has no select screen icon"); return false; }
  std::string name = j.value("name", std::string());
  f.name = name.empty() ? 0 : sjis_name(name);
  for (auto [key, out] : {std::pair<const char*, uint32_t*>{"dat", &f.dat}, {"costume", &f.costume}, {"anim", &f.anim}}) {
    std::string file = j.value(key, std::string());
    if (!file.empty()) *out = guest_string(file);
  }
  f.portrait = image_of(j, "portrait", PORTRAIT_W, PORTRAIT_H, f.id);
  f.icon = image_of(j, "icon", ICON_W, ICON_H, f.id);
  f.stock = image_of(j, "stock", STOCK_W, STOCK_H, f.id);
  g_fighters.push_back(std::move(f));
  return true;
}

// Where each new fighter sits on the select screen, and how many pages there are.
void layout() {
  size_t n = g_fighters.size();
  g_arrow = n > 2;
  for (size_t i = 0; i < n; ++i) {
    Fighter& f = g_fighters[i];
    if (!g_arrow) { f.page = 0; f.spot = i == 0 ? SPOT_RIGHT_OF_ROY : SPOT_LEFT_OF_PICHU; }
    else if (i == 0) { f.page = 0; f.spot = SPOT_LEFT_OF_PICHU; }
    else { f.page = 1 + (int)(i - 1) / ICON_COUNT; f.spot = (int)(i - 1) % ICON_COUNT; }
  }
  g_pages = n ? g_fighters.back().page + 1 : 1;
  for (int i = 0; i < ICON_COUNT; ++i) {
    uint32_t b = CSS_ICONS + i * CSS_ICON_SIZE + ICON_BOUNDS;
    g_grid[i] = {H->rdf32(b), H->rdf32(b + 4), H->rdf32(b + 8), H->rdf32(b + 12)};
  }
  if (g_arrow) for (int p = 0; p < g_pages; ++p) g_arrow_images.push_back(arrow_image(p, g_pages));
}

}  // namespace

extern "C" __declspec(dllexport) int pp_plugin_load(const pp_host* host, const char* config_path) {
  if (!host || host->abi != PP_PLUGIN_ABI) return 1;
  H = host;
  if (!config_path) { log("no extra-fighters.json; nothing to add"); return 0; }
  nlohmann::json cfg;
  try { std::ifstream in(config_path); in >> cfg; } catch (const std::exception& e) { log(std::string("bad config: ") + e.what()); return 2; }
  g_dir = config_path;
  g_dir = g_dir.substr(0, g_dir.find_last_of("/\\") == std::string::npos ? 0 : g_dir.find_last_of("/\\"));
  if (g_dir.empty()) g_dir = ".";
  for (int& p : g_slot_portrait) p = -1;
  for (int& p : g_slot_icon) p = -1;
  for (int& p : g_slot_stock) p = -1;
  for (int i = 0; i < SPARES; ++i) {
    Spare& s = g_spares[i];
    s.kind = SPARE_KINDS[i];
    s.rows = save_rows(s.kind);
    uint32_t costume = H->rd32(COSTUME_STRINGS + s.kind * 4);
    for (uint32_t b = 0; b < 12; ++b) s.costume[b] = H->rd8(costume + b);
  }
  int n = 0, total = 0;
  for (const auto& f : cfg.value("fighters", nlohmann::json::array())) { ++total; n += install(f) ? 1 : 0; }
  for (const auto& s : cfg.value("slots", nlohmann::json::array())) install_slot(s);
  layout();
  bool slot_icons = false;
  for (int p : g_slot_icon) slot_icons |= p >= 0;
  if ((slot_icons || !g_fighters.empty()) && !(g_cursor_think = H->hook(CURSOR_THINK, cursor_think, nullptr)))
    log("cannot hook the character select screen; new fighters cannot be picked");
  if (!g_images.empty() && !(g_door_portrait = H->hook(DOOR_PORTRAIT, door_portrait, nullptr)))
    log("cannot hook the door portrait; characters show the base fighter's portrait");
  if (!g_fighters.empty()) {
    g_load_common = H->hook(LOAD_COMMON_DATA, load_common_data, nullptr);
    g_player_create = H->hook(PLAYER_CREATE, player_create, nullptr);
    g_preload_ckind = H->hook(PRELOAD_CKIND, preload_ckind, nullptr);
    if (!g_load_common || !g_player_create || !g_preload_ckind)
      log("cannot hook the match load; new fighters will not load");
    if (!(g_door_update = H->hook(DOOR_UPDATE, door_update, nullptr)))
      log("cannot hook the door name plate; new fighters cannot be picked");
  }
  bool stocks = false;
  for (int p : g_slot_stock) stocks |= p >= 0;
  for (const Fighter& f : g_fighters) stocks |= f.stock >= 0;
  if (stocks && !(g_if_stock = H->hook(IF_STOCK, if_stock, nullptr)))
    log("cannot hook the stock display; stocks show the base fighter");
  char msg[96];
  std::snprintf(msg, sizeof msg, "%d of %d fighters added (%d select screen page%s)", n, total, g_pages, g_pages == 1 ? "" : "s");
  log(msg);
  return 0;
}
