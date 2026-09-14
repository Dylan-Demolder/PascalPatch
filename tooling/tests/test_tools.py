import json, tempfile, unittest, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parents[1]/"../host/src"))
from meleemod.discovery import _looks_like_emulator
class ToolTests(unittest.TestCase):
 def test_no_false_positive_for_arbitrary_script(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/"fake"; p.write_text("#!/bin/sh\necho hello\n"); p.chmod(0o755); self.assertFalse(_looks_like_emulator(p))
if __name__=="__main__":unittest.main()
