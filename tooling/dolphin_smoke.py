from __future__ import annotations
import argparse, json, os, signal, subprocess
from pathlib import Path

def run_one(dolphin, game, timeout):
    proc=subprocess.Popen([str(dolphin),"-b","-e",str(game)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,start_new_session=True)
    timed_out=False
    try: output,_=proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out=True
        try: os.killpg(proc.pid,signal.SIGKILL)
        except (ProcessLookupError,PermissionError): proc.kill()
        output,_=proc.communicate()
    return {"game":str(game),"pid":proc.pid,"exit_code":proc.returncode,"timed_out":timed_out,"started":timed_out or proc.returncode==0,"output_tail":output[-4000:]}

def main(argv=None):
    p=argparse.ArgumentParser(description="Bounded clean/modified Dolphin smoke test")
    p.add_argument("--dolphin",required=True); p.add_argument("--clean",required=True); p.add_argument("--modified",required=True); p.add_argument("--timeout",type=float,default=20.0)
    a=p.parse_args(argv); results=[run_one(a.dolphin,a.clean,a.timeout),run_one(a.dolphin,a.modified,a.timeout)]
    print(json.dumps({"results":results,"both_started":all(x["started"] for x in results)},indent=2)); return 0
if __name__=="__main__": raise SystemExit(main())
