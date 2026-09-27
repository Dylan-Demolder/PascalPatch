import os
import json, tempfile, unittest, sys
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).parents[1]/"../host/src"))
from pascalpatch.discovery import _looks_like_emulator
sys.path.insert(0,str(Path(__file__).parents[1]))
from dolphin_smoke import custom_fighter_assertion, parse_playback_evidence, read_dtm_metadata, run_one
class ToolTests(unittest.TestCase):
 def test_no_false_positive_for_arbitrary_script(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/"fake"; p.write_text("#!/bin/sh\necho hello\n"); p.chmod(0o755); self.assertFalse(_looks_like_emulator(p))
 def test_marker_checks_are_opt_in(self):
  result=custom_fighter_assertion({"output_tail":""})
  self.assertFalse(result["enabled"])
  self.assertFalse(result["passed"])
 def test_requested_marker_must_be_emitted(self):
  result=custom_fighter_assertion({"output_tail":""}, character_select="CHARACTER_SELECT_COMPLETE")
  self.assertTrue(result["enabled"])
  self.assertFalse(result["checks"]["character_select_complete"])
  self.assertFalse(result["passed"])
 def test_input_automation_marker_requires_dolphin_output(self):
  result=custom_fighter_assertion({"output_tail":""}, input_automation_ready="INPUT_AUTOMATION_READY")
  self.assertFalse(result["checks"]["input_automation_ready"])
  result=custom_fighter_assertion({"output_tail":"INPUT_AUTOMATION_READY"}, input_automation_ready="INPUT_AUTOMATION_READY")
  self.assertTrue(result["checks"]["input_automation_ready"])
  self.assertTrue(result["passed"])
 @unittest.skipIf(os.name=="nt","POSIX shell/socket fixture")
 def test_movie_is_forwarded_to_dolphin(self):
  with tempfile.TemporaryDirectory() as td:
   dolphin=Path(td)/"dolphin"; movie=Path(td)/"input.dtm"
   dolphin.write_text("#!/bin/sh\nprintf '%s\\n' \"$@\"\n")
   dolphin.chmod(0o755); movie.write_bytes(b"DTM")
   result=run_one(dolphin,Path(td)/"game.iso",1,movie)
   self.assertIn(f"-m\n{movie}",result["output_tail"])
   self.assertIsNotNone(result["movie_error"])
 def test_raw_gale01_dtm_passes_metadata(self):
  from author_dtm import build_dtm
  with tempfile.TemporaryDirectory() as td:
   movie=Path(td)/"menu.dtm"; movie.write_bytes(build_dtm())
   meta=read_dtm_metadata(movie)
   self.assertEqual(meta["game_id"],"GALE01"); self.assertTrue(meta["frame_count"]>0)
   with self.assertRaises(ValueError): read_dtm_metadata(Path(td)/"missing.dtm")
   bad=Path(td)/"wrapped.dtm"; bad.write_bytes(b"PK\x03\x04"+b"\x00"*300)
   with self.assertRaises(ValueError): read_dtm_metadata(bad)
 def test_empty_tail_is_diagnostic_failure(self):
  evidence=parse_playback_evidence("")
  self.assertFalse(evidence["movie_playback_entered"])
  self.assertIsNone(evidence["last_scene"])
  self.assertEqual(evidence["output_len"],0)
 def test_playback_evidence_tracks_movie_and_scene(self):
  evidence=parse_playback_evidence("Playing movie input.dtm\n[pascalpatch] GM:28 SC:00\n[pascalpatch] GM:02 SC:01")
  self.assertTrue(evidence["movie_playback_entered"])
  self.assertEqual(evidence["last_scene"],"GM:02 SC:01")
  self.assertTrue(evidence["advanced_past_boot"])
  boot_only=parse_playback_evidence("[pascalpatch] GM:28 SC:00")
  self.assertFalse(boot_only["advanced_past_boot"])


class FakeGdbClient:
 """Fake standing in for DolphinGdbClient: answers read_memory() from a flat
 dict of {address: bytes} instead of a real socket, so verify_fighters'
 pointer-walking logic can be tested without a running Dolphin."""
 class _FakeSocket:
  def settimeout(self,_): pass
  def recv(self,_): return b""  # nothing queued to drain
 def __init__(self, memory):
  self.memory=memory; self.timeout=5.0; self.sock=self._FakeSocket()
  self.interrupt_calls=0; self.continue_calls=0
 def interrupt(self):
  self.interrupt_calls+=1; return b""
 def continue_execution(self):
  self.continue_calls+=1
 def read_memory(self,address,size):
  data=self.memory[address]
  if len(data)!=size: raise AssertionError(f"unexpected read size at {address:#x}: wanted {size}, fixture has {len(data)}")
  return data


class VerifyFightersTests(unittest.TestCase):
 def test_walks_mixed_roster_and_always_resumes(self):
  from verify_fighters import read_fighters, FIGHTER_LIST_BASE, LIST_HEAD_OFFSET, NEXT_OFFSET, FIGHTER_PTR_OFFSET, KIND_OFFSET, WEIGHT_OFFSET
  import struct
  list_head=0x80500000; node_a=0x80600000; node_b=0x80600100
  fighter_a=0x80700000; fighter_b=0x80700200
  memory={
   FIGHTER_LIST_BASE: list_head.to_bytes(4,"big"),
   list_head+LIST_HEAD_OFFSET: node_a.to_bytes(4,"big"),
   node_a+FIGHTER_PTR_OFFSET: fighter_a.to_bytes(4,"big"),
   node_a+NEXT_OFFSET: node_b.to_bytes(4,"big"),
   node_b+FIGHTER_PTR_OFFSET: fighter_b.to_bytes(4,"big"),
   node_b+NEXT_OFFSET: (0).to_bytes(4,"big"),
   fighter_a+KIND_OFFSET: (1).to_bytes(4,"big"),
   fighter_a+WEIGHT_OFFSET: struct.pack(">f",75.0),
   fighter_b+KIND_OFFSET: (20).to_bytes(4,"big"),
   fighter_b+WEIGHT_OFFSET: struct.pack(">f",999.0),
  }
  client=FakeGdbClient(memory)
  fighters=read_fighters(client)
  self.assertEqual(len(fighters),2)
  self.assertEqual(fighters[0]["kind"],1); self.assertAlmostEqual(fighters[0]["weight"],75.0,places=3)
  self.assertEqual(fighters[0]["kind_name"],"Fox")
  self.assertEqual(fighters[1]["kind"],20); self.assertAlmostEqual(fighters[1]["weight"],999.0,places=3)
  self.assertEqual(fighters[1]["kind_name"],"kind_20")
  self.assertEqual(client.interrupt_calls,1); self.assertEqual(client.continue_calls,1)
 def test_empty_list_still_resumes_execution(self):
  from verify_fighters import read_fighters, FIGHTER_LIST_BASE, LIST_HEAD_OFFSET
  list_head=0x80500000
  memory={FIGHTER_LIST_BASE: list_head.to_bytes(4,"big"), list_head+LIST_HEAD_OFFSET: (0).to_bytes(4,"big")}
  client=FakeGdbClient(memory)
  self.assertEqual(read_fighters(client),[])
  self.assertEqual(client.continue_calls,1)
 def test_resumes_execution_even_on_read_error(self):
  from verify_fighters import read_fighters
  client=FakeGdbClient({})  # missing FIGHTER_LIST_BASE -> KeyError inside try/finally
  with self.assertRaises(KeyError):
   read_fighters(client)
  self.assertEqual(client.continue_calls,1,"must resume the CPU even when the read fails")


class PrimeMemcardTests(unittest.TestCase):
 def test_refuses_to_write_inside_a_git_repo(self):
  from prime_memcard import _refuses_repo_path
  with tempfile.TemporaryDirectory() as td:
   repo=Path(td)/"repo"; (repo/".git").mkdir(parents=True)
   self.assertTrue(_refuses_repo_path(repo/"nested"/"seed"))
   self.assertFalse(_refuses_repo_path(Path(td)/"outside-any-repo"))
 def test_picks_the_render_window_not_a_helper_window(self):
  from prime_memcard import _find_window
  search_result=type("R",(),{"stdout":"25165830\n25165833\n25165835\n25165836\n","returncode":0})()
  names={
   "25165830":"Qt Selection Owner for dolphin-emu",
   "25165833":"Dolphin 2606",
   "25165835":"dolphin-emu",
   "25165836":"Dolphin 2606 | JIT64 SC | OpenGL | HLE | Super Smash Bros. Melee (GALE01)",
  }
  def fake_run(cmd,**kwargs):
   if cmd[:2]==["xdotool","search"]:
    return search_result
   window_id=cmd[-1]
   return type("R",(),{"stdout":names[window_id],"returncode":0})()
  with patch("prime_memcard.subprocess.run",side_effect=fake_run):
   window_id=_find_window(":0",timeout=1.0)
  self.assertEqual(window_id,"25165836","must pick the window whose title has pipe-separated core/backend/game info, not a helper window with the same substring match")
 def test_find_window_times_out_when_none_match(self):
  from prime_memcard import _find_window
  def fake_run(cmd,**kwargs):
   if cmd[:2]==["xdotool","search"]:
    return type("R",(),{"stdout":"","returncode":0})()
   return type("R",(),{"stdout":"","returncode":0})()
  with patch("prime_memcard.subprocess.run",side_effect=fake_run):
   with self.assertRaises(SystemExit):
    _find_window(":0",timeout=0.1)


if __name__=="__main__":unittest.main()
