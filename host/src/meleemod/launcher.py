from __future__ import annotations
from dataclasses import dataclass
from pathlib import Path
import json, subprocess, time, os, signal
@dataclass(frozen=True)
class LaunchResult:
 command:tuple[str,...]; pid:int; exit_code:int|None; timed_out:bool; log:Path
def launch(executable,game,log,wait=False,timeout=None,command=None,cwd=None):
 cmd=tuple(command) if command else (str(executable),"-e",str(game)); log=Path(log); log.parent.mkdir(parents=True,exist_ok=True)
 with log.open("w",encoding="utf-8") as f:
  f.write("command: "+json.dumps(cmd)+"\n"); f.flush(); proc=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT,text=True,start_new_session=True,cwd=cwd)
  if not wait: return LaunchResult(cmd,proc.pid,None,False,log)
  try: code=proc.wait(timeout=timeout); timed=False
  except subprocess.TimeoutExpired:
   # Dolphin opens a modal Confirm Stop dialog on graceful termination. A
   # bounded automation run must hard-stop it so no GUI prompt is left behind.
   try:
    if os.name=="nt": proc.kill()
    else: os.killpg(proc.pid,signal.SIGKILL)
   except (ProcessLookupError,PermissionError): proc.kill()
   code=proc.wait()
   timed=True
  with log.open("a",encoding="utf-8") as out: out.write(f"exit_code: {code}\n")
 return LaunchResult(cmd,proc.pid,code,timed,log)
