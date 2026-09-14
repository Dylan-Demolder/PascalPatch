import hashlib, json, shutil, tempfile, unittest
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).parents[1]/"src"))
from meleemod.discovery import inspect_iso, inspect_game, EXPECTED_DOL_SHA1
from meleemod.manifest import validate_profile, validate_plugin, validate_mod
from meleemod.profile import load_profile
from meleemod.store import BuildStore
from meleemod.errors import ManifestError, CompositionError, DiscoveryError
from meleemod.character_package import validate_package, installation_plan
from meleemod.plugin_composer import resolve_plugin_order, compose_static_manifest
from meleemod.launcher import launch
from meleemod.bridge import Message, encode, decode
from meleemod.diagnostics import report
from meleemod.registry import install_local
from meleemod.static_integration import make_bundle, apply_overlay

ISO=Path("/home/dyland/Downloads/Super Smash Bros. Melee (USA) (En,Ja) (v1.02).iso")
DOL=Path("/home/dyland/Documents/MeleeDecomp/melee/build/GALE01/main.dol")
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

 def test_plugin_order_and_cycle_rejection(self):
  def p(ident,deps): return {"id":ident,"version":"1.0.0","api_version":1,"entrypoint":"plugin_init","capabilities":["visual-only"],"dependencies":deps,"game_versions":["GALE01-1.02"],"online_safe":True}
  ordered=resolve_plugin_order([p("bb",["aa"]),p("aa",[])]); self.assertEqual([x["id"] for x in ordered],["aa","bb"]); self.assertEqual(compose_static_manifest(ordered)["link_status"],"deferred-until-runtime-integration")
  with self.assertRaises(Exception): resolve_plugin_order([p("aa",["bb"]),p("bb",["aa"])])

 def test_launch_helper_records_exit(self):
  with tempfile.TemporaryDirectory() as td:
   t=Path(td); fake=t/"emu"; fake.write_text("#!/bin/sh\necho booted\nexit 7\n"); fake.chmod(0o755); r=launch(fake,"game.iso",t/"run.log",wait=True,timeout=2); self.assertEqual(r.exit_code,7); self.assertIn("booted",(t/"run.log").read_text()); self.assertIn("exit_code: 7",(t/"run.log").read_text())

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
   t=Path(td); (t/"src/melee/gm").mkdir(parents=True); (t/"plugin.c").write_text("void plugin_init(void) {}\n"); (t/"src/melee/gm/gmmain.c").write_text("int main(void)\n{\n    char* unused;\n    u32 _[2];\n    OSInit();\n    return 0;\n}\n"); plugins=[{"id":"boot-log","entrypoint":"plugin_init","source":"plugin.c"}]; first=make_bundle(plugins,t); second=make_bundle(plugins,t); self.assertEqual(first,second); apply_overlay(t,plugins,t); result=(t/"src/melee/gm/gmmain.c").read_text(); self.assertIn("mm_meleemod_static_init",result); self.assertTrue((t/"src/melee/gm/meleemod_static_bundle.c").exists())

if __name__=="__main__": unittest.main()
