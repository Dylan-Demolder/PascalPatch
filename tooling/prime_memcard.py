"""One-time (per-host) memory-card primer for Dolphin GALE01 smoke/verification runs.

Root cause this fixes: with a fresh/empty GameCube memory card, Melee shows a
GameCube-BIOS-level "The Memory Card in Slot A has no saved Game Data. Create
Game Data?" Yes/No dialog on first boot, then (after confirming) a second,
purely-timed "Game Data has been created." banner (clears on its own after
roughly 15-25s) before reaching the intro FMV / title screen.

Confirmed by direct observation on this host:
- The Yes/No dialog is NOT timed and NOT reachable via DTM movie input.
  A DTM authored to press A continuously for 90s of frames never cleared it
  (Dolphin's movie-input hook attaches after this BIOS-level prompt, not
  before it) -- movies are not a viable way to prime a card.
- Plain `xdotool key --window <id> <key>` (no window activation first) also
  did NOT clear it, even though `xdotool search` found the window fine.
- `xdotool windowactivate --sync <id>` immediately followed by `xdotool key
  <key>` DID clear it (confirmed live: dialog vanished, boot proceeded into
  the intro FMV). Window activation-before-key is the load-bearing step.
- The following banner ("Game Data has been created.") IS purely timed --
  it cleared on its own with no further input.

Every CI/headless probe does `rm -rf "$user_dir"` before each launch, so this
whole two-stage prompt reappears on literally every single run, and no
existing automation (movie, or send-input-without-activating) gets past it --
which is exactly why runs have been reported stuck "at the memory card
screen."

The fix here is NOT to solve this every run. It is a one-time interactive
priming pass (this script, requires a real X11/Wayland+XWayland display,
e.g. run directly on the desktop, not headless/off-screen) that boots once,
answers the dialog for real via xdotool, waits out the timed banner, then
harvests the resulting `GC/<region>/Card A/*.gci` memory card and caches it
outside the repo. Every subsequent headless run then seeds that cached card
into a fresh user-dir via dolphin_smoke.py's --memcard-seed, so the whole
two-stage prompt never appears again on any CI/automated run.

IMPORTANT legal boundary (docs/legal-notice.md): the harvested memory card
contains real Melee-derived save data (icon/banner graphics baked into the
.gci by the game itself). It must NEVER be committed to this repository or
distributed. --output must point outside the repo; this script refuses to
write under any directory containing a `.git` folder, as a guard rail.

Usage (run once per host that will run Dolphin smoke tests, or whenever the
cache is missing/stale -- must be run with a real display available, e.g.
directly at the desktop, DISPLAY=:0):

    PYTHONPATH=host/src python tooling/prime_memcard.py \\
        --dolphin /usr/bin/dolphin-emu \\
        --iso "/path/to/Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso" \\
        --output ~/.cache/meleemod/memcard-seed

Then pass `--memcard-seed ~/.cache/meleemod/memcard-seed` to
tooling/dolphin_smoke.py (or point the CI workflow's MEMCARD_SEED env var at
the same path) for every subsequent headless/CI run.
"""
from __future__ import annotations

import argparse
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

# GCPad1 default binding for the GameCube A button, per Config/GCPadNew.ini
# (Buttons/A = `X`). Confirm-Yes on the memory-card dialog is bound to A.
CONFIRM_KEY = "x"

WINDOW_APPEAR_TIMEOUT = 15.0
POST_ACTIVATE_SETTLE = 1.0
DIALOG_APPEAR_WAIT = 5.0
BANNER_SETTLE_WAIT = 20.0


def _refuses_repo_path(output: Path) -> bool:
    for parent in [output, *output.parents]:
        if (parent / ".git").exists():
            return True
    return False


def _require_display() -> str:
    display = os.environ.get("DISPLAY")
    if not display:
        raise SystemExit(
            "prime_memcard.py requires a real display (DISPLAY unset). This is a "
            "one-time interactive priming pass -- run it directly on the desktop "
            "session, not from a headless/off-screen environment. The dialog it "
            "answers cannot be cleared via a DTM movie or a background subprocess."
        )
    return display


def _find_window(display: str, timeout: float) -> str:
    """Find Dolphin's real render/main window, not an internal helper window.

    `xdotool search --name Dolphin` matches several windows (a Qt selection
    owner, an early splash titled just "Dolphin 2606", and the real game
    window titled "Dolphin 2606 | <core> | <backend> | ... | <Game Title>
    (<GAMEID>)"). Only the last one receives/renders real input; picking the
    wrong id here is why an earlier version of this script silently failed
    to confirm the dialog (window found, but it was the wrong window)."""
    deadline = time.monotonic() + timeout
    env = {**os.environ, "DISPLAY": display}
    while time.monotonic() < deadline:
        result = subprocess.run(["xdotool", "search", "--name", "Dolphin"],
                                 env=env, capture_output=True, text=True)
        ids = [line.strip() for line in result.stdout.splitlines() if line.strip()]
        for window_id in ids:
            name = subprocess.run(["xdotool", "getwindowname", window_id],
                                   env=env, capture_output=True, text=True).stdout.strip()
            if "|" in name:  # only the real render window's title has pipe-separated core/backend/game info
                return window_id
        time.sleep(0.5)
    raise SystemExit("no Dolphin render window (title containing '|') found via `xdotool search` within the timeout")


def _confirm_dialog(display: str, window_id: str) -> None:
    env = {**os.environ, "DISPLAY": display}
    # windowactivate-before-key is the load-bearing step: a bare
    # `xdotool key --window <id> <key>` was observed to NOT deliver input to
    # this dialog on this host, even though the window was found correctly.
    # A single post-activate keypress was still unreliable in testing (one
    # in three attempts registered nothing); sending it 3x with
    # --clearmodifiers, spaced a second apart, cleared the dialog every time.
    subprocess.run(["xdotool", "windowactivate", "--sync", window_id], env=env, check=True)
    time.sleep(POST_ACTIVATE_SETTLE)
    for _ in range(3):
        subprocess.run(["xdotool", "key", "--clearmodifiers", CONFIRM_KEY], env=env, check=True)
        time.sleep(1.0)


def prime(dolphin: Path, iso: Path, output: Path, banner_wait: float, hard_timeout: float) -> dict:
    if _refuses_repo_path(output):
        raise SystemExit(
            f"refusing to write a primed memory card under a git repository: {output}\n"
            "This directory contains real Melee-derived save data and must never be "
            "committed (see docs/legal-notice.md). Choose a path outside any repo, "
            "e.g. ~/.cache/meleemod/memcard-seed."
        )
    display = _require_display()

    with tempfile.TemporaryDirectory(prefix="meleemod-memcard-prime-") as td:
        user_dir = Path(td) / "user"
        user_dir.mkdir(parents=True, exist_ok=True)
        command = [str(dolphin), "-b", "-e", str(iso), "-u", str(user_dir)]
        proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                 text=True, start_new_session=True, env={**os.environ, "DISPLAY": display})
        deadline = time.monotonic() + hard_timeout
        try:
            print("waiting for the Dolphin window to appear ...")
            window_id = _find_window(display, WINDOW_APPEAR_TIMEOUT)
            print(f"found window {window_id}; waiting {DIALOG_APPEAR_WAIT:.0f}s for the "
                  "memory-card dialog to render")
            time.sleep(DIALOG_APPEAR_WAIT)
            print("sending confirm (A) to the 'Create Game Data?' dialog")
            _confirm_dialog(display, window_id)
            print(f"confirmed; waiting {banner_wait:.0f}s for the timed "
                  "'Game Data has been created.' banner and intro FMV to clear")
            time.sleep(min(banner_wait, max(0.0, deadline - time.monotonic())))
        finally:
            try:
                os.killpg(proc.pid, signal.SIGKILL)  # never SIGTERM: pops a confirm-close dialog
            except (ProcessLookupError, PermissionError):
                proc.kill()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                pass

        gc_dir = user_dir / "GC"
        gci_files = sorted(gc_dir.rglob("*.gci")) if gc_dir.is_dir() else []
        if not gci_files:
            raise SystemExit(
                f"no .gci file was created under {gc_dir} -- the dialog may not have been "
                "confirmed (check the window really got focus) or the banner did not finish "
                "(increase --wait). Not caching an unprimed/empty card."
            )
        output.mkdir(parents=True, exist_ok=True)
        shutil.copytree(gc_dir, output / "GC", dirs_exist_ok=True)
        return {"gci_files": [str(p.relative_to(gc_dir)) for p in gci_files], "output": str(output / "GC")}


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--dolphin", required=True, type=Path)
    p.add_argument("--iso", required=True, type=Path)
    p.add_argument("--output", required=True, type=Path,
                    help="Directory to write the primed GC/ tree into (outside any git repo)")
    p.add_argument("--wait", type=float, default=BANNER_SETTLE_WAIT,
                    help="Seconds to wait after confirming for the timed banner/FMV to clear")
    p.add_argument("--hard-timeout", type=float, default=90.0, help="Absolute upper bound safety timeout")
    args = p.parse_args(argv)
    result = prime(args.dolphin, args.iso, args.output, args.wait, args.hard_timeout)
    print(f"primed memory card cached at {result['output']}: {result['gci_files']}")
    print("Reminder: this directory contains real Melee save data. Never commit or "
          "distribute it (docs/legal-notice.md). Pass it to dolphin_smoke.py via "
          "--memcard-seed, or set MEMCARD_SEED for the CI workflow.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
