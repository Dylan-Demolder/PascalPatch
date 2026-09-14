from __future__ import annotations
import argparse, json, subprocess, sys, datetime
from pathlib import Path
from .profile import load_profile
from .store import BuildStore
from .discovery import find_dolphin
from .errors import MeleeModError

def _root(args): return Path(args.root).expanduser().resolve()
def _profile(args): return _root(args)/"profiles"/(args.id+".json")
def cmd_list(args):
 for p in sorted((_root(args)/"profiles").glob("*.json")):
  try: d=json.loads(p.read_text()); print(f"{d.get('id',p.stem)}\t{d.get('name','?')}\t{d.get('mode','?')}")
  except Exception: print(f"{p.stem}\tINVALID")
 return 0
def resolve(args): return load_profile(_profile(args),_root(args))
def cmd_validate(args):
 p=resolve(args); print(json.dumps({"id":p.data["id"],"game_version":p.data["game_version"],"compatibility":p.compatibility,"plugins":[x["id"] for x in p.plugins],"mods":[x["id"] for x in p.mods]},indent=2)); return 0
def cmd_build(args):
 p=resolve(args); result=BuildStore(args.data).build(p); print(json.dumps({"profile":p.data["id"],"game_version":p.data["game_version"],"output":str(result.output),"metadata":str(result.metadata),"compatibility":result.compatibility},indent=2)); return 0
def cmd_launch(args):
 p=resolve(args); output=BuildStore(args.data).build(p).output if not args.no_build else BuildStore(args.data).root/"builds"/p.data["id"]/"current"/"game"
 if p.compatibility != "online-safe" and not args.allow_unsafe: raise MeleeModError(f"profile is {p.compatibility}; pass --allow-unsafe to launch it")
 dolphin=find_dolphin(args.dolphin); logdir=BuildStore(args.data).root/"logs"/p.data["id"]; logdir.mkdir(parents=True,exist_ok=True); log=logdir/(datetime.datetime.now().strftime("%Y%m%dT%H%M%S")+".log")
 cmd=[str(dolphin),str(output)]
 if args.dry_run: print(json.dumps({"command":cmd,"log":str(log),"compatibility":p.compatibility},indent=2)); return 0
 with log.open("w") as f:
  f.write("command: "+json.dumps(cmd)+"\n"); proc=subprocess.Popen(cmd,stdout=f,stderr=subprocess.STDOUT,text=True); print(f"launched {p.data['id']} with {dolphin}; log={log}; pid={proc.pid}")
 return 0
def cmd_logs(args):
 d=BuildStore(args.data).root/"logs"/args.id
 for p in sorted(d.glob("*.log")) if d.exists() else []: print(p)
 return 0
def main(argv=None):
 ap=argparse.ArgumentParser(prog="meleemod"); ap.add_argument("--root",default="."); ap.add_argument("--data",default=None); sub=ap.add_subparsers(dest="command",required=True)
 profile=sub.add_parser("profile"); ps=profile.add_subparsers(dest="profile_command",required=True); x=ps.add_parser("list"); x.set_defaults(func=cmd_list); x=ps.add_parser("validate"); x.add_argument("id"); x.set_defaults(func=cmd_validate)
 for name,fn in [("build",cmd_build),("launch",cmd_launch)]:
  x=sub.add_parser(name); x.add_argument("id"); x.set_defaults(func=fn)
 x=sub.choices["launch"]; x.add_argument("--dolphin"); x.add_argument("--allow-unsafe",action="store_true"); x.add_argument("--no-build",action="store_true"); x.add_argument("--dry-run",action="store_true")
 x=sub.add_parser("logs"); x.add_argument("id"); x.set_defaults(func=cmd_logs)
 args=ap.parse_args(argv)
 try: return args.func(args)
 except MeleeModError as e:
  print(str(e),file=sys.stderr)
  for er in getattr(e,"errors",[]): print(f"{er.path}: {er.code}: {er.message}",file=sys.stderr)
  return 2
 except Exception as e:
  print(f"error: {e}",file=sys.stderr); return 1
if __name__=="__main__": raise SystemExit(main())
