from __future__ import annotations
import argparse, json, os, re, signal, subprocess
from pathlib import Path

MOVIE_HINT_RE = re.compile(r"(?:playing|playback|movie|\.dtm|\bDTM\b)", re.IGNORECASE)
SCENE_RE = re.compile(r"GM:(\d+)\s+SC:(\d+)")
DTM_HEADER_SIZE = 256
DTM_GAME_ID_OFFSET = 4
DTM_GAME_ID_SIZE = 6
DTM_WII_OFFSET = 10
DTM_CONTROLLERS_OFFSET = 11
DTM_SAVE_STATE_OFFSET = 12
DTM_FRAME_COUNT_OFFSET = 13


def read_dtm_metadata(movie_path):
    data = movie_path.read_bytes()
    if len(data) < DTM_HEADER_SIZE:
        raise ValueError(f"DTM is shorter than the 256-byte header: {movie_path}")
    if data[:4] != b"DTM\x1a":
        raise ValueError(f"movie is not a raw DTM file: {movie_path}")
    game_id = data[DTM_GAME_ID_OFFSET : DTM_GAME_ID_OFFSET + DTM_GAME_ID_SIZE].rstrip(b"\x00").decode("ascii", "replace")
    if game_id != "GALE01":
        raise ValueError(f"DTM game id must be GALE01, got {game_id!r}")
    if data[DTM_WII_OFFSET] != 0:
        raise ValueError("DTM is a Wii movie; GALE01 smoke requires a GameCube movie")
    if not data[DTM_CONTROLLERS_OFFSET] & 0x01:
        raise ValueError("DTM has no GameCube controller 1 enabled")
    if data[DTM_SAVE_STATE_OFFSET] != 0:
        raise ValueError("DTM starts from a save state; smoke requires boot playback")
    frame_count = int.from_bytes(data[DTM_FRAME_COUNT_OFFSET : DTM_FRAME_COUNT_OFFSET + 8], "little")
    if frame_count <= 0:
        raise ValueError("DTM has no recorded frames")
    return {"game_id": game_id, "controllers": data[DTM_CONTROLLERS_OFFSET], "frame_count": frame_count}

def parse_playback_evidence(output):
    text = output or ""
    scenes = ["GM:%s SC:%s" % (gm, sc) for gm, sc in SCENE_RE.findall(text)]
    last_scene = scenes[-1] if scenes else None
    advanced_past_boot = any(s != "GM:28 SC:00" for s in scenes)
    return {"movie_playback_entered": bool(MOVIE_HINT_RE.search(text)), "scenes": scenes[-20:], "last_scene": last_scene, "advanced_past_boot": advanced_past_boot, "output_len": len(text)}

def _read_dolphin_log(log_file, user_dir=None):
    candidates = [Path(log_file)] if log_file else []
    roots = []
    if user_dir:
        roots.append(Path(user_dir))
    configured_dir = os.environ.get("DOLPHIN_EMU_USER_DIR")
    if configured_dir:
        roots.append(Path(configured_dir).expanduser())
    roots.extend((Path.home() / ".config" / "dolphin-emu", Path.home() / ".dolphin-emu"))
    for root in roots:
        for path in (root / "dolphin.log", root / "Logs" / "dolphin.log"):
            if path not in candidates:
                candidates.append(path)
        if root.is_dir():
            candidates.extend(path for path in root.rglob("dolphin.log") if path not in candidates)
    content = ""
    source = None
    for path in candidates:
        try:
            value = path.read_text(errors="replace")
        except OSError:
            continue
        if len(value) > len(content):
            content, source = value, path
    return content, str(source) if source else None


def run_one(dolphin, game, timeout, movie=None, log_file=None, user_dir=None):
    movie_path=Path(movie).expanduser() if movie else None
    movie_metadata = None
    movie_error = None
    if movie_path:
        try:
            movie_metadata = read_dtm_metadata(movie_path)
        except (OSError, ValueError) as exc:
            movie_error = str(exc)
    command=[str(dolphin)]
    if Path(dolphin).name != "dolphin-emu-nogui":
        command.append("--batch")
    command.extend(["-e", str(game)])
    if movie_path:
        command.extend(["-m", str(movie_path)])
    run_user_dir = Path(user_dir).expanduser() if user_dir else None
    if run_user_dir:
        run_user_dir.mkdir(parents=True, exist_ok=True)
        command.extend(["-u", str(run_user_dir)])
    for setting in ("Logger.Options.WriteToFile=True", "Logger.Options.WriteToConsole=True", "Logger.Logs.BOOT=True", "Logger.Logs.CORE=True", "Logger.Logs.OSREPORT=True"):
        command.extend(["-C", setting])
    log_path = Path(log_file).expanduser() if log_file else None
    if log_path:
        log_path.parent.mkdir(parents=True, exist_ok=True)
    # PAS-165 host root cause: the runner tab exports QT_QPA_PLATFORM=xcb plus
    # QT_XCB_NO_XI2/QT_WAYLAND_RECONNECT (needed only for xdotool in the record
    # job). Under that Qt env, Dolphin 2606 stalls/SIGILLs during movie playback
    # (boot without a movie still passes). Native Wayland playback passes, so
    # neutralise the xcb vars for the smoke subprocess only.
    dolphin_env = os.environ.copy()
    for var in ("QT_QPA_PLATFORM", "QT_XCB_NO_XI2", "QT_WAYLAND_RECONNECT"):
        dolphin_env.pop(var, None)
    if os.environ.get("PAS_DOLPHIN_QT_QPA_PLATFORM"):
        dolphin_env["QT_QPA_PLATFORM"] = os.environ["PAS_DOLPHIN_QT_QPA_PLATFORM"]
    try:
        proc=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,start_new_session=True,env=dolphin_env)
    except OSError as exc:
        return {"game":str(game),"pid":None,"exit_code":None,"timed_out":False,"started":False,"error":str(exc),"output_tail":"","output_len":0,"log_file":str(log_path) if log_path else None,"log_len":0,"user_dir":None,"movie":str(movie_path) if movie_path else None,"movie_exists":movie_path.is_file() if movie_path else False,"movie_metadata":movie_metadata,"movie_error":movie_error,"playback":parse_playback_evidence("")}
    timed_out=False
    previous_handlers = {signum: signal.getsignal(signum) for signum in (signal.SIGTERM, signal.SIGINT)}
    for signum in previous_handlers:
        signal.signal(signum, lambda _signum, _frame: (_ for _ in ()).throw(KeyboardInterrupt()))
    try:
        try: output,_=proc.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out=True
            try: os.killpg(proc.pid,signal.SIGKILL)
            except (ProcessLookupError,PermissionError): proc.kill()
            output,_=proc.communicate()
    finally:
        for signum, handler in previous_handlers.items():
            signal.signal(signum, handler)
        if proc.poll() is None:
            try: os.killpg(proc.pid,signal.SIGKILL)
            except (ProcessLookupError,PermissionError): proc.kill()
            proc.wait()
    log_output, discovered_log = _read_dolphin_log(log_path, run_user_dir)
    evidence_output = log_output or output or ""
    if log_path and evidence_output and discovered_log != str(log_path):
        log_path.write_text(evidence_output)
    started=timed_out or proc.returncode==0
    return {"game":str(game),"pid":proc.pid,"exit_code":proc.returncode,"timed_out":timed_out,"started":started,"output_tail":evidence_output[-4000:],"output_len":len(evidence_output),"log_file":str(log_path) if log_path else discovered_log,"log_len":len(log_output),"user_dir":str(run_user_dir) if run_user_dir else None,"movie":str(movie_path) if movie_path else None,"movie_exists":movie_path.is_file() if movie_path else False,"movie_metadata":movie_metadata,"movie_error":movie_error,"playback":parse_playback_evidence(evidence_output)}


def custom_fighter_assertion(result, archive=None, symbol=None, data=None,
                              observation=None, character_select=None,
                              match_start=None, input_automation_ready=None):
    checks={}
    if archive:
        path=Path(archive).expanduser()
        checks["archive_exists"]=path.is_file()
    if data:
        path=Path(data).expanduser()
        checks["data_exists"]=path.is_file()
    output=result.get("output_tail","")

    if symbol:
        checks["symbol_observed"]=symbol in output
    if observation:
        checks["in_game_observed"]=observation in output
    if character_select:
        checks["character_select_complete"]=character_select in output
    if match_start:
        checks["offline_match_started"]=match_start in output
    if input_automation_ready:
         checks["input_automation_ready"]=input_automation_ready in output

    return {"enabled":bool(checks),"checks":checks,"passed":bool(checks) and all(checks.values())}

def main(argv=None):
    p=argparse.ArgumentParser(description="Bounded clean/modified Dolphin smoke test")
    p.add_argument("--dolphin",required=True); p.add_argument("--clean",required=True); p.add_argument("--modified",required=True); p.add_argument("--timeout",type=float,default=20.0)
    p.add_argument("--movie",help="Dolphin movie/DTM file to play")
    p.add_argument("--log-dir",help="Directory for one raw Dolphin log per ISO")
    p.add_argument("--user-dir",help="Explicit Dolphin user directory for isolated settings and logs")
    p.add_argument("--expected-archive",help="Runner-local custom fighter archive expected to exist")
    p.add_argument("--expected-symbol",help="Symbol or load marker expected in Dolphin output")
    p.add_argument("--expected-data",help="Runner-local custom fighter data expected to exist")
    p.add_argument("--expected-observation",help="In-game observation marker expected in Dolphin output")
    p.add_argument("--expected-character-select",help="Character-select completion marker expected in Dolphin output")
    p.add_argument("--expected-match-start",help="Offline-match start marker expected in Dolphin output")
    p.add_argument("--expected-input-automation-ready",help="Input automation readiness marker expected in Dolphin output")
    a=p.parse_args(argv)
    log_dir=Path(a.log_dir).expanduser() if a.log_dir else None
    if log_dir:
        log_dir.mkdir(parents=True, exist_ok=True)
    clean_log=log_dir / "clean-dolphin.log" if log_dir else None
    modified_log=log_dir / "modified-dolphin.log" if log_dir else None
    user_dir=Path(a.user_dir).expanduser() if a.user_dir else None
    clean_user_dir=user_dir / "clean" if user_dir else None
    modified_user_dir=user_dir / "modified" if user_dir else None
    results=[run_one(a.dolphin,a.clean,a.timeout,a.movie,clean_log,clean_user_dir),run_one(a.dolphin,a.modified,a.timeout,a.movie,modified_log,modified_user_dir)]
    fighter=custom_fighter_assertion(results[1],a.expected_archive,a.expected_symbol,a.expected_data,a.expected_observation,a.expected_character_select,a.expected_match_start,a.expected_input_automation_ready)
    payload={"results":results,"both_started":all(x["started"] for x in results),"evidence_scope":"marker_checks" if fighter["enabled"] else "boot_only","custom_fighter":fighter}
    print(json.dumps(payload,indent=2))
    return 0 if payload["both_started"] and (not fighter["enabled"] or fighter["passed"]) else 1
if __name__=="__main__": raise SystemExit(main())
