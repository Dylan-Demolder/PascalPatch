import json, tempfile, unittest, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"../host/src"))
from meleemod.discovery import _looks_like_emulator
sys.path.insert(0,str(Path(__file__).parents[1]))
from dolphin_smoke import custom_fighter_assertion
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
 if __name__=="__main__":unittest.main()

