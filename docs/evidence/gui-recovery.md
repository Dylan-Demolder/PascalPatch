# Tk GUI recovery observation

Date: 2026-09-14. This is a bounded X11 check using the locally extracted Tk/Tcl libraries. It does not launch Dolphin or use game data.

The real `tooling/meleemod_gui.py` window launched with:

```text
LD_LIBRARY_PATH=/tmp/tk-standalone/usr/lib
TK_LIBRARY=/tmp/tk-standalone/usr/lib/tk8.6
TCL_LIBRARY=/tmp/tk-standalone/usr/share/tcl8.6
```

A temporary project contained an intentionally invalid profile. X11 automation selected the profile and activated `Validate`. The resulting error dialog was dismissed with Escape, and `xdotool getactivewindow` returned the original `MeleeMod` window. The main window therefore remained alive after a validation failure. The process was then stopped by the bounded timeout with no orphan GUI process.

This proves basic error-dialog recovery. It does not prove every build/launch failure path or a full interactive production workflow.
