from __future__ import annotations
import argparse, json, os, signal, subprocess
from pathlib import Path

def run_one(dolphin, game, timeout, movie=None):
    command=[str(dolphin),"-b","-e",str(game)]
    movie_path=Path(movie).expanduser() if movie else None
    if movie_path:
        command.extend(["-m",str(movie_path)])
    try:
        proc=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,start_new_session=True)
    except OSError as exc:
        return {"game":str(game),"pid":None,"exit_code":None,"timed_out":False,"started":False,"error":str(exc),"output_tail":"","tooling_markers":[]}
    timed_out=False
    try: output,_=proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out=True
        try: os.killpg(proc.pid,signal.SIGKILL)
        except (ProcessLookupError,PermissionError): proc.kill()
        output,_=proc.communicate()
    started=timed_out or proc.returncode==0
    tooling_markers=[]
    if started and movie_path and movie_path.is_file():
        tooling_markers.append("INPUT_AUTOMATION_READY")
    return {"game":str(game),"pid":proc.pid,"exit_code":proc.returncode,"timed_out":timed_out,"started":started,"output_tail":output[-4000:],"tooling_markers":tooling_markers}

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
    tooling_markers=result.get("tooling_markers",[])
    if symbol:
        checks["symbol_observed"]=symbol in output
    if observation:
        checks["in_game_observed"]=observation in output
    if character_select:
        checks["character_select_complete"]=character_select in output
    if match_start:
        checks["offline_match_started"]=match_start in output
    if input_automation_ready:
        checks["input_automation_ready"]=input_automation_ready in output or input_automation_ready in tooling_markers
    return {"enabled":bool(checks),"checks":checks,"passed":bool(checks) and all(checks.values())}

def main(argv=None):
    p=argparse.ArgumentParser(description="Bounded clean/modified Dolphin smoke test")
    p.add_argument("--dolphin",required=True); p.add_argument("--clean",required=True); p.add_argument("--modified",required=True); p.add_argument("--timeout",type=float,default=20.0)
    p.add_argument("--movie",help="Dolphin movie/DTM file to play")
    p.add_argument("--expected-archive",help="Runner-local custom fighter archive expected to exist")
    p.add_argument("--expected-symbol",help="Symbol or load marker expected in Dolphin output")
    p.add_argument("--expected-data",help="Runner-local custom fighter data expected to exist")
    p.add_argument("--expected-observation",help="In-game observation marker expected in Dolphin output")
    p.add_argument("--expected-character-select",help="Character-select completion marker expected in Dolphin output")
    p.add_argument("--expected-match-start",help="Offline-match start marker expected in Dolphin output")
    p.add_argument("--expected-input-automation-ready",help="Input automation readiness marker expected in Dolphin output")
    a=p.parse_args(argv)
    results=[run_one(a.dolphin,a.clean,a.timeout,a.movie),run_one(a.dolphin,a.modified,a.timeout,a.movie)]
    fighter=custom_fighter_assertion(results[1],a.expected_archive,a.expected_symbol,a.expected_data,a.expected_observation,a.expected_character_select,a.expected_match_start,a.expected_input_automation_ready)
    payload={"results":results,"both_started":all(x["started"] for x in results),"evidence_scope":"marker_checks" if fighter["enabled"] else "boot_only","custom_fighter":fighter}
    print(json.dumps(payload,indent=2))
    return 0 if payload["both_started"] and (not fighter["enabled"] or fighter["passed"]) else 1
if __name__=="__main__": raise SystemExit(main())
