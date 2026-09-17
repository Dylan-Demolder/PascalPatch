from __future__ import annotations
import argparse, json, os, signal, subprocess
from pathlib import Path

def run_one(dolphin, game, timeout):
    try:
        proc=subprocess.Popen([str(dolphin),"-b","-e",str(game)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,start_new_session=True)
    except OSError as exc:
        return {"game":str(game),"pid":None,"exit_code":None,"timed_out":False,"started":False,"error":str(exc),"output_tail":""}
    timed_out=False
    try: output,_=proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out=True
        try: os.killpg(proc.pid,signal.SIGKILL)
        except (ProcessLookupError,PermissionError): proc.kill()
        output,_=proc.communicate()
    return {"game":str(game),"pid":proc.pid,"exit_code":proc.returncode,"timed_out":timed_out,"started":timed_out or proc.returncode==0,"output_tail":output[-4000:]}

def custom_fighter_assertion(result, archive=None, symbol=None, data=None,
                             observation=None, character_select=None,
                             match_start=None):
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
    return {"enabled":bool(checks),"checks":checks,"passed":bool(checks) and all(checks.values())}

def main(argv=None):
    p=argparse.ArgumentParser(description="Bounded clean/modified Dolphin smoke test")
    p.add_argument("--dolphin",required=True); p.add_argument("--clean",required=True); p.add_argument("--modified",required=True); p.add_argument("--timeout",type=float,default=20.0)
    p.add_argument("--expected-archive",help="Runner-local custom fighter archive expected to exist")
    p.add_argument("--expected-symbol",help="Symbol or load marker expected in Dolphin output")
    p.add_argument("--expected-data",help="Runner-local custom fighter data expected to exist")
    p.add_argument("--expected-observation",help="In-game observation marker expected in Dolphin output")
    p.add_argument("--expected-character-select",help="Character-select completion marker expected in Dolphin output")
    p.add_argument("--expected-match-start",help="Offline-match start marker expected in Dolphin output")
    a=p.parse_args(argv)
    results=[run_one(a.dolphin,a.clean,a.timeout),run_one(a.dolphin,a.modified,a.timeout)]
    fighter=custom_fighter_assertion(results[1],a.expected_archive,a.expected_symbol,a.expected_data,a.expected_observation,a.expected_character_select,a.expected_match_start)
    payload={"results":results,"both_started":all(x["started"] for x in results),"evidence_scope":"marker_checks" if fighter["enabled"] else "boot_only","custom_fighter":fighter}
    print(json.dumps(payload,indent=2))
    return 0 if payload["both_started"] and (not fighter["enabled"] or fighter["passed"]) else 1
if __name__=="__main__": raise SystemExit(main())
