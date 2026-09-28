# Reading the game

[`melee.h`](../../sdk/include/pascalpatch/melee.h) names the data plugins use most. Its offsets come from the [doldecomp/melee](https://github.com/doldecomp/melee) struct definitions and were checked against real matches. [`melee_states.h`](../../sdk/include/pascalpatch/melee_states.h) has the name of every action state, generated from the decomp.

## The pattern

```cpp
#include "pascalpatch/melee.h"

void frame(void*) {
  if (!pp_in_match(H)) return;                 // 1. is a match on?
  for (int port = 0; port < 4; ++port) {
    uint32_t fp = pp_fighter(H, port);         // 2. this port's fighter, validated, or 0
    if (!fp) continue;
    uint32_t kind  = H->rd32(fp + PP_FT_KIND);           // 3. read what you need
    uint32_t state = H->rd32(fp + PP_FT_STATE);
    float percent  = H->rdf32(fp + PP_FT_PERCENT);
    // 4. decide, then draw
  }
}
```

**Always check `pp_in_match` first.** Fighter pointers outlive the match: after a game ends they still look valid on the menus, and the title screen's demo has real fighters. `pp_in_match` knows which scenes actually hold a match. Those are VS, Training, Quick Match, the single-player modes and the event matches.

## Players and fighters

There are two structures:

- **The player block** (one per port, at `pp_player_block(port)`). It lasts all game and holds the chosen character (`PP_PL_CHARACTER`, in character-select order), whether the port is human or CPU (`pp_player_type`: 0 human, 1 CPU, 3 none), stocks (`PP_PL_STOCKS`), and a pointer to the fighter.
- **The fighter** (`pp_fighter(h, port)`). It exists during a match. For Ice Climbers, `pp_fighter_ex(h, port, 1)` gives Nana.

Useful fighter fields:

| Field | Type | What |
|---|---|---|
| `PP_FT_KIND` | u32 | who it is: `PP_KIND_FOX`, ... (`pp_kind_name`) |
| `PP_FT_STATE` | u32 | action state: what it is doing (`pp_state_name`, `pp_move_name`) |
| `PP_FT_ANIM_FRAME` | f32 | frame of the current animation, from 0 |
| `PP_FT_POS` | Vec3 | position in world units (x right, y up) |
| `PP_FT_SELF_VEL`, `PP_FT_KB_VEL` | Vec3 | its own velocity, and knockback velocity |
| `PP_FT_FACING` | f32 | 1 right, -1 left |
| `PP_FT_AIRBORNE` | u32 | 0 grounded, 1 airborne |
| `PP_FT_PERCENT` | f32 | damage |
| `PP_FT_SHIELD` | f32 | shield health, 60 when full |
| `PP_FT_HITLAG`, `PP_FT_HITSTUN` | f32 | frames of hitlag and hitstun left |
| `PP_FT_JUMPS_USED` | u8 | jumps used since landing |
| `PP_FT_HELD`, `PP_FT_PRESSED` | u32 | buttons this frame (`PP_BTN_*`), as the fighter sees them |
| `PP_FT_STICK`, `PP_FT_CSTICK`, `PP_FT_TRIGGER` | Vec2, Vec2, f32 | sticks and trigger this frame |
| `PP_FT_SINCE_LR` | u8 | frames since L/R/Z was pressed: the L-cancel timer |
| `PP_FT_HITBOXES` | 4 × HitCapsule | active hitboxes (`PP_HIT_*`) |
| `PP_FT_HIT_SOURCE` | s32 | port of whoever hit it last |

A Vec3 is three floats: `rdf32(fp + PP_FT_POS)`, `+ 4`, `+ 8`.

## Action states

The action state is the most useful number in the game. Every fighter is always in exactly one state: `Wait`, `Dash`, `KneeBend` (jump squat), `AttackAirN` (nair), `DamageFlyHi`, `CliffWait`, `EscapeAir` (airdodge) and so on. States 0 to 340 are shared by every fighter. States from `PP_STATE_COMMON` (341) upward are each fighter's own (specials, and some character-specific moves).

The helpers answer the questions that come up most:

| Helper | True when |
|---|---|
| `pp_state_is_actionable(s)` | standing, moving, falling or crouching: the player can act |
| `pp_can_interrupt(h, fp)` | a move's IASA frames have begun |
| `pp_state_is_damage(s)` | in hitstun |
| `pp_state_is_shield(s)` | shielding |
| `pp_state_is_grabbed(s)` | held or thrown |
| `pp_state_is_down(s)` | knocked down, getting up, or teching |
| `pp_state_is_punished(s)` | any of hit, tumble, grabbed, down, dying: the test combo counters use |
| `pp_state_is_dead(s)` | in a death state |
| `pp_move_name(kind, s)` | "Nair", "Fsmash", "Up throw", or the state's own name |

To find a state you do not know, show `pp_state_name(kind, state)` on screen while you do the move (the template does exactly this), or log it on every change.

## Scenes

`pp_scene_major(h)` is the mode and `pp_scene_minor(h)` is the step within it:

| Major | Mode |
|---|---|
| 0x01 | the main menus |
| 0x02 | VS: minor 0 character select, 1 stage select, 2 the match, then results |
| 0x0E | Quick Match (the game's debug VS): minor 1 the match, 3 results |
| 0x1C | Training mode: minor 2 the match |
| 0x18 | the title screen and its demo |

Use `pp_in_match` rather than testing these yourself.

To act once when a match starts or ends, watch `pp_in_match` change from one frame to the next:

```cpp
bool was = false;
void frame(void*) {
  bool now = pp_in_match(H);
  if (now && !was) on_match_start();
  if (!now && was) on_match_end();
  was = now;
}
```

## Traps

- **Stale flags.** Game fields are often left over from an earlier state. The interrupt flag behind `pp_can_interrupt` stays set through shield stun and landings. `PP_FT_HITSTUN` shares its address with the short-hop flag, so it only means hitstun during a Damage state. Before trusting a field, log it every frame across the moment you care about.
- **The frame order.** `on_frame` runs at the vertical blank, after the game has processed the frame. What you read is the result of this frame's inputs. What you write is seen by the next frame.
- **Frame counting.** `PP_FT_ANIM_FRAME` starts at 0, so frame data sites number it from 1. Several moves also play their animation at a rate other than 1 (`PP_FT_ANIM_SPEED`). To count frames spent in a state, count your own `on_frame` calls since the state changed.
- **Nana, Zelda and Sheik.** Ice Climbers have a second fighter. Zelda and Sheik transform: the port's fighter kind changes mid-match.
- **CPU ports read their AI, not the controller.** Buttons in `PP_FT_HELD` are what the fighter acts on, whether it is human or CPU.
- **Ports 4 to 6.** The helpers cover ports 1 to 4 (0 to 3). Event matches can use more.

## Beyond melee.h

The decomp has every struct in the game. When you need a field `melee.h` does not have:

1. Find it in [doldecomp/melee](https://github.com/doldecomp/melee): `src/melee/ft/types.h` for the fighter, `src/melee/lb/types.h` for hit capsules, `src/melee/gm/types.h` for game modes, and `src/melee/gr/` for stages.
2. Its offset is in the struct's comments, or work it out from the layout.
3. Define it in your plugin with a comment naming the struct and field, as the first-party plugins do. The hitbox viewer, for example, reads the camera with `GAME_CAMERA = 0x80452C68 // Camera game_camera`.
4. Check it in game by logging it before you build on it.

If the field is generally useful, send a pull request adding it to `melee.h`.
