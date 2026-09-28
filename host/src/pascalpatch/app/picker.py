"""Show a native Open dialog and print the chosen path (empty when cancelled).

Run in its own process by the app (``python picker.py <kind> <title> <start>``), so the dialog never
shares a thread with the HTTP server. Uses Tk when Python has it; the Python bundled in a release
download has no Tk, so on Windows it falls back to the system's own dialog through PowerShell.
"""
import os
import subprocess
import sys

FILETYPES = {
    "exe": [("Melee Unlocked", "melee_port*.exe"), ("Programs", "*.exe")],
    "iso": [("GameCube disc image", "*.iso *.gcm"), ("All files", "*.*")],
    "zip": [("PascalPatch plugin", "*.zip"), ("All files", "*.*")],
}

# Title, start folder and filter come in through the environment, so nothing needs quoting.
POWERSHELL = r"""
Add-Type -AssemblyName System.Windows.Forms
[Console]::OutputEncoding = [Text.Encoding]::UTF8
$owner = New-Object System.Windows.Forms.Form -Property @{ TopMost = $true; ShowInTaskbar = $false }
if ($env:PP_PICK_KIND -eq 'folder') {
  $d = New-Object System.Windows.Forms.FolderBrowserDialog
  $d.Description = $env:PP_PICK_TITLE
  if ($env:PP_PICK_START) { $d.SelectedPath = $env:PP_PICK_START }
  if ($d.ShowDialog($owner) -eq 'OK') { [Console]::Out.Write($d.SelectedPath) }
} else {
  $d = New-Object System.Windows.Forms.OpenFileDialog
  $d.Title = $env:PP_PICK_TITLE
  $d.Filter = $env:PP_PICK_FILTER
  if ($env:PP_PICK_START) { $d.InitialDirectory = $env:PP_PICK_START }
  if ($d.ShowDialog($owner) -eq 'OK') { [Console]::Out.Write($d.FileName) }
}
"""


def pick_tk(kind, title, start):
    import tkinter as tk
    from tkinter import filedialog
    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)   # in front of the app window, not behind it
    if kind == "folder":
        path = filedialog.askdirectory(parent=root, title=title, initialdir=start or None, mustexist=True)
    else:
        path = filedialog.askopenfilename(parent=root, title=title, initialdir=start or None,
                                          filetypes=FILETYPES.get(kind, [("All files", "*.*")]))
    root.destroy()
    return path


def pick_windows(kind, title, start):
    # Windows' filter syntax: "Name|*.a;*.b|Name|*.c"
    filt = "|".join(f"{name}|{pattern.replace(' ', ';')}" for name, pattern in FILETYPES.get(kind, [("All files", "*.*")]))
    env = dict(os.environ, PP_PICK_KIND=kind, PP_PICK_TITLE=title, PP_PICK_START=start, PP_PICK_FILTER=filt)
    r = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-STA", "-Command", POWERSHELL],
                       env=env, capture_output=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return r.stdout.decode("utf-8-sig", "replace").strip()


def main():
    kind, title, start = (sys.argv[1:] + ["", "", ""])[:3]
    try:
        import tkinter  # noqa: F401
    except ImportError:
        if os.name != "nt":
            raise
        path = pick_windows(kind, title, start)
    else:
        path = pick_tk(kind, title, start)
    sys.stdout.write(path or "")


if __name__ == "__main__":
    main()
