/* Melee game data for PascalPatch plugins (NTSC 1.02 addresses).
 *
 * Field offsets come from the doldecomp/melee project's struct definitions (ft/types.h,
 * lb/types.h) and the player-block layout the community documented for Slippi; the runtime
 * checks nothing for you, so read through the helpers below, which validate every pointer.
 *
 *   uint32_t fp = pp_fighter(host, 0);          // port 1's fighter, or 0
 *   if (fp) {
 *       unsigned state = host->rd32(fp + PP_FT_STATE);
 *       const char *name = pp_state_name(host->rd32(fp + PP_FT_KIND), state);
 *       float percent = host->rdf32(fp + PP_FT_PERCENT);
 *   }
 */
#ifndef PASCALPATCH_MELEE_H
#define PASCALPATCH_MELEE_H
#include "plugin.h"
#include "melee_states.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- where things are ---- */
#define PP_PLAYER_BLOCK   0x80453080u  /* one block per port (StaticPlayer) */
#define PP_PLAYER_STRIDE  0xE90u
#define PP_PAD_STATUS     0x804C1FAC  /* HSD_PadMasterStatus[4], what the game reads each frame */
#define PP_PAD_STRIDE     0x44u
#define PP_SCENE          0x80479D30u  /* major scene at +0, minor at +3 */
#define PP_SCENE_VS       2u           /* VS mode; minor 0 character select, 1 stage select, 2 in game */

/* player block */
#define PP_PL_CHARACTER   0x04u  /* u32 external character id (CSS order) */
#define PP_PL_TYPE        0x08u  /* u32: 0 human, 1 CPU, 2 demo, 3 none */
#define PP_PL_STOCKS      0x8Eu  /* s8 */
#define PP_PL_FIGHTER     0xB0u  /* Fighter_GObj* (0 when absent) */
#define PP_PL_FIGHTER2    0xB4u  /* the second fighter of the port: Nana */
#define PP_GOBJ_DATA      0x2Cu  /* HSD_GObj -> its Fighter* */

/* Fighter (the gobj's user data) */
#define PP_FT_GOBJ         0x000u  /* HSD_GObj* back pointer (validates the fighter) */
#define PP_FT_KIND         0x004u  /* u32 FighterKind (PP_KIND_*) */
#define PP_FT_PORT         0x00Cu  /* u8 player index 0..5 */
#define PP_FT_STATE        0x010u  /* u32 action state (motion state) id, see melee_states.h */
#define PP_FT_ANIM         0x014u  /* u32 animation id */
#define PP_FT_FACING       0x02Cu  /* f32: 1 right, -1 left */
#define PP_FT_SELF_VEL     0x080u  /* Vec3 */
#define PP_FT_KB_VEL       0x08Cu  /* Vec3 knockback velocity */
#define PP_FT_POS          0x0B0u  /* Vec3 position (world units) */
#define PP_FT_PREV_POS     0x0BCu  /* Vec3 */
#define PP_FT_AIRBORNE     0x0E0u  /* u32: 0 grounded, 1 airborne */
#define PP_FT_GROUND_VEL   0x0ECu  /* f32 */
#define PP_FT_MAX_JUMPS    0x168u  /* s32 (fighter attributes copied into the fighter) */
#define PP_FT_WEIGHT       0x198u  /* f32 */
#define PP_FT_STICK        0x620u  /* Vec2 control stick this frame (buffer of 3) */
#define PP_FT_CSTICK       0x638u  /* Vec2 C-stick this frame */
#define PP_FT_TRIGGER      0x650u  /* f32 analog trigger this frame */
#define PP_FT_HELD         0x65Cu  /* u32 buttons held this frame */
#define PP_FT_PRESSED      0x668u  /* u32 buttons pressed this frame (rising edge) */
#define PP_FT_RELEASED     0x66Cu  /* u32 buttons released this frame */
#define PP_FT_SINCE_LR     0x67Fu  /* u8 frames since L/R/Z was last pressed (the L-cancel timer) */
#define PP_FT_ANIM_FRAME   0x894u  /* f32 frame of the current animation */
#define PP_FT_ANIM_SPEED   0x89Cu  /* f32 animation rate (2 on an L-cancelled landing) */
#define PP_FT_HITBOXES     0x914u  /* HitCapsule[4], PP_HIT_STRIDE apart */
#define PP_FT_HURT_COUNT   0x119Eu /* u8 */
#define PP_FT_HURTBOXES    0x11A0u /* FighterHurtCapsule[15], 0x4C apart */
#define PP_FT_PERCENT      0x1830u /* f32 damage */
#define PP_FT_KB_ANGLE     0x1848u /* s32 */
#define PP_FT_KB_APPLIED   0x1850u /* f32 */
#define PP_FT_HIT_SOURCE   0x18C4u /* s32 port of whoever hit this fighter last */
#define PP_FT_SINCE_HIT    0x18ACu /* s32 frames since last hit */
#define PP_FT_HITLAG       0x195Cu /* f32 hitlag frames left */
#define PP_FT_JUMPS_USED   0x1968u /* u8 */
#define PP_FT_SHIELD       0x1998u /* f32 shield health (60 full) */
#define PP_FT_LEDGE_CD     0x2064u /* s32 ledge regrab cooldown */
#define PP_FT_FLAGS_2218   0x2218u /* u8 flags; 0x80 = allow_interrupt (the move's IASA frames have begun) */
#define PP_FT_ATTACK_ID    0x2068u /* s32 id of the move being done (for stale moves) */
#define PP_FT_HITSTUN      0x2340u /* f32 hitstun frames left, while in a Damage* state */
#define PP_FT_SHORT_HOP    0x2340u /* s32 in KneeBend: 1 when the jump will be a short hop */

/* HitCapsule (a hitbox) */
#define PP_HIT_STRIDE     0x138u
#define PP_HIT_STATE      0x00u  /* u32: 0 off, 1+ active */
#define PP_HIT_DAMAGE     0x0Cu  /* f32 */
#define PP_HIT_SIZE       0x1Cu  /* f32 radius */
#define PP_HIT_ANGLE      0x20u  /* s32 knockback angle */
#define PP_HIT_POS        0x4Cu  /* Vec3 world position this frame */
#define PP_HIT_PREV_POS   0x58u  /* Vec3 last frame (the hitbox sweeps between the two) */

/* buttons (PP_FT_HELD / PP_FT_PRESSED, and the pad status) */
enum {
    PP_BTN_DL = 0x1, PP_BTN_DR = 0x2, PP_BTN_DD = 0x4, PP_BTN_DU = 0x8, PP_BTN_Z = 0x10, PP_BTN_R = 0x20,
    PP_BTN_L = 0x40, PP_BTN_A = 0x100, PP_BTN_B = 0x200, PP_BTN_X = 0x400, PP_BTN_Y = 0x800, PP_BTN_START = 0x1000
};

/* FighterKind (PP_FT_KIND) */
enum {
    PP_KIND_MARIO, PP_KIND_FOX, PP_KIND_FALCON, PP_KIND_DK, PP_KIND_KIRBY, PP_KIND_BOWSER, PP_KIND_LINK,
    PP_KIND_SHEIK, PP_KIND_NESS, PP_KIND_PEACH, PP_KIND_POPO, PP_KIND_NANA, PP_KIND_PIKACHU, PP_KIND_SAMUS,
    PP_KIND_YOSHI, PP_KIND_JIGGLYPUFF, PP_KIND_MEWTWO, PP_KIND_LUIGI, PP_KIND_MARTH, PP_KIND_ZELDA,
    PP_KIND_YLINK, PP_KIND_DOC, PP_KIND_FALCO, PP_KIND_PICHU, PP_KIND_GAW, PP_KIND_GANON, PP_KIND_ROY,
    PP_KIND_MASTER_HAND, PP_KIND_CRAZY_HAND, PP_KIND_WIREFRAME_M, PP_KIND_WIREFRAME_F, PP_KIND_GIGA_BOWSER,
    PP_KIND_SANDBAG, PP_KIND_COUNT
};

static const char *const pp_kind_names[PP_KIND_COUNT] = {
    "Mario", "Fox", "Captain Falcon", "Donkey Kong", "Kirby", "Bowser", "Link", "Sheik", "Ness", "Peach",
    "Popo", "Nana", "Pikachu", "Samus", "Yoshi", "Jigglypuff", "Mewtwo", "Luigi", "Marth", "Zelda",
    "Young Link", "Dr. Mario", "Falco", "Pichu", "Mr. Game & Watch", "Ganondorf", "Roy", "Master Hand",
    "Crazy Hand", "Wireframe", "Wireframe", "Giga Bowser", "Sandbag"
};

/* Common action states worth naming in code (see melee_states.h for all of them). */
enum {
    PP_ST_DEAD_FIRST = 0x00, PP_ST_DEAD_LAST = 0x0A, PP_ST_REBIRTH = 0x0C, PP_ST_REBIRTH_WAIT = 0x0D,
    PP_ST_WAIT = 0x0E, PP_ST_WALK_SLOW = 0x0F, PP_ST_WALK_FAST = 0x11, PP_ST_TURN = 0x12, PP_ST_TURN_RUN = 0x13,
    PP_ST_DASH = 0x14, PP_ST_RUN = 0x15, PP_ST_RUN_BRAKE = 0x17, PP_ST_KNEE_BEND = 0x18,
    PP_ST_JUMP_F = 0x19, PP_ST_JUMP_AERIAL_B = 0x1C, PP_ST_FALL = 0x1D, PP_ST_FALL_AERIAL_B = 0x22,
    PP_ST_FALL_SPECIAL = 0x23, PP_ST_FALL_SPECIAL_B = 0x25,
    PP_ST_DAMAGE_FALL = 0x26, PP_ST_SQUAT = 0x27, PP_ST_SQUAT_WAIT = 0x28, PP_ST_SQUAT_RV = 0x29,
    PP_ST_LANDING = 0x2A, PP_ST_LANDING_FALL_SPECIAL = 0x2B,
    PP_ST_ATTACK_11 = 0x2C, PP_ST_ATTACK_AIR_N = 0x41, PP_ST_ATTACK_AIR_LW = 0x45,
    PP_ST_LANDING_AIR_N = 0x46, PP_ST_LANDING_AIR_LW = 0x4A,
    PP_ST_DAMAGE_FIRST = 0x4B, PP_ST_DAMAGE_LAST = 0x5B,
    PP_ST_GUARD_ON = 0xB2, PP_ST_GUARD = 0xB3, PP_ST_GUARD_OFF = 0xB4, PP_ST_GUARD_SET_OFF = 0xB5,
    PP_ST_GUARD_REFLECT = 0xB6,
    PP_ST_DOWN_BOUND_U = 0xB7, PP_ST_DOWN_BOUND_D = 0xBF,
    PP_ST_PASSIVE = 0xC7, PP_ST_PASSIVE_STAND_F = 0xC8, PP_ST_PASSIVE_STAND_B = 0xC9,
    PP_ST_PASSIVE_WALL = 0xCA, PP_ST_PASSIVE_WALL_JUMP = 0xCB, PP_ST_PASSIVE_CEIL = 0xCC,
    PP_ST_ESCAPE_F = 0xE9, PP_ST_ESCAPE_B = 0xEA, PP_ST_ESCAPE = 0xEB, PP_ST_ESCAPE_AIR = 0xEC,
    PP_ST_CATCH = 0xD4, PP_ST_CATCH_DASH_PULL = 0xD7, PP_ST_THROW_F = 0xDB, PP_ST_THROW_LW = 0xDE,
    PP_ST_THROWN_F = 0xEF, PP_ST_THROWN_LW_WOMEN = 0xF3,
    PP_ST_CLIFF_CATCH = 0xFC, PP_ST_CLIFF_WAIT = 0xFD
};

/* ---- helpers ---- */

static inline int pp_is_ptr(uint32_t a) { return a >= 0x80000000u && a < 0x81800000u; }

static inline uint32_t pp_player_block(int port) { return PP_PLAYER_BLOCK + PP_PLAYER_STRIDE * (uint32_t)port; }

/* 0 human, 1 CPU, 2 demo, 3 none */
static inline uint32_t pp_player_type(const pp_host *h, int port) { return h->rd32(pp_player_block(port) + PP_PL_TYPE); }

/* A port's fighter (sub = 1 for Nana), or 0 when there is none or the pointers do not add up. */
static inline uint32_t pp_fighter_ex(const pp_host *h, int port, int sub) {
    if (port < 0 || port > 3) return 0;
    uint32_t gobj = h->rd32(pp_player_block(port) + (sub ? PP_PL_FIGHTER2 : PP_PL_FIGHTER));
    if (!pp_is_ptr(gobj)) return 0;
    uint32_t fp = h->rd32(gobj + PP_GOBJ_DATA);
    if (!pp_is_ptr(fp) || h->rd32(fp + PP_FT_GOBJ) != gobj) return 0;
    return fp;
}
static inline uint32_t pp_fighter(const pp_host *h, int port) { return pp_fighter_ex(h, port, 0); }

/* The scene controller: major scene is the mode (1 the menus, 2 VS, 0x1C training, ...), minor
 * the stage within it. */
static inline uint32_t pp_scene_major(const pp_host *h) { return h->rd8(PP_SCENE); }
static inline uint32_t pp_scene_minor(const pp_host *h) { return h->rd8(PP_SCENE + 3); }

/* True while a match is on screen, in VS or any other mode that plays one (the port's own rule:
 * in those modes minor scenes 0 and 1 are the character and stage selects, 2 and up the match),
 * and some port has a fighter. Fighter pointers outlive the match (they still read as valid on
 * the menus after it, and during the title demo), so never go by pp_fighter alone. */
static inline int pp_in_match(const pp_host *h) {
    uint32_t major = pp_scene_major(h), minor = pp_scene_minor(h);
    int mode = major == 0x02 || major == 0x03 || major == 0x04 || major == 0x05 || major == 0x0F ||
               (major >= 0x10 && major <= 0x13) || major == 0x1B || major == 0x1C;
    if (major == 0x08) mode = minor == 2; /* Slippi online, if ever */
    if (!mode || minor < 2) return 0;
    /* VS and Training: 2 is the game itself; a quit (L+R+A+Start) or the results screen come after */
    if ((major == 0x02 || major == 0x1C) && minor != 2) return 0;
    for (int i = 0; i < 4; ++i) if (pp_fighter(h, i)) return 1;
    return 0;
}

/* Frames of hitlag (freeze on hit) left, and hitstun left while in a Damage state. */
static inline float pp_hitlag(const pp_host *h, uint32_t fp) { return h->rdf32(fp + PP_FT_HITLAG); }

static inline const char *pp_kind_name(uint32_t kind) { return kind < PP_KIND_COUNT ? pp_kind_names[kind] : "?"; }

static inline int pp_state_is_dead(uint32_t s) { return s <= PP_ST_DEAD_LAST; }
static inline int pp_state_is_damage(uint32_t s) { return s >= PP_ST_DAMAGE_FIRST && s <= PP_ST_DAMAGE_LAST; }
static inline int pp_state_is_shield(uint32_t s) { return s >= PP_ST_GUARD_ON && s <= PP_ST_GUARD_REFLECT; }
static inline int pp_state_is_aerial_landing(uint32_t s) { return s >= PP_ST_LANDING_AIR_N && s <= PP_ST_LANDING_AIR_LW; }

/* Held in a grab (being pulled, held or pummelled), or being thrown by any throw, command grabs
 * included. Grab releases and grab escapes do not count. */
static inline int pp_state_is_grabbed(uint32_t s) {
    if (s >= PP_STATE_COMMON || s == 0xE5 || s == 0xE6) return 0; /* CaptureCut, CaptureJump: let go */
    const char *n = pp_common_states[s];
    return (n[0] == 'C' && n[1] == 'a' && n[2] == 'p' && n[3] == 't') ||
           (n[0] == 'T' && n[1] == 'h' && n[2] == 'r' && n[3] == 'o' && n[4] == 'w' && n[5] == 'n');
}

/* Knocked down on the ground (missed tech, lying, getting up) or teching. */
static inline int pp_state_is_down(uint32_t s) { return s >= PP_ST_DOWN_BOUND_U && s <= PP_ST_PASSIVE_CEIL; }

/* The fighter is being punished: hit, tumbling, grabbed, thrown, knocked down, teching or dying.
 * Combo counters and "openings" statistics are built on this test. */
static inline int pp_state_is_punished(uint32_t s) {
    return pp_state_is_dead(s) || pp_state_is_damage(s) || s == PP_ST_DAMAGE_FALL || pp_state_is_grabbed(s) ||
           pp_state_is_down(s);
}

/* States the player can act out of right away (standing, moving, falling, crouching). A good
 * stand-in for "actionable" when measuring frame advantage; it misses IASA frames of attacks. */
static inline int pp_state_is_actionable(uint32_t s) {
    return (s >= PP_ST_WAIT && s <= PP_ST_RUN_BRAKE) || (s >= PP_ST_JUMP_F && s <= PP_ST_FALL_AERIAL_B) ||
           s == PP_ST_SQUAT_WAIT || s == PP_ST_SQUAT_RV || s == PP_ST_SQUAT || s == PP_ST_GUARD ||
           s == PP_ST_CLIFF_WAIT;
}

/* The move's IASA frames have begun: the player may already act out of it. The flag behind this
 * (set by the move script's "allow interrupt" command) is only cleared when a new script sets it
 * again, so it is left over in states that never read it, such as shield stun (GuardSetOff) and
 * landings. Only attacks, grabs, throws and specials (character states, 0x155 on) count here. */
static inline int pp_can_interrupt(const pp_host *h, uint32_t fp) {
    uint32_t s = h->rd32(fp + PP_FT_STATE);
    int move = (s >= PP_ST_ATTACK_11 && s <= PP_ST_ATTACK_AIR_LW) || (s >= PP_ST_CATCH && s <= PP_ST_THROW_LW) || s >= 0x155;
    return move && (h->rd8(fp + PP_FT_FLAGS_2218) & 0x80) != 0;
}

/* The name players use for a common move ("Nair", "Fsmash", "Up throw"), or the action state's own
 * name for everything else (specials: "SpecialN", ...). */
static inline const char *pp_move_name(uint32_t kind, uint32_t s) {
    static const char *const attacks[] = {
        "Jab", "Jab", "Jab", "Rapid jab", "Rapid jab", "Rapid jab", "Dash attack", "Ftilt", "Ftilt", "Ftilt",
        "Ftilt", "Ftilt", "Utilt", "Dtilt", "Fsmash", "Fsmash", "Fsmash", "Fsmash", "Fsmash", "Usmash", "Dsmash",
        "Nair", "Fair", "Bair", "Uair", "Dair", "Nair", "Fair", "Bair", "Uair", "Dair"};
    static const char *const throws[] = {"Forward throw", "Back throw", "Up throw", "Down throw"};
    if (s >= PP_ST_ATTACK_11 && s <= PP_ST_LANDING_AIR_LW) return attacks[s - PP_ST_ATTACK_11];
    if (s >= PP_ST_THROW_F && s <= PP_ST_THROW_LW) return throws[s - PP_ST_THROW_F];
    if (s >= PP_ST_CATCH && s <= PP_ST_CATCH_DASH_PULL) return "Grab";
    if (s == 0xD9) return "Pummel";
    if (s == 0x100 || s == 0x101) return "Ledge attack";
    if (s == 0xBB || s == 0xC3) return "Getup attack";
    const char *n = pp_state_name(kind, s);
    return n ? n : "?";
}

/* Pascal UI port colours, 0xRRGGBB (P1 red, P2 blue, P3 yellow, P4 green). */
static const uint32_t pp_port_rgb[4] = {0xE5322D, 0x2E6BF0, 0xF2C200, 0x2DB84D};
static inline uint32_t pp_rgba(uint32_t rgb, float alpha) {
    if (alpha < 0) alpha = 0;
    if (alpha > 1) alpha = 1;
    return (rgb << 8) | (uint32_t)(alpha * 255.0f + 0.5f);
}

#ifdef __cplusplus
}
#endif
#endif
