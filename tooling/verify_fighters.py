"""Live GDB-based fighter-list verifier for Dolphin smoke/character testing.

Boots a modified ISO with an already-enabled GDB stub is NOT this script's job;
launch Dolphin yourself (see docs/dolphin-control.md) with
`-C Dolphin.General.GDBPort=<port>` and this script attaches, polls the live
fighter list, and reports every FighterKind + weight it finds. Use it to
confirm a mixed roster (stock retail characters *and* a newly-injected custom
fighter) actually instantiate correctly in the same running match/demo,
instead of only checking that the ISO boots.

This is a read-only diagnostic: it never writes game memory. It reuses the
project's own bounded `host/src/meleemod/dolphin_gdb.py` transport and the
pointer chain proven live in docs/evidence/t10-memory-read-result.md:

    0x804D782C -> +0x20 (fighter list head) -> +0x08 (next) ... -> +0x2C (Fighter*)
    Fighter + 0x04  = FighterKind (u32)
    Fighter + 0x198 = weight (f32, big-endian)

FighterKind values are from src/melee/ft/forward.h in doldecomp/melee (0=Mario,
1=Fox, 2=Captain Falcon, 3=Donkey Kong, 4=Kirby, 5=Bowser/Koopa, 6=Link,
7=Sheik, 8=Ness, 9=Peach, 10=Popo, 11=Nana, 12=Pikachu, 13=Samus, 14=Yoshi,
...). Extend FIGHTER_KIND_NAMES if you need more entries verified.

Example, verifying an old (retail) + new (custom-injected) character both
resolve to live Fighter instances in the same poll:

    PYTHONPATH=host/src python tooling/verify_fighters.py \\
        --port 24689 --duration 180 --interval 8 \\
        --expect-kind 1 --expect-kind 20

--expect-kind is repeatable; the script exits non-zero if any requested
FighterKind is never observed within --duration seconds.
"""
from __future__ import annotations

import argparse
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "host" / "src"))

from meleemod.dolphin_gdb import DolphinGdbClient, DolphinGdbError  # noqa: E402

FIGHTER_LIST_BASE = 0x804D782C
LIST_HEAD_OFFSET = 0x20
NEXT_OFFSET = 0x08
FIGHTER_PTR_OFFSET = 0x2C
KIND_OFFSET = 0x04
WEIGHT_OFFSET = 0x198
MAX_CHAIN_LEN = 64  # bounded walk; a real fighter list never has this many entries

FIGHTER_KIND_NAMES = {
    0: "Mario", 1: "Fox", 2: "Captain Falcon", 3: "Donkey Kong", 4: "Kirby",
    5: "Bowser", 6: "Link", 7: "Sheik", 8: "Ness", 9: "Peach", 10: "Popo",
    11: "Nana", 12: "Pikachu", 13: "Samus", 14: "Yoshi",
}


def _drain(client: DolphinGdbClient) -> None:
    """Flush stray queued bytes after an interrupt (see t10-memory-read-result.md:
    a single Ctrl-C can produce multiple queued stop-reply packets on this
    Dolphin build; leftovers get misparsed as the next command's response)."""
    client.sock.settimeout(0.2)
    try:
        while client.sock.recv(4096):
            pass
    except OSError:
        pass
    finally:
        client.sock.settimeout(client.timeout)


def read_fighters(client: DolphinGdbClient) -> list[dict]:
    """One bounded poll of the live fighter list. Always re-derives the full
    pointer chain from the fixed base address; never caches a pointer across
    polls, since fighter slots are reused as fighters are created/destroyed."""
    client.interrupt()
    _drain(client)
    fighters = []
    try:
        list_head_ptr = int.from_bytes(client.read_memory(FIGHTER_LIST_BASE, 4), "big")
        node = int.from_bytes(client.read_memory(list_head_ptr + LIST_HEAD_OFFSET, 4), "big")
        seen = 0
        while node and seen < MAX_CHAIN_LEN:
            seen += 1
            fighter_ptr = int.from_bytes(client.read_memory(node + FIGHTER_PTR_OFFSET, 4), "big")
            if fighter_ptr:
                kind = int.from_bytes(client.read_memory(fighter_ptr + KIND_OFFSET, 4), "big")
                weight_raw = client.read_memory(fighter_ptr + WEIGHT_OFFSET, 4)
                (weight,) = struct.unpack(">f", weight_raw)
                fighters.append({
                    "gobj": node, "fighter_ptr": fighter_ptr,
                    "kind": kind, "kind_name": FIGHTER_KIND_NAMES.get(kind, f"kind_{kind}"),
                    "weight": weight,
                })
            node = int.from_bytes(client.read_memory(node + NEXT_OFFSET, 4), "big")
    finally:
        client.continue_execution()
    return fighters


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=24689, help="Dolphin.General.GDBPort")
    p.add_argument("--duration", type=float, default=180.0, help="total wall-clock seconds to poll")
    p.add_argument("--interval", type=float, default=8.0, help="seconds between polls")
    p.add_argument("--timeout", type=float, default=5.0, help="GDB socket timeout per command")
    p.add_argument("--expect-kind", type=int, action="append", default=[],
                    help="FighterKind that must be observed at least once (repeatable)")
    args = p.parse_args(argv)

    seen_kinds: dict[int, dict] = {}
    deadline = time.monotonic() + args.duration
    poll = 0
    try:
        client = DolphinGdbClient.tcp(args.host, args.port, timeout=args.timeout)
    except DolphinGdbError as exc:
        print(f"verify_fighters: could not connect to Dolphin GDB stub: {exc}", file=sys.stderr)
        return 2

    with client:
        client.continue_execution()  # let the CPU run past the initial connection stop
        while time.monotonic() < deadline:
            poll += 1
            try:
                fighters = read_fighters(client)
            except DolphinGdbError as exc:
                print(f"poll {poll}: read error: {exc}", file=sys.stderr)
                time.sleep(args.interval)
                continue
            elapsed = args.duration - (deadline - time.monotonic())
            if fighters:
                print(f"poll {poll} t={elapsed:6.1f}s fighters={len(fighters)}")
                for f in fighters:
                    print(f"  kind={f['kind']:>3} ({f['kind_name']:<16}) weight={f['weight']:.2f} "
                          f"fighter_ptr={f['fighter_ptr']:#010x}")
                    seen_kinds.setdefault(f["kind"], f)
            else:
                print(f"poll {poll} t={elapsed:6.1f}s fighters=0")
            time.sleep(args.interval)

    print()
    print("=== summary ===")
    for kind, f in sorted(seen_kinds.items()):
        print(f"kind={kind:>3} ({f['kind_name']:<16}) last_weight={f['weight']:.2f}")

    missing = [k for k in args.expect_kind if k not in seen_kinds]
    if missing:
        print(f"MISSING expected FighterKind(s): {missing}", file=sys.stderr)
        return 1
    if args.expect_kind:
        print(f"all expected FighterKind(s) observed: {args.expect_kind}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
