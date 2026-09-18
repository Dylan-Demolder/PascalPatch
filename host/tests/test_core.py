import hashlib, json, os, shutil, tempfile, unittest
from unittest.mock import patch
from types import SimpleNamespace
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).parents[1]/"src"))
from meleemod.discovery import inspect_iso, inspect_game, EXPECTED_DOL_SHA1
from meleemod.manifest import validate_profile, validate_plugin, validate_mod
from meleemod.profile import load_profile
from meleemod.store import BuildStore
from meleemod.errors import ManifestError, CompositionError, DiscoveryError
from meleemod.character_package import validate_package, installation_plan, compose_validated_package
from meleemod.plugin_composer import resolve_plugin_order, compose_static_manifest
from meleemod.launcher import launch
from meleemod.discovery import find_dolphin
from meleemod.bridge import Message, encode, decode
from meleemod.bridge_transport import UnixBridgeServer
from meleemod.dolphin_gdb import DolphinGdbClient, DolphinGdbMailbox, DolphinGdbError, MAX_TRANSFER, RUNTIME_FRAME_CAPACITY
from meleemod.diagnostics import report, symbolize_native
from meleemod.safety import validate_safety
from meleemod.registry import install_local, install_remote, install_remote_archive, install_remote_character_package
from meleemod.registry_signing import public_key, key_id, make_trust, sign, sign_index, verify_index, sign_trust_update, verify_trust_update, fetch_index, fetch_https_index, update_https_index
from meleemod.static_integration import make_bundle, apply_overlay
sys.path.insert(0,str(Path(__file__).parents[2]/"tooling"))
from meleemod.recompose_iso import recompose_iso
sys.path.insert(0,str(Path(__file__).parents[2]/"tooling"))
from dolphin_smoke import custom_fighter_assertion, run_one

ISO=Path(os.environ.get("MELEEMOD_TEST_ISO","/home/dyland/Downloads/Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso")).expanduser()
DOL=Path(os.environ.get("MELEEMOD_TEST_DOL","/home/dyland/Documents/MeleeDecomp/melee/build/GALE01/main.dol")).expanduser()
class CoreTests(unittest.TestCase):
 def test_real_iso_revision_and_hash(self):
  if not ISO.exists(): self.skipTest("local user ISO unavailable")
  x=inspect_iso(ISO); self.assertEqual(x.main_dol_sha1,EXPECTED_DOL_SHA1); self.assertEqual(x.revision,2)
 def test_profile_rejects_unknown_and_duplicate(self):
  x={"id":"x-profile","name":"x","game_version":"GALE01-1.02","base_game":"x","plugins":["a","a"],"mods":[],"mode":"offline","online_safe":False,"extra":1}
  errors=validate_profile(x); self.assertTrue(any(e.code=="unknown_field" for e in errors)); self.assertTrue(any(e.code=="duplicate" for e in errors))
 def test_build_is_atomic_and_composes_exact_target(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); base=t/"base"; (base/"sys").mkdir(parents=True); shutil.copy2(DOL,base/"sys/main.dol"); (base/"files").mkdir(); (base/"files/original.bin").write_bytes(b"old")
   modsrc=t/"mods/skin"; (modsrc/"files").mkdir(parents=True); (modsrc/"files/original.bin").write_bytes(b"new")
   (t/"mods/skin/mod.json").write_text(json.dumps({"id":"skin","version":"1.0.0","type":"filesystem","source":".","targets":["files/original.bin"],"conflicts":[],"priority":0}))
   (t/"plugins").mkdir(); (t/"profiles").mkdir()
   # catalog source is relative to project root (t), so use mods/skin
   (t/"mods/skin/mod.json").write_text(json.dumps({"id":"skin","version":"1.0.0","type":"filesystem","source":"mods/skin","targets":["files/original.bin"],"conflicts":[],"priority":0}))
   prof={"id":"offline","name":"Offline","game_version":"GALE01-1.02","base_game":"../base","plugins":[],"mods":["skin"],"mode":"offline","online_safe":False}; (t/"profiles/offline.json").write_text(json.dumps(prof))
   p=load_profile(t/"profiles/offline.json",t); r=BuildStore(t/"data").build(p); self.assertEqual((r.output/"files/original.bin").read_bytes(),b"new"); self.assertTrue((r.metadata).exists()); self.assertTrue((t/"data/builds/offline/current").exists())
 def test_conflicting_targets_fail(self):
  from meleemod.composer import compose_assets
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"base").mkdir(); (t/"a").mkdir(); (t/"b").mkdir(); (t/"a/x").write_bytes(b"a"); (t/"b/x").write_bytes(b"b")
   mods=[{"id":"a","source":str(t/"a"),"targets":["x"],"priority":0},{"id":"b","source":str(t/"b"),"targets":["x"],"priority":0}]
   with self.assertRaises(CompositionError): compose_assets(t/"base",mods,t/"out")
 def test_character_package_checksums_and_traversal(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"assets").mkdir(); (t/"assets/model.bin").write_bytes(b"model")
   char={"id":"clone","display_name":"Clone","version":"1.0.0","author":"test","license":"CC0","target_game_version":"GALE01-1.02","compatibility":"offline-gameplay"}
   (t/"character.json").write_text(json.dumps(char)); (t/"moveset.json").write_text(json.dumps({"base_fighter":"mario","moves":[]}))
   import hashlib; (t/"checksums.json").write_text(json.dumps({"assets/model.bin":hashlib.sha256(b"model").hexdigest()}))
   self.assertEqual(validate_package(t)["id"],"clone"); self.assertEqual(installation_plan(t,"offline")["status"],"validated-only")
   with self.assertRaises(ManifestError): installation_plan(t,"slippi")

 def test_opt_in_runtime_diagnostics_validate(self):
  plugin={"id":"timing","version":"1.0.0","api_version":1,"entrypoint":"plugin_init","capabilities":["visual-only"],"dependencies":[],"game_versions":["GALE01-1.02"],"online_safe":True,"frame_timing":True,"shutdown_after_frames":120}
  self.assertEqual(validate_plugin(plugin),[])
  self.assertEqual(validate_plugin(dict(plugin,init_phase="startup")),[])
  self.assertTrue(validate_plugin(dict(plugin,frame_timing="yes")))
  self.assertTrue(validate_plugin(dict(plugin,shutdown_after_frames=0)))

 def test_catalog_loader_accepts_startup_plugin(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"plugins/demo-test").mkdir(parents=True); (t/"profiles").mkdir(); (t/"base.iso").write_bytes(b"owned")
   plugin={"id":"demo-test","version":"1.0.0","api_version":1,"entrypoint":"plugin_init","capabilities":["visual-only"],"dependencies":[],"game_versions":["GALE01-1.02"],"online_safe":True,"init_phase":"startup"}
   (t/"plugins/demo-test/plugin.json").write_text(json.dumps(plugin))
   profile={"id":"demo-profile","name":"Demo","game_version":"GALE01-1.02","base_game":"../base.iso","plugins":["demo-test"],"mods":[],"mode":"offline","online_safe":False}
   (t/"profiles/demo-profile.json").write_text(json.dumps(profile))
   loaded=load_profile(t/"profiles/demo-profile.json",t)
   self.assertEqual(loaded.plugins[0]["id"],"demo-test")
   self.assertEqual(loaded.plugins[0]["init_phase"],"startup")

 def test_plugin_order_and_cycle_rejection(self):
  def p(ident,deps): return {"id":ident,"version":"1.0.0","api_version":1,"entrypoint":"plugin_init","capabilities":["visual-only"],"dependencies":deps,"game_versions":["GALE01-1.02"],"online_safe":True}
  ordered=resolve_plugin_order([p("bb",["aa"]),p("aa",[])]); self.assertEqual([x["id"] for x in ordered],["aa","bb"]); self.assertEqual(compose_static_manifest(ordered)["link_status"],"deferred-until-runtime-integration")
  with self.assertRaises(Exception): resolve_plugin_order([p("aa",["bb"]),p("bb",["aa"])])

 def test_dolphin_discovery_accepts_real_help_wording(self):
  with tempfile.TemporaryDirectory() as td:
   fake=Path(td)/"dolphin-emu"; fake.write_text("#!/bin/sh\nif [ \"$1\" = \"--help\" ]; then echo 'Load the specified file'; echo '--batch'; fi\n"); fake.chmod(0o755)
   self.assertEqual(find_dolphin(fake),fake.resolve())

 def test_launch_helper_records_exit(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); fake=t/"emu"; fake.write_text("#!/bin/sh\necho booted\nexit 7\n"); fake.chmod(0o755); r=launch(fake,"game.iso",t/"run.log",wait=True,timeout=2); self.assertEqual(r.exit_code,7); self.assertIn("booted",(t/"run.log").read_text()); self.assertIn("exit_code: 7",(t/"run.log").read_text())

 def test_launch_helper_hard_stops_on_timeout(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); fake=t/"emu"; fake.write_text("#!/bin/sh\ntrap '' TERM\nsleep 30\n"); fake.chmod(0o755); r=launch(fake,"game.iso",t/"run.log",wait=True,timeout=0.1); self.assertTrue(r.timed_out); self.assertIsNotNone(r.exit_code)

 def test_launch_no_build_resolves_iso_game_target(self):
  import io, contextlib
  from meleemod import cli
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"profiles").mkdir(); (t/"base.iso").write_bytes(b"disc"); (t/"profiles/iso-profile.json").write_text(json.dumps({"id":"iso-profile","name":"Iso","game_version":"GALE01-1.02","base_game":"../base.iso","plugins":[],"mods":[],"mode":"offline","online_safe":False}))
   with patch("meleemod.store.inspect_game",return_value=SimpleNamespace(kind="iso",main_dol_sha1=EXPECTED_DOL_SHA1)): built=BuildStore(t/"data").build(load_profile(t/"profiles/iso-profile.json",t)).output
   current=Path(td)/"data/builds/iso-profile/current/game/game.iso"; self.assertTrue(built.is_file()); self.assertTrue(current.is_file()); self.assertEqual(built.resolve(),current.resolve())
   out=io.StringIO()
   with patch("meleemod.cli.find_dolphin",return_value=Path("/usr/bin/dolphin-emu")), contextlib.redirect_stdout(out): code=cli.main(["--root",td,"--data",str(t/"data"),"launch","iso-profile","--no-build","--dry-run"])
   self.assertEqual(code,0); planned=json.loads(out.getvalue())["command"][2]; self.assertTrue(planned.endswith("game.iso")); self.assertTrue(Path(planned).is_file()); self.assertEqual(Path(planned).resolve(),built.resolve())

 def test_iso_static_profile_recomposes_staged_output(self):
  if not ISO.exists(): self.skipTest("local user ISO unavailable")
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); data={"id":"code-iso","name":"Code ISO","game_version":"GALE01-1.02","base_game":str(ISO),"plugins":[],"mods":[],"mode":"offline","online_safe":False,"decomp_repo":"repo","decomp_orig":"orig","plugin_source_root":"plugins","runtime_root":"runtime"}; plugin={"id":"p","version":"1.0.0","api_version":1,"entrypoint":"plugin_init","source":"p.c","static_signature":"context"}; profile=__import__("meleemod.profile",fromlist=["ResolvedProfile"]).ResolvedProfile(data,(plugin,),(),"offline-gameplay",t/"profile.json")
   def fake_build(*args,**kwargs): Path(args[3]).write_bytes(b"dol"); return SimpleNamespace(sha1="deadbeef")
   def fake_recompose(base,dol,output): Path(output).write_bytes(b"recomposed")
   with patch("meleemod.store.build_in_worktree",side_effect=fake_build), patch("meleemod.store.recompose_iso",side_effect=fake_recompose): result=BuildStore(t/"data").build(profile)
   self.assertFalse(result.output.is_symlink()); self.assertEqual(result.output.read_bytes(),b"recomposed"); self.assertEqual(json.loads(result.metadata.read_text())["plugin_composition"],"static-source-overlay+iso-recomposition")
 def test_iso_build_uses_safe_reference_not_copy(self):
  if not ISO.exists(): self.skipTest("local user ISO unavailable")
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"profiles").mkdir(); (t/"profiles/v.json").write_text(json.dumps({"id":"vv","name":"Vanilla","game_version":"GALE01-1.02","base_game":str(ISO),"plugins":[],"mods":[],"mode":"vanilla","online_safe":True})); p=load_profile(t/"profiles/v.json",t); r=BuildStore(t/"data").build(p); self.assertTrue(r.output.is_symlink()); self.assertEqual(r.output.resolve(),ISO.resolve())

 def test_character_zip_rejects_duplicate_members(self):
  with tempfile.TemporaryDirectory() as td:
   import zipfile
   z=Path(td)/"bad.melee-character"
   with zipfile.ZipFile(z,"w") as out: out.writestr("character.json",b"{}"); out.writestr("character.json",b"{}")
   with self.assertRaises(ManifestError): validate_package(z)

 def test_bridge_frame_round_trip_and_rejects_corruption(self):
  raw=encode(Message(2,17,b"heartbeat")); self.assertEqual(decode(raw),Message(2,17,b"heartbeat")); bad=bytearray(raw); bad[-1]^=1
  with self.assertRaises(ValueError): decode(bytes(bad))
  with self.assertRaises(ValueError): encode(Message(1,1,b"x"*(64*1024+1)))

 def test_diagnostics_redact_user_game_path(self):
  r=report("offline","abc",[{"id":"p","version":"1.0.0"}],1,"failed /home/dyland/secret.iso"); self.assertNotIn("/home/dyland",json.dumps(r)); self.assertIn("<GAME_DATA>",r["error"])

 def test_local_registry_install_verifies_hash(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); package=t/"plugin.bin"; package.write_bytes(b"plugin"); digest=hashlib.sha256(b"plugin").hexdigest(); entry={"id":"test-plugin","version":"1.0.0","source":str(package),"sha256":digest,"license":"MIT","compatibility":"online-safe","dependencies":[],"maintainer":"test"}; target=install_local(entry,t/"registry"); self.assertEqual((target/"package").read_bytes(),b"plugin")

 def test_static_bundle_overlay_is_deterministic_and_has_hook(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"src/melee/gm").mkdir(parents=True); (t/"plugin.c").write_text("void plugin_init(void) {}\nvoid plugin_shutdown(void) {}\n"); (t/"src/melee/gm/gmmain.c").write_text("int main(void)\n{\n    char* unused;\n    u32 _[2];\n    OSReport(\"#\\n\\n\");\n    gm_801A4510();\n    return 0;\n}\n"); (t/"src/melee/gm/gm_1A45.c").write_text("void frame(void) {\n        lb_800195D0();\n\n        if (HSD_PadGetResetSwitch()) {\n        }\n}\n"); plugins=[{"id":"boot-log","entrypoint":"plugin_init","shutdown":"plugin_shutdown","source":"plugin.c"}]; first=make_bundle(plugins,t); second=make_bundle(plugins,t); self.assertEqual(first,second); apply_overlay(t,plugins,t,runtime_root=Path(__file__).parents[2]); result=(t/"src/melee/gm/gmmain.c").read_text(); loop=(t/"src/melee/gm/gm_1A45.c").read_text(); bundle=(t/"src/melee/gm/meleemod_static_bundle.c").read_text(); self.assertIn("mm_meleemod_static_init",result); self.assertIn("mm_meleemod_static_shutdown",result); self.assertIn("plugin_shutdown",bundle); self.assertIn("mm_meleemod_frame();",loop); self.assertIn("mm_input_history_push",bundle); self.assertIn("mm_meleemod_frame_init",bundle); self.assertIn("if (mm_meleemod_shutdown_done) return;",bundle); self.assertTrue((t/"src/melee/gm/meleemod_static_bundle.c").exists())



 def test_symbolizer_rejects_bad_address_and_redacts_missing_elf(self):
  result=symbolize_native("/definitely/missing.dol",["0x10","not-an-address"])
  self.assertEqual(result[0]["error"],"ELF not found"); self.assertEqual(result[1]["error"],"ELF not found")

 def test_iso_recomposer_shifts_fst_and_files_without_mutating_source(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); base=t/"base.iso"; out=t/"out.iso"; dol=t/"new.dol"; raw=bytearray(0x900); raw[0x420:0x424]=(0x100).to_bytes(4,"big"); raw[0x424:0x428]=(0x300).to_bytes(4,"big"); raw[0x428:0x42c]=(0x30).to_bytes(4,"big")
   # root directory, one directory, and two files.
   raw[0x300:0x30c]=bytes([1,0,0,0])+ (0).to_bytes(4,"big")+(4).to_bytes(4,"big")
   raw[0x30c:0x318]=bytes([1,0,0,0])+ (0).to_bytes(4,"big")+(4).to_bytes(4,"big")
   raw[0x318:0x324]=bytes([0,0,0,0])+ (0x500).to_bytes(4,"big")+(4).to_bytes(4,"big")
   raw[0x324:0x330]=bytes([0,0,0,0])+ (0x600).to_bytes(4,"big")+(4).to_bytes(4,"big")
   raw[0x500:0x504]=b"file"; raw[0x600:0x604]=b"data"; base.write_bytes(raw); dol.write_bytes(b"D"*0x280); recompose_iso(base,dol,out)
   self.assertEqual(base.read_bytes(),bytes(raw)); self.assertEqual(out.read_bytes()[0x100:0x380],b"D"*0x280); self.assertEqual(out.read_bytes()[0x580:0x584],b"file"); self.assertEqual(int.from_bytes(out.read_bytes()[0x424:0x428],"big"),0x380)

 def test_dolphin_smoke_hard_timeout(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); fake=t/"dolphin"; fake.write_text("#!/bin/sh\nsleep 30\n"); fake.chmod(0o755); result=run_one(fake,t/"game.iso",0.05); self.assertTrue(result["timed_out"]); self.assertTrue(result["started"]); self.assertIsNotNone(result["exit_code"])

 def test_character_package_staging_is_offline_and_non_game(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"pkg").mkdir(); (t/"pkg/assets").mkdir(); (t/"pkg/character.json").write_text(json.dumps({"id":"clone","display_name":"Clone","version":"1.0.0","author":"test","license":"CC0","target_game_version":"GALE01-1.02","compatibility":"offline-gameplay"})); (t/"pkg/moveset.json").write_text(json.dumps({"moves":[]})); (t/"pkg/assets/model.bin").write_bytes(b"model"); (t/"pkg/checksums.json").write_text(json.dumps({"assets/model.bin":hashlib.sha256(b"model").hexdigest()})); out=compose_validated_package(t/"pkg",t/"stage","offline"); self.assertTrue((out/"character.json").exists()); self.assertIn("\"game_integration\": false",(out/"staging.json").read_text())

 def test_signed_registry_index_and_key_rotation(self):
  root_seed=bytes.fromhex("01"*32); next_seed=bytes.fromhex("02"*32)
  root_public=public_key(root_seed); next_public=public_key(next_seed)
  root_id=key_id(root_public); next_id=key_id(next_public)
  trust=make_trust({root_id:{"public_key":root_public.hex(),"status":"trusted"}})
  entries=[{"id":"signed-plugin","version":"1.0.0","source":"https://example.invalid/plugin","sha256":"0"*64,"license":"MIT","compatibility":"offline-only","dependencies":[],"maintainer":"test"}]
  signed=sign_index(entries,root_seed)
  self.assertEqual(verify_index(signed,trust),entries)
  raw=json.dumps(signed,sort_keys=True,separators=(",",":")).encode()
  self.assertEqual(fetch_index(lambda limit: raw,trust),entries)
  class Response:
   headers={}
   def __enter__(self): return self
   def __exit__(self,*args): pass
   def read(self,limit): return raw
  with patch("urllib.request.urlopen",return_value=Response()):
   self.assertEqual(fetch_https_index("https://registry.example/index.json",trust),entries)
   cache=Path(tempfile.mkdtemp())/"index.json"
   self.assertEqual(update_https_index("https://registry.example/index.json",cache,trust),cache)
   self.assertEqual(json.loads(cache.read_text()),signed)
  with self.assertRaises(ValueError): fetch_https_index("http://registry.example/index.json",trust)
  tampered=dict(signed); tampered["entries"]=[{"id":"tampered"}]
  with self.assertRaises(ValueError): verify_index(tampered,trust)
  rotated=sign_trust_update({root_id:{"public_key":root_public.hex(),"status":"trusted"},next_id:{"public_key":next_public.hex(),"status":"trusted"}},root_seed)
  new_trust=verify_trust_update(rotated,trust)
  self.assertEqual(verify_index(sign_index(entries,next_seed),new_trust),entries)
  revoked=make_trust({root_id:{"public_key":root_public.hex(),"status":"revoked"}})
  with self.assertRaises(ValueError): verify_index(signed,revoked)

 def test_remote_registry_install_is_https_bounded_and_atomic(self):
  class Response:
   headers={"Content-Length":"6"}
   def __init__(self,data): self.data=data
   def __enter__(self): return self
   def __exit__(self,*args): pass
   def read(self,size): data,self.data=self.data,b""; return data
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); payload=b"plugin"; entry={"id":"remote-plugin","version":"1.0.0","source":"https://registry.example/plugin","sha256":hashlib.sha256(payload).hexdigest(),"license":"MIT","compatibility":"offline-only","dependencies":[],"maintainer":"test"}
   target=install_remote(entry,t/"registry",opener=lambda request,timeout: Response(payload)); self.assertEqual((target/"package").read_bytes(),payload)
   with self.assertRaises(ManifestError): install_remote(dict(entry,sha256="0"*64),t/"registry",opener=lambda request,timeout: Response(payload))
   with self.assertRaises(ManifestError): install_remote(dict(entry,source="http://registry.example/plugin"),t/"registry",opener=lambda request,timeout: Response(payload))

 def test_local_registry_rejects_symlinked_packages(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); source=t/"source"; source.mkdir(); (source/"file.txt").write_text("x"); link=source/"link"
   try: link.symlink_to(source/"file.txt")
   except (OSError,NotImplementedError): self.skipTest("symlinks unavailable")
   entry={"id":"symlinked","version":"1.0.0","source":str(source),"sha256":"0"*64,"license":"MIT","compatibility":"offline-only","dependencies":[],"maintainer":"test"}
   with self.assertRaises(ManifestError): install_local(entry,t/"registry")

 def test_remote_registry_archive_extraction_is_safe_and_bounded(self):
  import io, zipfile
  class Response:
   def __init__(self,data): self.data=data; self.headers={"Content-Length":str(len(data))}
   def __enter__(self): return self
   def __exit__(self,*args): pass
   def read(self,size): data,self.data=self.data,b""; return data
  def archive(name):
   stream=io.BytesIO()
   with zipfile.ZipFile(stream,"w") as zf:
    if name == "duplicate": zf.writestr("a.txt",b"a"); zf.writestr("a.txt",b"b")
    else: zf.writestr(name,b"data")
   return stream.getvalue()
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); safe=archive("assets/model.bin"); entry={"id":"remote-archive","version":"1.0.0","source":"https://registry.example/package.zip","sha256":hashlib.sha256(safe).hexdigest(),"license":"MIT","compatibility":"offline-only","dependencies":[],"maintainer":"test"}
   target=install_remote_archive(entry,t/"registry",opener=lambda request,timeout: Response(safe)); self.assertEqual((target/"assets/model.bin").read_bytes(),b"data")
   unsafe=archive("../escape.bin"); bad=dict(entry,sha256=hashlib.sha256(unsafe).hexdigest())
   with self.assertRaises(ManifestError): install_remote_archive(bad,t/"unsafe",opener=lambda request,timeout: Response(unsafe))
   windows=archive("..\\escape.bin"); win=dict(entry,sha256=hashlib.sha256(windows).hexdigest())
   with self.assertRaises(ManifestError): install_remote_archive(win,t/"windows",opener=lambda request,timeout: Response(windows))
   stream=io.BytesIO(); character={"id":"remote-character","display_name":"Remote","version":"1.0.0","author":"test","license":"MIT","target_game_version":"GALE01-1.02","compatibility":"visual-only"}; moves={"moves":[]}
   with zipfile.ZipFile(stream,"w") as zf:
    zf.writestr("character.json",json.dumps(character)); zf.writestr("moveset.json",json.dumps(moves)); zf.writestr("asset.bin",b"asset"); zf.writestr("checksums.json",json.dumps({"asset.bin":hashlib.sha256(b"asset").hexdigest()}))
   package=stream.getvalue(); pe=dict(entry,id="remote-character",sha256=hashlib.sha256(package).hexdigest())
   installed=install_remote_character_package(pe,t/"character",opener=lambda request,timeout: Response(package)); self.assertEqual((installed/"character.json").exists(),True)
   duplicate=archive("duplicate"); dup=dict(entry,sha256=hashlib.sha256(duplicate).hexdigest())
   with self.assertRaises(ManifestError): install_remote_archive(dup,t/"duplicate",opener=lambda request,timeout: Response(duplicate))

 def test_online_profile_rejects_gameplay_and_unknown_capabilities(self):
  for capability in ("gameplay-changing","unknown","future-capability"):
   effective,errors=validate_safety({"mode":"slippi","online_safe":True},[{"id":"p","capabilities":[capability]}],[]); self.assertNotEqual(effective,"online-safe"); self.assertTrue(errors)
  effective,errors=validate_safety({"mode":"tournament-safe"},[],[{"id":"m","capability":"future-mod-capability"}]); self.assertEqual(effective,"unknown"); self.assertTrue(errors)

 def test_dolphin_smoke_reports_missing_executable(self):
  result=run_one("/definitely/missing/dolphin", "game.iso", 0.1); self.assertFalse(result["started"]); self.assertIn("error",result)

 def test_custom_fighter_assertion_checks_artifacts_and_observation(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); archive=t/"leesin.dat"; data=t/"leesin.bin"; archive.write_bytes(b"archive"); data.write_bytes(b"data")
   result={"output_tail":"custom_fighter_symbol entered\nCUSTOM_FIGHTER_VISIBLE\n"}
   check=custom_fighter_assertion(result,archive,"custom_fighter_symbol",data,"CUSTOM_FIGHTER_VISIBLE")
   self.assertTrue(check["passed"]); self.assertEqual(set(check["checks"]),{"archive_exists","data_exists","symbol_observed","in_game_observed"})
   self.assertFalse(custom_fighter_assertion(result,archive,"missing_symbol")["passed"])

 def test_static_startup_phase_is_explicit(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"plugin.c").write_text("void plugin_init(void) {}\n"); bundle=make_bundle([{"id":"p","entrypoint":"plugin_init","source":"plugin.c","init_phase":"startup"}],t); self.assertIn("mm_meleemod_startup_init",bundle); self.assertIn("plugin_init();",bundle)


class BridgeTransportTests(unittest.TestCase):
 def test_socket_pair_round_trip_and_handler(self):
  import socket
  from meleemod.bridge_transport import receive, send, serve_once
  left,right=socket.socketpair()
  try:
   send(left,Message(1,8,b"ping")); self.assertEqual(serve_once(right,lambda m: Message(2,m.request_id,b"pong")),Message(2,8,b"pong")); self.assertEqual(receive(left),Message(2,8,b"pong"))
  finally: left.close(); right.close()
 def test_dolphin_gdb_memory_adapter_is_bounded(self):
  import socket, threading
  left,right=socket.socketpair(); commands=[]; errors=[]
  def server():
   try:
    for response in (b"T05",b"00112233",b"OK"):
     while right.recv(1) != b"$": pass
     data=b"$"
     while len(data) < 2 or data[-1:] != b"#": data += right.recv(1)
     data += right.recv(2); commands.append(data)
     packet=b"$"+response+b"#"+f"{sum(response)&255:02x}".encode()
     right.sendall(b"+"+packet)
     right.recv(1)
   except Exception as exc: errors.append(exc)
   finally: right.close()
  thread=threading.Thread(target=server); thread.start()
  client=DolphinGdbClient(left,timeout=2)
  try:
   self.assertEqual(client.stop_reason(),b"T05")
   self.assertEqual(client.read_memory(0x804eec00,4),b"\x00\x11\x22\x33")
   client.write_memory(0x804eec00,b"\x01\x02")
   self.assertTrue(commands[1].startswith(b"$m804eec00,4#")); self.assertTrue(commands[2].startswith(b"$M804eec00,2:0102#"))
   with self.assertRaises(ValueError): client.read_memory(0,MAX_TRANSFER+1)
  finally:
   client.close(); thread.join(2); self.assertEqual(errors,[])

 def test_dolphin_gdb_mailbox_publishes_after_payload(self):
  class Memory:
   def __init__(self): self.raw=bytearray(16+2*RUNTIME_FRAME_CAPACITY); self.raw[:8]=b"MMBX\0\0\0\1"
   def read_memory(self,address,size): return bytes(self.raw[address:address+size])
   def write_memory(self,address,data): self.raw[address:address+len(data)]=data
  memory=Memory(); mailbox=DolphinGdbMailbox(memory,0); mailbox.send(b"frame")
  self.assertEqual(memory.raw[16:21],b"frame"); self.assertEqual(memory.raw[8:12],b"\0\0\0\5")
  memory.raw[12:16]=(5).to_bytes(4,"big"); memory.raw[16+RUNTIME_FRAME_CAPACITY:21+RUNTIME_FRAME_CAPACITY]=b"reply"
  self.assertEqual(mailbox.receive(),b"reply"); self.assertEqual(memory.raw[12:16],b"\0\0\0\0")
  memory.raw[12:16]=(1).to_bytes(4,"big")
  with self.assertRaises(DolphinGdbError): mailbox.send(b"again")

 def test_disconnect_is_bounded_error(self):
  import socket
  from meleemod.bridge_transport import BridgeTransportError, receive
  left,right=socket.socketpair(); right.close()
  try:
   with self.assertRaises(BridgeTransportError): receive(left)
  finally: left.close()

 def test_unix_bridge_server_is_private_and_cleans_up(self):
  import socket, threading
  from meleemod.bridge_transport import receive, send
  with tempfile.TemporaryDirectory() as td:
   path=Path(td)/"bridge.sock"; result=[]; ready=threading.Event()
   def serve():
    with UnixBridgeServer(path,timeout=2) as server:
     ready.set(); result.append(server.serve_once(lambda m: Message(2,m.request_id,b"ack")))
   thread=threading.Thread(target=serve); thread.start(); self.assertTrue(ready.wait(2))
   client=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM); client.connect(str(path)); send(client,Message(1,3,b"hello")); self.assertEqual(receive(client),Message(2,3,b"ack")); client.close(); thread.join(2)
   self.assertEqual(result,[Message(2,3,b"ack")]); self.assertFalse(path.exists())

class GuiTests(unittest.TestCase):
 def test_controller_lists_invalid_profiles_without_tk(self):
  from meleemod.gui import GuiController
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"profiles").mkdir(); (t/"profiles/bad.json").write_text("{not json")
   rows=GuiController(t,t/"data").list_profiles()
   self.assertEqual(len(rows),1); self.assertEqual(rows[0].id,"bad"); self.assertEqual(rows[0].compatibility,"invalid")
 def test_controller_rejects_profile_path_traversal(self):
  from meleemod.gui import GuiController
  with tempfile.TemporaryDirectory() as td:
   with self.assertRaises(ValueError): GuiController(td)._path("../bad")
 def test_profile_mod_manager_toggles_catalog_entry(self):
  from meleemod.mods import catalog, resolve, set_enabled
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); (t/"profiles").mkdir(); (t/"plugins/demo").mkdir(parents=True)
   manifest={"id":"demo","version":"1.0.0","api_version":1,"entrypoint":"plugin_init","capabilities":["visual-only"],"dependencies":[],"game_versions":["GALE01-1.02"],"online_safe":True}
   (t/"plugins/demo/plugin.json").write_text(json.dumps(manifest))
   profile={"id":"p","name":"P","game_version":"GALE01-1.02","base_game":"base.iso","plugins":[],"mods":[],"mode":"offline","online_safe":False}
   (t/"profiles/p.json").write_text(json.dumps(profile))
   self.assertEqual(sorted(catalog(t)),["demo"]); self.assertEqual(resolve(t,"p")[1],set())
   result=set_enabled(t,"p","demo",True); self.assertIn("+ demo",result["changed"])
   self.assertEqual(resolve(t,"p")[1],{"demo"})
   set_enabled(t,"p","demo",False); self.assertEqual(resolve(t,"p")[1],set())

if __name__=="__main__": unittest.main()
