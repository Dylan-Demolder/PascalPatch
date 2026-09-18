import json, tempfile, unittest, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"../host/src"))
from meleemod.discovery import _looks_like_emulator
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
   evidence=parse_playback_evidence("Playing movie input.dtm\n[meleemod] GM:28 SC:00\n[meleemod] GM:02 SC:01")
   self.assertTrue(evidence["movie_playback_entered"])
   self.assertEqual(evidence["last_scene"],"GM:02 SC:01")
   self.assertTrue(evidence["advanced_past_boot"])
   boot_only=parse_playback_evidence("[meleemod] GM:28 SC:00")
   self.assertFalse(boot_only["advanced_past_boot"])
if __name__=="__main__":unittest.main()
