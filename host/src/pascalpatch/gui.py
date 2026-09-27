from __future__ import annotations
import datetime
import json
from dataclasses import dataclass
from pathlib import Path
from .discovery import find_dolphin
from .launcher import launch
from .profile import load_profile
from .store import BuildStore
from .mods import CatalogError, all_catalog, resolve, set_enabled

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

    def mods(self):
        return all_catalog(self.project_root)

    def profile_mods(self, profile_id):
        entries, enabled, missing = resolve(self.project_root, profile_id)
        roadmap = all_catalog(self.project_root)
        return roadmap, enabled, missing

    def set_mod_enabled(self, profile_id, mod_id, enabled):
        return set_enabled(self.project_root, profile_id, mod_id, enabled)

def run(project_root, data_root=None, dolphin=None, self_test=False, self_test_launch=False):
    """Run the desktop launcher.

    The GUI is intentionally a view over ``GuiController``. Profiles and
    manifests remain plain files, so automation and the CLI use the same
    state as this application.
    """
    import tkinter as tk
    from tkinter import filedialog, messagebox, simpledialog, ttk

    controller=GuiController(project_root,data_root)
    app=tk.Tk(); app.title("PascalPatch"); app.geometry("1220x760"); app.minsize(980,620)
    style=ttk.Style(app)
    try: style.theme_use("clam")
    except tk.TclError: pass
    style.configure("Title.TLabel",font=("TkDefaultFont",18,"bold"))
    style.configure("Subtitle.TLabel",foreground="#667085")
    style.configure("Badge.TLabel",font=("TkDefaultFont",9,"bold"),padding=(6,2))
    style.configure("Card.TLabelframe",padding=8)

    selected_profile=tk.StringVar(); search=tk.StringVar(); category=tk.StringVar(value="All categories"); status_filter=tk.StringVar(value="All")
    allow=tk.BooleanVar(value=False); selected_mod_id=[None]
    profiles_frame=ttk.Frame(app,padding=12); profiles_frame.pack(side="left",fill="y")
    center=ttk.Frame(app,padding=(0,12,12,12)); center.pack(side="left",fill="both",expand=True)
    details=ttk.Frame(app,padding=(0,12,12,12),width=315); details.pack(side="right",fill="y"); details.pack_propagate(False)

    ttk.Label(profiles_frame,text="PascalPatch",style="Title.TLabel").pack(anchor="w")
    ttk.Label(profiles_frame,text="Profiles",style="Subtitle.TLabel").pack(anchor="w",pady=(0,8))
    profile_list=tk.Listbox(profiles_frame,width=28,height=28,exportselection=False,activestyle="dotbox",borderwidth=0,highlightthickness=1)
    profile_list.pack(fill="y",expand=True)
    profile_buttons=ttk.Frame(profiles_frame); profile_buttons.pack(fill="x",pady=(8,0))

    header=ttk.Frame(center); header.pack(fill="x")
    header_left=ttk.Frame(header); header_left.pack(side="left",fill="x",expand=True)
    profile_title=ttk.Label(header_left,text="Select a profile",style="Title.TLabel"); profile_title.pack(anchor="w")
    profile_meta=ttk.Label(header_left,text="",style="Subtitle.TLabel"); profile_meta.pack(anchor="w")
    header_right=ttk.Frame(header); header_right.pack(side="right")
    ttk.Button(header_right,text="Validate",command=lambda: action(lambda p: json.dumps(controller.validate(p),indent=2))).pack(side="left",padx=2)
    ttk.Button(header_right,text="Build",command=lambda: action(lambda p: str(controller.build(p).output))).pack(side="left",padx=2)
    ttk.Button(header_right,text="Launch",command=lambda: action(lambda p: str(controller.launch(p,dolphin=dolphin,allow_unsafe=allow.get())))).pack(side="left",padx=2)
    ttk.Checkbutton(header_right,text="Allow unsafe",variable=allow).pack(side="left",padx=(8,0))

    toolbar=ttk.Frame(center); toolbar.pack(fill="x",pady=(14,8))
    ttk.Label(toolbar,text="Mod catalog",font=("TkDefaultFont",12,"bold")).pack(side="left")
    ttk.Label(toolbar,text="Search").pack(side="left",padx=(18,4))
    ttk.Entry(toolbar,textvariable=search,width=22).pack(side="left")
    ttk.Label(toolbar,text="Category").pack(side="left",padx=(12,4))
    category_box=ttk.Combobox(toolbar,textvariable=category,state="readonly",width=18); category_box.pack(side="left")
    ttk.Label(toolbar,text="Status").pack(side="left",padx=(12,4))
    status_box=ttk.Combobox(toolbar,textvariable=status_filter,state="readonly",width=12,values=("All","available","prototype","planned")); status_box.pack(side="left")

    tree=ttk.Treeview(center,columns=("name","status","category","version","enabled"),show="headings",selectmode="browse")
    for col,label,width in (("name","Mod",220),("status","Status",100),("category","Category",120),("version","Version",90),("enabled","Profile",90)):
        tree.heading(col,text=label); tree.column(col,width=width,anchor="w")
    tree.pack(fill="both",expand=True)
    ttk.Label(center,text="Double-click an installed mod to enable or disable it. Planned entries are roadmap items.",style="Subtitle.TLabel").pack(anchor="w",pady=(6,0))

    ttk.Label(details,text="Mod details",font=("TkDefaultFont",14,"bold")).pack(anchor="w")
    detail_title=ttk.Label(details,text="Choose a mod",font=("TkDefaultFont",12,"bold")); detail_title.pack(anchor="w",pady=(12,2))
    detail_badge=ttk.Label(details,text=""); detail_badge.pack(anchor="w")
    detail_text=tk.Text(details,height=13,width=36,wrap="word",state="disabled",borderwidth=0,background=app.cget("background")); detail_text.pack(fill="both",expand=True,pady=(10,8))
    mod_actions=ttk.Frame(details); mod_actions.pack(fill="x")
    enable_btn=ttk.Button(mod_actions,text="Enable",state="disabled"); enable_btn.pack(side="left",fill="x",expand=True,padx=(0,4))
    disable_btn=ttk.Button(mod_actions,text="Disable",state="disabled"); disable_btn.pack(side="left",fill="x",expand=True)

    activity_frame=ttk.LabelFrame(app,text="Activity",padding=6); activity_frame.pack(side="bottom",fill="x",padx=12,pady=(0,12))
    activity=tk.Text(activity_frame,height=5,wrap="word",state="disabled"); activity.pack(fill="x")
    def write(value):
        text=str(value); activity.configure(state="normal"); activity.insert("end",text+"\n"); activity.see("end"); activity.configure(state="disabled")
    def current():
        if not profile_list.curselection(): raise ValueError("select a profile")
        return profile_list.get(profile_list.curselection()[0]).split("  ",1)[0]
    def selected_entry():
        ident=selected_mod_id[0]
        if not ident: raise ValueError("select a mod")
        entries,_,_=controller.profile_mods(current())
        return entries[ident]
    def set_detail(entry):
        detail_title.configure(text=entry.name)
        detail_badge.configure(text=f"{entry.status.upper()}  •  {entry.category}  •  v{entry.version}")
        detail_text.configure(state="normal"); detail_text.delete("1.0","end")
        lines=[entry.description or "No description available.","",f"ID: {entry.id}",f"Type: {entry.kind}",f"Compatibility: {'online-safe' if entry.online_safe else 'offline only'}"]
        if entry.capabilities: lines.append("Capabilities: "+", ".join(entry.capabilities))
        if entry.tags: lines.append("Tags: "+", ".join(entry.tags))
        if entry.requirements: lines.extend(["","Requirements:"]+["• "+x for x in entry.requirements])
        if entry.status != "available": lines.extend(["","This entry is visible in the roadmap and cannot be enabled yet."])
        detail_text.insert("end","\n".join(lines)); detail_text.configure(state="disabled")
        installed=entry.manifest is not None
        enable_btn.configure(state="normal" if installed else "disabled")
        disable_btn.configure(state="normal" if installed else "disabled")
    def refresh_profiles():
        profile_list.delete(0,"end")
        rows=controller.list_profiles()
        for row in rows: profile_list.insert("end",f"{row.id}  [{row.compatibility}]")
        if rows: profile_list.selection_set(0); profile_list.activate(0)
    def refresh_catalog(*_):
        tree.delete(*tree.get_children())
        if not profile_list.size(): return
        try: entries,enabled,missing=controller.profile_mods(current())
        except Exception as exc: write("Profile error: "+str(exc)); return
        categories=sorted({e.category for e in entries.values()}); category_box.configure(values=("All categories",*categories))
        query=search.get().lower().strip(); cat=category.get(); sf=status_filter.get()
        for ident,entry in sorted(entries.items(),key=lambda kv:(kv[1].status,kv[1].category,kv[1].name)):
            if query and query not in (entry.name+" "+entry.id+" "+entry.description).lower(): continue
            if cat != "All categories" and entry.category != cat: continue
            if sf != "All" and entry.status != sf: continue
            mark="Enabled" if ident in enabled else "Not enabled"
            tree.insert("","end",iid=ident,values=(entry.name,entry.status.title(),entry.category,entry.version,mark))
        if missing: write("Missing profile entries: "+", ".join(missing))
        profile_title.configure(text=next((r.name for r in controller.list_profiles() if r.id==current()),current()))
        row=next((r for r in controller.list_profiles() if r.id==current()),None)
        profile_meta.configure(text=f"{current()}  •  {row.mode if row else '?'}  •  {row.compatibility if row else 'invalid'}  •  {len(enabled)} enabled")
    def select_mod(_event=None):
        sel=tree.selection()
        if not sel: return
        selected_mod_id[0]=sel[0]
        try: set_detail(selected_entry())
        except Exception as exc: write(str(exc))
    def toggle(enabled):
        try:
            entry=selected_entry()
            if entry.manifest is None: raise CatalogError(f"{entry.name} is {entry.status}; it is not installable yet")
            write(json.dumps(controller.set_mod_enabled(current(),entry.id,enabled),indent=2)); refresh_catalog(); tree.selection_set(entry.id); select_mod()
        except Exception as exc: messagebox.showerror("PascalPatch",str(exc))
    def action(fn):
        try: write(fn(current()))
        except Exception as exc:
            if self_test: write("ERROR: "+str(exc)); app.after(100,app.destroy)
            else: messagebox.showerror("PascalPatch",str(exc))
    def create_new():
        base=filedialog.askopenfilename(title="Choose your Melee 1.02 ISO",filetypes=(("GameCube ISO","*.iso *.gcm"),("All files","*.*")))
        if not base: return
        ident=simpledialog.askstring("New profile","Profile ID (lowercase):",parent=app)
        name=simpledialog.askstring("New profile","Profile name:",parent=app)
        if not ident or not name: return
        try:
            from .mods import create_profile
            create_profile(controller.project_root,ident,name,base,mode="offline")
            refresh_profiles(); refresh_catalog(); write("Created profile "+ident)
        except Exception as exc: messagebox.showerror("PascalPatch",str(exc))
    def delete_current():
        try:
            pid=current()
            if messagebox.askyesno("Delete profile",f"Delete {pid}?",parent=app):
                from .mods import delete_profile
                delete_profile(controller.project_root,pid); refresh_profiles(); refresh_catalog(); write("Deleted profile "+pid)
        except Exception as exc: messagebox.showerror("PascalPatch",str(exc))
    ttk.Button(profile_buttons,text="New profile",command=create_new).pack(side="left",fill="x",expand=True,padx=(0,3))
    ttk.Button(profile_buttons,text="Delete",command=delete_current).pack(side="left",fill="x",expand=True)
    profile_list.bind("<<ListboxSelect>>",refresh_catalog); tree.bind("<<TreeviewSelect>>",select_mod); tree.bind("<Double-Button-1>",lambda _e: toggle(selected_entry().id not in controller.profile_mods(current())[1]))
    enable_btn.configure(command=lambda: toggle(True)); disable_btn.configure(command=lambda: toggle(False))
    search.trace_add("write",refresh_catalog); category_box.bind("<<ComboboxSelected>>",refresh_catalog); status_box.bind("<<ComboboxSelected>>",refresh_catalog)
    refresh_profiles(); refresh_catalog()
    if self_test:
        def automated():
            if not profile_list.size(): raise RuntimeError("GUI self-test requires at least one profile")
            profile_list.selection_set(0); profile_list.activate(0); refresh_catalog()
            action(lambda p: json.dumps(controller.validate(p),indent=2)); action(lambda p: str(controller.build(p).output))
            if self_test_launch: action(lambda p: str(controller.launch(p,dolphin=dolphin,allow_unsafe=allow.get(),wait=True,timeout=2)))
            app.after(100,app.destroy)
        app.after(100,automated)
    app.mainloop()
