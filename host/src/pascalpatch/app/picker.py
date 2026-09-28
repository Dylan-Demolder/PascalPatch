"""Show a native Open dialog and print the chosen path (empty when cancelled).

Run in its own process by the app (``python picker.py <kind> <title> <start>``), so Tk never
shares a thread with the HTTP server.
"""
import sys
import tkinter as tk
from tkinter import filedialog

FILETYPES = {
    "exe": [("Melee Unlocked", "melee_port*.exe"), ("Programs", "*.exe")],
    "iso": [("GameCube disc image", "*.iso *.gcm"), ("All files", "*.*")],
    "zip": [("PascalPatch plugin", "*.zip"), ("All files", "*.*")],
}


def main():
    kind, title, start = (sys.argv[1:] + ["", "", ""])[:3]
    root = tk.Tk()
    root.withdraw()
    root.attributes("-topmost", True)   # in front of the app window, not behind it
    if kind == "folder":
        path = filedialog.askdirectory(parent=root, title=title, initialdir=start or None, mustexist=True)
    else:
        path = filedialog.askopenfilename(parent=root, title=title, initialdir=start or None,
                                          filetypes=FILETYPES.get(kind, [("All files", "*.*")]))
    root.destroy()
    sys.stdout.write(path or "")


if __name__ == "__main__":
    main()
