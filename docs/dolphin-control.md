# Controlling Dolphin for character/roster smoke testing

This is the operator guide for booting a modified (custom-character) ISO
alongside the clean retail ISO, driving Dolphin headless, and confirming
that both old (stock retail) and new (custom-injected) characters actually
instantiate as live `Fighter` objects in the same run — not just that the
ISO boots.

It ties together three things that already exist in this repo but are not
otherwise documented in one place:

- `tooling/dolphin_smoke.py` — bounded clean-vs-modified **boot** smoke test
  (process-level: did Dolphin start, run to timeout, not crash).
- `host/src/meleemod/dolphin_gdb.py` + `tooling/verify_fighters.py` — live
  **memory-level** verification that specific characters (by `FighterKind`)
  actually exist as `Fighter` instances in emulated RAM, with real weight
  values. Read-only, never writes game memory.
- `.github/workflows/integration-smoke.yml` — how the self-hosted runner
  wires these together in CI (PAS-99 gate).

## 1. Rules that keep Dolphin controllable in a script/CI

These are hard lessons from this repo's own incident history — follow them
or Dolphin will hang or crash under automation.

- **Never send SIGTERM to Dolphin in batch mode.** It pops a "Confirm Stop"
  modal dialog instead of exiting, and the process then hangs forever
  waiting for a click that will never come. Always kill the whole process
  group with SIGKILL: `os.killpg(pid, signal.SIGKILL)` (Python) or
  `timeout -s KILL <n> ...` / `kill -9 <pid>` (shell). `dolphin_smoke.py`
  and `integration-smoke.yml` both do this — copy that pattern, don't
  reinvent it.
- **Always `start_new_session=True` (Python) / a dedicated process group**
  so you can kill children (audio/render helper threads) along with the
  main process.
- **Set a hard wall-clock timeout on every launch**, in addition to any
  internal polling deadline. A modal dialog, a stuck GDB stub, or an idle
  attract-mode loop can all hang indefinitely otherwise.
- **On this host, under the runner's xcb/Qt env vars** (`QT_QPA_PLATFORM=xcb`,
  `QT_XCB_NO_XI2=1`, `QT_WAYLAND_RECONNECT=1`, set for xdotool in the
  record job), Dolphin **movie (`-m`/`--movie`) playback stalls or
  SIGILLs**. Unset those three vars for any smoke/verification subprocess
  (see `dolphin_smoke.py`'s `dolphin_env` handling) — batch boot without a
  movie is unaffected, but movie playback specifically breaks under xcb.
  Native Wayland playback and `dolphin-emu-nogui` (never pass `-b` to the
  nogui binary — it rejects that flag) both work.
- **Inside the Paperclip Docker container** there is no display and no
  `/dev/dri`; Dolphin needs `QT_QPA_PLATFORM=offscreen` or it crashes with
  "could not connect to display" / "Could not load the Qt platform plugin
  xcb". This is now baked into the container's environment
  (`~/paperclip/.paperclip/up.sh`) — do not remove it, and if you ever
  `docker run` a fresh Dolphin/Paperclip container by hand, set it again.

## 1.5. The "stuck at the memory card screen" trap — and the real fix

A fresh/empty GameCube memory card triggers a **two-stage, mostly
button-gated** prompt on Melee's first boot with any given user-dir:

1. A GameCube-BIOS-level "The Memory Card in Slot A has no saved Game Data.
   Create Game Data?" Yes/No dialog. **This needs a real confirm keypress —
   it does not clear on its own, and it does NOT respond to DTM movie
   input** (confirmed by direct testing: a DTM authored to mash the A
   button continuously for 90 real-time seconds never cleared it; Dolphin's
   movie-input hook attaches at the game's own SI polling layer, which
   starts after this BIOS-level prompt, not before it).
2. After confirming, a second, purely-timed "Game Data has been created."
   banner (~15-25s, no input needed) plays before the intro FMV/title
   screen.

Every CI/headless probe does `rm -rf "$user_dir"` before each launch, so
this two-stage prompt reappears on **every single run** — this is the
actual reason runs report being stuck "at the memory card screen" and never
reach character select or a match, independent of any of the batch-vs-GUI,
audio-backend, or xcb/Qt issues documented elsewhere in this file.

**The fix is not to solve this every run.** `tooling/prime_memcard.py` does
a one-time, per-host, interactive priming pass:

```sh
DISPLAY=:0 PYTHONPATH=host/src python tooling/prime_memcard.py \
  --dolphin /usr/bin/dolphin-emu \
  --iso "/path/to/Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso" \
  --output ~/.cache/meleemod/memcard-seed
```

It requires a real display (must be run at the desktop, not headless/
off-screen — the dialog cannot be cleared any other way). It finds
Dolphin's actual render window via `xdotool search`/`getwindowname`
(several other Dolphin-owned windows exist and do not receive input — only
the one whose title contains `|`, e.g. `Dolphin 2606 | JIT64 SC | OpenGL |
HLE | Super Smash Bros. Melee (GALE01)`), then the load-bearing sequence is:

```sh
xdotool windowactivate --sync "$WINDOW_ID"   # REQUIRED before key — a bare
                                              # `xdotool key --window <id> x`
                                              # with no activation was
                                              # observed to NOT deliver input
xdotool key --clearmodifiers x               # GCPad1 Buttons/A default
# repeat 2 more times, 1s apart -- a single press was unreliable (~1 in 3
# attempts registered nothing) in testing on this host
```

...then it waits out the timed banner and harvests the resulting
`GC/<region>/Card A/*.gci` into `--output`.

**Legal boundary:** the harvested `.gci` contains real Melee-derived save
data (the game bakes icon/banner graphics into it). Per `docs/legal-notice.md`
it must **never** be committed to this repository or distributed —
`prime_memcard.py` refuses to write under any path containing a `.git`
folder as a guard rail. Cache it somewhere like `~/.cache/meleemod/
memcard-seed` outside any repo, once per host.

Every subsequent run — `dolphin_smoke.py` via `--memcard-seed <dir>`, or the
CI workflow via the `MEMCARD_SEED` env var (defaults to
`~/.cache/meleemod/memcard-seed`) — copies that cached `GC/` tree into the
fresh `user_dir` **before** boot, so the whole two-stage prompt never
appears on any headless/CI run again.

## 2. Boot-level smoke: does clean AND modified both start?

```sh
PYTHONPATH=host/src python tooling/dolphin_smoke.py \
  --dolphin /usr/bin/dolphin-emu \
  --clean "/path/to/Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso" \
  --modified /path/to/generated-modified.iso \
  --movie tooling/fixtures/pas44-menu-to-match.dtm \
  --memcard-seed ~/.cache/meleemod/memcard-seed \
  --log-dir /tmp/meleemod-smoke-logs \
  --user-dir /tmp/meleemod-smoke-user \
  --timeout 30
```

`--memcard-seed` (see section 1.5) is what actually gets you past the first-boot
memory-card prompt at all — without it, every run hits the un-clearable
"Create Game Data?" dialog and never reaches anything else.

This runs the clean ISO, then the modified ISO, each in its own isolated
Dolphin user directory, and reports `started`/`exit_code`/`timed_out`/a
tail of Dolphin's own log for each. `--expected-archive`/`--expected-data`/
`--expected-symbol`/`--expected-observation`/`--expected-character-select`/
`--expected-match-start`/`--expected-input-automation-ready` turn on
marker-string assertions against Dolphin's log output when your build
prints them (see `custom_fighter_assertion` in the script and the CI job's
usage for the full marker set MeleeMod itself expects at the PAS-99 gate).

This tells you the emulator ran without crashing. It does **not** tell you
a character actually loaded correctly in memory — for that, go live with
GDB (next section).

## 3. Memory-level verification: is the character actually a live Fighter?

This is the direct answer to "test that the new AND old characters are
working" — attach to the running emulator over its GDB stub and read the
real in-game fighter list, the same way `docs/evidence/t10-memory-read-result.md`
proved a patched Fox weight really reached a live `Fighter` instance.

### Step 1 — boot Dolphin with the GDB stub enabled

```sh
timeout -s KILL 560 dolphin-emu -b --batch -e /path/to/modified.iso \
  -C Dolphin.General.GDBPort=24689 \
  -C Dolphin.Display.RenderToMain=False \
  > /tmp/dolphin-gdb-boot.log 2>&1 &
DOLPHIN_PID=$!
```

Dolphin's GDB stub halts the emulated CPU the moment a debugger attaches,
so nothing runs until your client sends a `continue`.

### Step 2 — poll the live fighter list

```sh
PYTHONPATH=host/src python tooling/verify_fighters.py \
  --port 24689 --duration 180 --interval 8 \
  --expect-kind 1 --expect-kind 20
```

- `--expect-kind` is repeatable: pass the retail `FighterKind` you expect to
  see (e.g. `1` = Fox) **and** the custom/new character's kind (whatever
  slot/kind id your composed profile assigns it — check your profile's
  fighter-table composition step for the actual value). The script exits
  non-zero if any requested kind never shows up within `--duration`.
- It prints every fighter it finds each poll: `kind`, decoded name (for the
  15 kinds already named in `FIGHTER_KIND_NAMES`, taken from
  `doldecomp/melee`'s `src/melee/ft/forward.h` — extend the dict for more),
  and its live `weight` float, so you can visually confirm an old character
  (e.g. retail Fox at ~75) and a new/patched character (whatever weight your
  package wrote) both resolve correctly in the *same* poll — exactly the
  cross-validation pattern from `t10-memory-read-result.md`.
- **Attract-mode idle timeout is ~119s** from a cold boot to the title
  screen before Melee's own demo battles start creating `Fighter` instances
  (empirically measured, see the same evidence doc) — don't set
  `--duration` below roughly 150s if you're relying on attract-mode demos
  rather than driving menu navigation yourself with a `.dtm` movie
  (`tooling/author_dtm.py` / `tooling/fixtures/*.dtm` build/hold real menu
  navigation into character select + a match start).

### Step 3 — always clean up

```sh
kill -9 -"$DOLPHIN_PID" 2>/dev/null   # process-group kill, not the PID alone
wait "$DOLPHIN_PID" 2>/dev/null
ps -ef | grep '[d]olphin-emu'          # must be empty
coredumpctl list --since -5min          # check for a crash you didn't expect
```

If `verify_fighters.py` errors with `Dolphin GDB peer disconnected`,
Dolphin's process likely crashed/zombied mid-run (an observed, not fully
root-caused, oddity around demo-to-demo scene transitions — see
`t10-memory-read-result.md`'s "An observed Dolphin crash" section). Re-run
the whole boot rather than trying to reattach.

## 4. Driving character select / starting a match deterministically

Waiting on attract-mode demos only proves *some* fighters exist; to test
your own explicit character-select choice (old vs. new character in a real
match, not a random attract-mode battle), drive real input via a `.dtm`
movie instead of waiting:

- `tooling/author_dtm.py` shows the minimal DTM structure (256-byte header +
  8-byte-per-frame GameCube controller records) and a working menu-navigation
  frame sequence (Start -> Down -> A x3 -> Start -> A) as a starting
  template — copy its `press()`/`controller()` helpers and extend the frame
  sequence to select your specific characters/stage instead of the default
  menu path.
- `tooling/fixtures/pas44-menu-to-match.dtm` is a ready-made menu-to-match
  movie already used by the PAS-99 CI gate — inspect it as a reference for
  frame timing if you're authoring a new one for a different character slot.
- Feed the movie to Dolphin with `--movie`/`-m`, **without** the runner's
  xcb env vars set (see the rules in section 1), and read the fighter list
  with `verify_fighters.py` while it plays, or check
  `dolphin_smoke.py`'s `--expected-character-select`/`--expected-match-start`
  marker checks against your build's own `OSReport` log output if your
  runtime plugin already prints those markers (see
  `docs/character-conversion-research.md`'s PAS-28 gate list for what your
  build needs to log at each stage).

## 5. What "both characters work" evidence actually requires

Per `docs/character-conversion-research.md`'s PAS-28 gates, do not treat a
booting ISO or `archive_exists`/`data_exists` alone as proof a character is
playable. The bar this project has already set for itself is, in order:
symbol resolution logged, attribute-table validated, callback/action-table
validated, character-select completion logged, offline match started
logged — **and**, per this guide, a live GDB memory read confirming the
character's `Fighter` instance exists with the expected `kind` (and ideally
attribute values like `weight`) in the same running match as an
unmodified stock character. Any single boot or a green CI run without that
live-memory cross-check is not sufficient evidence of a working character.
