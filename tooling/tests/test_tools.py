import json, tempfile, unittest, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"../host/src"))
from meleemod.discovery import _looks_like_emulator
sys.path.insert(0,str(Path(__file__).parents[1]))
from dolphin_smoke import custom_fighter_assertion, run_one
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
 def test_input_automation_marker_is_checked(self):
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
if __name__=="__main__":unittest.main()
