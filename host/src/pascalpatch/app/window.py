"""Show a local web UI in a window of its own (shared by PascalPatch and Character Studio).

Order of preference: pywebview when it is installed (a native window), then Microsoft Edge or
Chrome in app mode (no tabs or address bar, a private profile under the data folder), then the
default browser. Returns when the window closes (the browser fallback returns at once and the
caller keeps serving until Ctrl+C).
"""
from __future__ import annotations

import os
import shutil
import subprocess
import time
import webbrowser
from pathlib import Path

APP_BROWSERS = [
    r"%ProgramFiles(x86)%\Microsoft\Edge\Application\msedge.exe",
    r"%ProgramFiles%\Microsoft\Edge\Application\msedge.exe",
    r"%ProgramFiles%\Google\Chrome\Application\chrome.exe",
    r"%LocalAppData%\Google\Chrome\Application\chrome.exe",
]


def _app_browser():
    for p in APP_BROWSERS:
        path = Path(os.path.expandvars(p))
        if path.is_file():
            return str(path)
    return shutil.which("msedge") or shutil.which("chrome") or shutil.which("chromium")


def open_window(url, title, data_dir, size=(1320, 860)):
    try:
        import webview   # optional: pip install pywebview
        webview.create_window(title, url, width=size[0], height=size[1], min_size=(900, 600), background_color="#070B1C")
        webview.start()
        return
    except ImportError:
        pass
    exe = _app_browser()
    if exe:
        profile = Path(data_dir) / "window-profile" / title.lower().replace(" ", "-")
        profile.mkdir(parents=True, exist_ok=True)
        proc = subprocess.Popen([exe, f"--app={url}", f"--user-data-dir={profile}", f"--window-size={size[0]},{size[1]}",
                                 "--no-first-run", "--no-default-browser-check", "--disable-extensions"])
        proc.wait()
        return
    webbrowser.open(url)
    try:
        while True:
            time.sleep(3600)
    except KeyboardInterrupt:
        pass
