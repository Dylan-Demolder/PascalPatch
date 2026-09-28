from __future__ import annotations
import argparse, json, shutil, sys, datetime
from pathlib import Path
from .profile import load_profile
from .store import BuildStore
from .discovery import find_dolphin
from .errors import PascalPatchError
from .launcher import launch
from .plugin_store import PluginStore
from .native import find_melee_port, find_pascalpatch_launcher, native_command, stage_quick_match

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
def _launch_target(store,profile_id):
 current=store.root/"builds"/profile_id/"current"/"game"; iso=current/"game.iso"
 if iso.is_file(): return iso
 if current.is_dir(): return current
 raise PascalPatchError(f"no built game for {profile_id}; run 'pascalpatch build {profile_id}' first: {current}")
def prepare_run_mods(data_root,profile_id,build_mods,quick_match=None):
 """A fresh mods folder for one run: the profile's own plugins plus every enabled downloaded plugin.

 A profile's quick_match stages the quick-match plugin set to that match (it wins over a
 downloaded copy, as every profile plugin does)."""
 run=Path(data_root)/"run"/profile_id/"mods"
 if run.exists(): shutil.rmtree(run)
 if Path(build_mods).is_dir(): shutil.copytree(build_mods,run)
 if quick_match is not None: stage_quick_match(quick_match,data_root,run)
 PluginStore(data_root).stage(run)
 return run if run.is_dir() and any(run.iterdir()) else None
def cmd_launch(args):
 p=resolve(args); store=BuildStore(args.data); output=store.build(p).output if not args.no_build else _launch_target(store,p.data["id"])
 if p.compatibility != "online-safe" and not args.allow_unsafe: raise PascalPatchError(f"profile is {p.compatibility}; pass --allow-unsafe to launch it")
 logdir=BuildStore(args.data).root/"logs"/p.data["id"]; logdir.mkdir(parents=True,exist_ok=True); log=logdir/(datetime.datetime.now().strftime("%Y%m%dT%H%M%S")+".log")
 if args.runtime=="native":
  mods=prepare_run_mods(store.root,p.data["id"],Path(output).parent.parent/"mods",p.data.get("quick_match"))
  # Always through the PascalPatch launcher: the unmodified port runs with the runtime injected (offline guard, plugins).
  exe=find_melee_port(args.port); launcher=find_pascalpatch_launcher(store.root,args.launcher)
  cmd=native_command(exe,output,args.port_arg,mods=mods,launcher=launcher,sandbox=store.root/"sandbox"/p.data["id"],log=log.with_suffix(".pascalpatch.log"),settings=PluginStore(store.root).settings_dir)
  if args.dry_run: print(json.dumps({"command":cmd,"log":str(log),"compatibility":p.compatibility,"runtime":"native"},indent=2)); return 0
  # The port resolves scripts, settings and melee_port.log relative to its working directory.
  result=launch(exe,output,log,wait=args.wait,timeout=args.timeout,command=cmd,cwd=args.port_cwd); print(f"launched {p.data['id']} with {exe}; log={log}; pid={result.pid}" + (f"; exit={result.exit_code}" if result.exit_code is not None else ""))
  return 0
 dolphin=find_dolphin(args.dolphin)
 cmd=[str(dolphin),"-e",str(output)]
 if args.dry_run: print(json.dumps({"command":cmd,"log":str(log),"compatibility":p.compatibility},indent=2)); return 0
 result=launch(dolphin,output,log,wait=args.wait,timeout=args.timeout); print(f"launched {p.data['id']} with {dolphin}; log={log}; pid={result.pid}" + (f"; exit={result.exit_code}" if result.exit_code is not None else ""))
 return 0
def cmd_app(args):
 from .app.server import serve
 return serve(_root(args),BuildStore(args.data).root,port=args.http_port,window=not args.no_window)
def cmd_logs(args):
 d=BuildStore(args.data).root/"logs"/args.id
 for p in sorted(d.glob("*.log")) if d.exists() else []: print(p)
 return 0
def main(argv=None):
 ap=argparse.ArgumentParser(prog="pascalpatch"); ap.add_argument("--root",default="."); ap.add_argument("--data",default=None); sub=ap.add_subparsers(dest="command",required=True)
 profile=sub.add_parser("profile"); ps=profile.add_subparsers(dest="profile_command",required=True); x=ps.add_parser("list"); x.set_defaults(func=cmd_list); x=ps.add_parser("validate"); x.add_argument("id"); x.set_defaults(func=cmd_validate)
 for name,fn in [("build",cmd_build),("launch",cmd_launch)]:
  x=sub.add_parser(name); x.add_argument("id"); x.set_defaults(func=fn)
 x=sub.choices["launch"]; x.add_argument("--dolphin"); x.add_argument("--launcher",help="folder holding pascalpatch-launch.exe and pascalpatch_runtime.dll"); x.add_argument("--allow-unsafe",action="store_true"); x.add_argument("--no-build",action="store_true"); x.add_argument("--dry-run",action="store_true"); x.add_argument("--wait",action="store_true"); x.add_argument("--timeout",type=float,default=None); x.add_argument("--runtime",choices=("native","dolphin"),default="native"); x.add_argument("--port"); x.add_argument("--port-cwd"); x.add_argument("--port-arg",action="append",default=[],help="extra melee_port.exe argument (repeatable)")
 x=sub.add_parser("logs"); x.add_argument("id"); x.set_defaults(func=cmd_logs)
 x=sub.add_parser("app",help="open the PascalPatch desktop app"); x.add_argument("--http-port",type=int,default=0); x.add_argument("--no-window",action="store_true",help="serve only; open the printed address yourself"); x.set_defaults(func=cmd_app)
 args=ap.parse_args(argv)
 try: return args.func(args)
 except PascalPatchError as e:
  print(str(e),file=sys.stderr)
  for er in getattr(e,"errors",[]): print(f"{er.path}: {er.code}: {er.message}",file=sys.stderr)
  return 2
 except Exception as e:
  print(f"error: {e}",file=sys.stderr); return 1
if __name__=="__main__": raise SystemExit(main())
