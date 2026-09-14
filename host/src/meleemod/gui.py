from __future__ import annotations
import datetime
import json
from dataclasses import dataclass
from pathlib import Path
from .discovery import find_dolphin
from .launcher import launch
from .profile import load_profile
from .store import BuildStore

@dataclass(frozen=True)
class ProfileRow:
    id: str
    name: str
    mode: str
    compatibility: str

class GuiController:
    """GUI-safe facade over profile validation, builds, launches and logs.

    The facade contains no Tk calls, so it is testable in a headless CI runner.
    """
    def __init__(self, project_root, data_root=None):
        self.project_root = Path(project_root).expanduser().resolve()
        self.store = BuildStore(data_root)

    def _path(self, profile_id):
        if not profile_id or Path(profile_id).name != profile_id or Path(profile_id).suffix:
            raise ValueError("invalid profile ID")
        return self.project_root / "profiles" / (profile_id + ".json")

    def list_profiles(self):
        rows=[]
        for path in sorted((self.project_root/"profiles").glob("*.json")):
            try:
                data=json.loads(path.read_text(encoding="utf-8"))
                profile=load_profile(path,self.project_root)
                rows.append(ProfileRow(data.get("id",path.stem),data.get("name","?"),data.get("mode","?"),profile.compatibility))
            except Exception:
                rows.append(ProfileRow(path.stem,"INVALID","?","invalid"))
        return rows

    def validate(self, profile_id):
        profile=load_profile(self._path(profile_id),self.project_root)
        return {"id":profile.data["id"],"game_version":profile.data["game_version"],"compatibility":profile.compatibility,"plugins":[p["id"] for p in profile.plugins],"mods":[m["id"] for m in profile.mods]}

    def build(self, profile_id):
        return self.store.build(load_profile(self._path(profile_id),self.project_root))

    def launch(self, profile_id, dolphin=None, allow_unsafe=False, wait=False, timeout=None):
        profile=load_profile(self._path(profile_id),self.project_root)
        if profile.compatibility != "online-safe" and not allow_unsafe:
            raise ValueError(f"profile is {profile.compatibility}; enable unsafe launch explicitly")
        result=self.build(profile_id)
        executable=find_dolphin(dolphin)
        logdir=self.store.root/"logs"/profile_id; logdir.mkdir(parents=True,exist_ok=True)
        log=logdir/(datetime.datetime.now().strftime("%Y%m%dT%H%M%S")+".log")
        return launch(executable,result.output,log,wait=wait,timeout=timeout)

    def logs(self, profile_id):
        directory=self.store.root/"logs"/profile_id
        return sorted(directory.glob("*.log")) if directory.exists() else []

def run(project_root, data_root=None, self_test=False):
    # Import Tk only when the actual GUI is requested.
    import tkinter as tk
    from tkinter import messagebox
    controller=GuiController(project_root,data_root)
    app=tk.Tk(); app.title("MeleeMod"); app.geometry("760x480")
    selected=tk.StringVar(); allow=tk.BooleanVar(value=False)
    left=tk.Frame(app); left.pack(side="left",fill="y",padx=8,pady=8)
    right=tk.Frame(app); right.pack(side="right",fill="both",expand=True,padx=8,pady=8)
    tk.Label(left,text="Profiles").pack(anchor="w")
    listbox=tk.Listbox(left,width=28,height=18,exportselection=False); listbox.pack(fill="y",expand=True)
    output=tk.Text(right,state="disabled",wrap="word"); output.pack(fill="both",expand=True)
    def write(value):
        output.configure(state="normal"); output.delete("1.0","end"); output.insert("end",value); output.configure(state="disabled")
    def refresh():
        listbox.delete(0,"end")
        for row in controller.list_profiles(): listbox.insert("end",f"{row.id}  [{row.compatibility}]")
    def current():
        if not listbox.curselection(): raise ValueError("select a profile")
        return listbox.get(listbox.curselection()[0]).split("  ",1)[0]
    def action(fn):
        try: write(fn(current()))
        except Exception as exc: messagebox.showerror("MeleeMod",str(exc))
    buttons=tk.Frame(right); buttons.pack(fill="x",pady=(0,6));
    tk.Button(buttons,text="Validate",command=lambda: action(lambda p: json.dumps(controller.validate(p),indent=2))).pack(side="left")
    tk.Button(buttons,text="Build",command=lambda: action(lambda p: str(controller.build(p).output))).pack(side="left")
    tk.Button(buttons,text="Launch",command=lambda: action(lambda p: str(controller.launch(p,allow_unsafe=allow.get())))).pack(side="left")
    tk.Button(buttons,text="Logs",command=lambda: action(lambda p: "\n".join(map(str,controller.logs(p))))).pack(side="left")
    tk.Checkbutton(buttons,text="Allow unsafe/offline",variable=allow).pack(side="right")
    refresh()
    if self_test:
        def automated():
            if not listbox.size():
                raise RuntimeError("GUI self-test requires at least one profile")
            listbox.selection_set(0); listbox.activate(0)
            action(lambda p: json.dumps(controller.validate(p),indent=2))
            action(lambda p: str(controller.build(p).output))
            app.after(100, app.destroy)
        app.after(100, automated)
    app.mainloop()
