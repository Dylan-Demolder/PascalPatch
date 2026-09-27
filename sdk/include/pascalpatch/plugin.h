/* PascalPatch native plugin ABI (melee-unlocked runtime).
 *
 * A native plugin is a DLL that runs inside the game process, the way BakkesMod
 * plugins run inside Rocket League or Openplanet plugins inside Trackmania. The
 * port is never modified: pascalpatch-launch.exe starts an unmodified
 * melee_port.exe suspended and injects pascalpatch_runtime.dll, which finds the
 * port's dispatch table in memory, loads every plugin in the `--mods <dir>`
 * folder given to the launcher, and talks to them through the table below.
 *
 * The game's PowerPC code is translated ahead of time, so a plugin never
 * patches instructions. It changes behaviour three ways:
 *   - reading and writing guest memory (game data, tables, fighter structs);
 *   - calling guest functions by address;
 *   - standing in front of a guest function: `hook` replaces what an indirect
 *     call (function pointer, callback table, bctrl) reaches, and
 *     `trampoline` gives a host function a guest address that can be stored in
 *     any game table or callback field.
 * Direct `bl` calls inside the translated code do not go through the hook
 * table, so hooks see every call made through a pointer, not every call.
 *
 * Every callback runs on the simulation thread, inside the frame, and must not
 * block. Plugins are offline-only: the runtime refuses every network request
 * the port makes, and the launcher's --sandbox hides the Slippi Launcher login.
 *
 * Addresses are NTSC 1.02 (the port refuses any other DOL).
 */
#ifndef PASCALPATCH_PLUGIN_H
#define PASCALPATCH_PLUGIN_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PP_PLUGIN_ABI 1u

typedef struct pp_cpu pp_cpu; /* the guest CPU state of the call in progress */

/* A host function reachable from the guest. `cpu` holds the guest arguments
 * (r3.., f1..) and receives the results (r3, f1). */
typedef void (*pp_guest_fn)(pp_cpu *cpu, void *user);
typedef void (*pp_frame_fn)(void *user);

typedef struct pp_host {
    uint32_t abi;  /* PP_PLUGIN_ABI */
    uint32_t size; /* sizeof(pp_host) as built by the port */

    void (*log)(const char *plugin, const char *message);

    uint8_t (*rd8)(uint32_t addr);
    uint16_t (*rd16)(uint32_t addr);
    uint32_t (*rd32)(uint32_t addr);
    float (*rdf32)(uint32_t addr);
    void (*wr8)(uint32_t addr, uint8_t v);
    void (*wr16)(uint32_t addr, uint16_t v);
    void (*wr32)(uint32_t addr, uint32_t v);
    void (*wrf32)(uint32_t addr, float v);

    uint32_t (*reg)(pp_cpu *cpu, int n); /* r0..r31 */
    void (*set_reg)(pp_cpu *cpu, int n, uint32_t v);
    double (*freg)(pp_cpu *cpu, int n); /* f0..f31 (ps0) */
    void (*set_freg)(pp_cpu *cpu, int n, double v);

    /* Calls the guest function at `addr` with the registers as they are in
     * `cpu`. The call's results are left in `cpu`. */
    void (*call)(pp_cpu *cpu, uint32_t addr);

    /* A guest address that runs `fn` when called through a pointer. Returns 0
     * when the pool is exhausted. */
    uint32_t (*trampoline)(pp_guest_fn fn, void *user);

    /* Puts `fn` in front of the guest function at `addr`. Returns a guest
     * address that still runs the original, or 0 on failure. */
    uint32_t (*hook)(uint32_t addr, pp_guest_fn fn, void *user);

    /* Runs `fn` once per game frame, at the VI retrace (vertical blank). */
    void (*on_frame)(pp_frame_fn fn, void *user);

    /* Guest memory for plugin data the game reads (tables, attribute blocks).
     * 32-byte aligned, zeroed, never freed; 0 when the arena is exhausted.
     * The arena is small (a few KB), so keep game data in the game's files
     * where possible. */
    uint32_t (*guest_alloc)(uint32_t size);

    /* ---- Since PascalPatch 0.2. An older runtime serves a shorter table:
     * check PP_HOST_HAS(host, field) before calling any of these. ---- */

    /* Settings. A plugin's settings come from its plugin.json (downloaded
     * plugins) or from declare_setting; the F2 overlay shows them on the
     * plugin's own tab and PascalPatch saves them between runs. `spec_json`
     * uses the plugin.json form, e.g.
     *   {"key":"opacity","type":"float","label":"Opacity","default":0.8,"min":0,"max":1}
     * with type bool, int, float, choice ("options":[...]) or text. Reads are
     * cheap and may be made every frame; a change in the overlay shows up on
     * the next read. */
    void (*declare_setting)(const char *plugin, const char *spec_json);
    /* bool (0/1), int, float; a choice gives the index of its value. */
    double (*setting_number)(const char *plugin, const char *key);
    /* text and choice values. The string stays valid until the plugin's next
     * setting_text call. */
    const char *(*setting_text)(const char *plugin, const char *key);

    /* One line shown at the top of the plugin's F2 tab ("10 fighters added"). */
    void (*set_status)(const char *plugin, const char *text);

    /* HUD drawing, from an on_frame callback: what a frame's callbacks draw is
     * shown until the next frame's. Coordinates are in the game's 640 x 480
     * screen whatever the window size; colours are 0xRRGGBBAA. `size` is the
     * text height in those units (Melee's damage digits are about 32). */
    void (*hud_text)(float x, float y, uint32_t rgba, float size, const char *text);
    void (*hud_rect)(float x0, float y0, float x1, float y1, uint32_t rgba, float rounding, int filled);
    void (*hud_circle)(float x, float y, float radius, uint32_t rgba, int filled);
} pp_host;

/* True when `host` serves `field` (the runtime may be older than this header). */
#define PP_HOST_HAS(host, field)     ((host)->size >= offsetof(pp_host, field) + sizeof(((pp_host *)0)->field))

/* Exported by every plugin. `config_path` is `<plugin>.json` beside the DLL,
 * or NULL when there is none. Return 0 on success; anything else unloads the
 * plugin. */
typedef int (*pp_plugin_load_fn)(const pp_host *host, const char *config_path);
#define PP_PLUGIN_LOAD_SYMBOL "pp_plugin_load"

#ifdef __cplusplus
}
#endif
#endif
