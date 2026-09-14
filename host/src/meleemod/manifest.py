"""Strict, dependency-free validation for versioned JSON contracts."""
from __future__ import annotations
import json, re
from pathlib import Path
from .errors import ValidationError, ManifestError

ID_RE=re.compile(r"^[a-z0-9][a-z0-9._-]{1,63}$")
GAME_VERSION="GALE01-1.02"
MOD_TYPES={"filesystem"}
MOD_CAPABILITIES={"visual-only","gameplay-changing","unknown"}

def load_json(path):
    try: return json.loads(Path(path).read_text(encoding="utf-8"))
    except FileNotFoundError: raise ManifestError(f"manifest not found: {path}")
    except json.JSONDecodeError as e: raise ManifestError(f"invalid JSON {path}: line {e.lineno}, column {e.colno}")

def _base(obj, path, required, errors, optional=()):
    if not isinstance(obj,dict): errors.append(ValidationError(path,"type","expected an object")); return
    unknown=set(obj)-set(required)-set(optional)
    for k in sorted(unknown): errors.append(ValidationError(f"{path}.{k}","unknown_field","unknown fields are not allowed"))
    for k in required:
        if k not in obj: errors.append(ValidationError(f"{path}.{k}","required",f"missing required field {k!r}"))
    if "id" in obj and (not isinstance(obj["id"],str) or not ID_RE.fullmatch(obj["id"])): errors.append(ValidationError(f"{path}.id","id","must match lowercase identifier format"))
    if "version" in obj and (not isinstance(obj["version"],str) or not re.fullmatch(r"\d+\.\d+\.\d+",obj["version"])): errors.append(ValidationError(f"{path}.version","version","must use semantic version X.Y.Z"))

def validate_plugin(obj, path="plugin"):
 e=[]; _base(obj,path,{"id","version","api_version","entrypoint","capabilities","dependencies","game_versions","online_safe"},e,optional=("conflicts","hooks","source","static_signature","shutdown",))
 if isinstance(obj,dict):
  if not isinstance(obj.get("api_version"),int) or obj.get("api_version",0)<1: e.append(ValidationError(path+".api_version","api_version","must be a positive integer"))
  if not isinstance(obj.get("entrypoint"),str) or not obj.get("entrypoint"): e.append(ValidationError(path+".entrypoint","entrypoint","must be non-empty"))
  if "shutdown" in obj and (not isinstance(obj.get("shutdown"),str) or not obj.get("shutdown")): e.append(ValidationError(path+".shutdown","shutdown","must be a non-empty function name"))
  caps=obj.get("capabilities");
  if not isinstance(caps,list) or not caps or any(c not in MOD_CAPABILITIES and not isinstance(c,str) for c in caps): e.append(ValidationError(path+".capabilities","capabilities","must be a non-empty list of capability names"))
  if not isinstance(obj.get("dependencies"),list) or any(not isinstance(x,str) for x in obj.get("dependencies",[])): e.append(ValidationError(path+".dependencies","dependencies","must be a list of IDs"))
  if obj.get("game_versions") != [GAME_VERSION]: e.append(ValidationError(path+".game_versions","game_version","must explicitly target GALE01-1.02"))
  if not isinstance(obj.get("online_safe"),bool): e.append(ValidationError(path+".online_safe","online_safe","must be boolean metadata"))
 return e

def validate_mod(obj,path="mod"):
 e=[]; _base(obj,path,{"id","version","type","source","targets","conflicts","priority"},e,optional=("capability",))
 if isinstance(obj,dict):
  if obj.get("type") not in MOD_TYPES: e.append(ValidationError(path+".type","type","only filesystem mods are supported in MVP"))
  if not isinstance(obj.get("source"),str) or not obj.get("source"): e.append(ValidationError(path+".source","source","must be a source directory"))
  if not isinstance(obj.get("targets"),list) or not obj.get("targets") or any(not isinstance(x,str) or x.startswith("/") or "\\" in x or ".." in Path(x).parts for x in obj.get("targets",[])): e.append(ValidationError(path+".targets","path","targets must be relative safe disc paths"))
  if not isinstance(obj.get("conflicts"),list): e.append(ValidationError(path+".conflicts","conflicts","must be a list"))
  if not isinstance(obj.get("priority"),int): e.append(ValidationError(path+".priority","priority","must be an integer"))
 return e

def validate_profile(obj,path="profile"):
 e=[]; _base(obj,path,{"id","name","game_version","base_game","plugins","mods","mode","online_safe"},e,optional=("allow_priority","decomp_repo","decomp_orig","plugin_source_root","runtime_root",))
 if isinstance(obj,dict):
  if obj.get("game_version")!=GAME_VERSION: e.append(ValidationError(path+".game_version","game_version","must be GALE01-1.02"))
  if not isinstance(obj.get("base_game"),str) or not obj.get("base_game"): e.append(ValidationError(path+".base_game","base_game","must be a path"))
  for k in ("plugins","mods"):
   if not isinstance(obj.get(k),list) or any(not isinstance(x,str) for x in obj.get(k,[])): e.append(ValidationError(f"{path}.{k}","references","must be a list of IDs"))
   elif len(obj[k]) != len(set(obj[k])): e.append(ValidationError(f"{path}.{k}","duplicate","entries must be unique"))
  if obj.get("mode") not in {"vanilla","tournament-safe","slippi","offline"}: e.append(ValidationError(path+".mode","mode","unsupported profile mode"))
  if not isinstance(obj.get("online_safe"),bool): e.append(ValidationError(path+".online_safe","online_safe","must be boolean metadata"))
  for k in ("decomp_repo","decomp_orig","plugin_source_root","runtime_root"):
   if k in obj and (not isinstance(obj[k],str) or not obj[k]): e.append(ValidationError(path+"."+k,"path","must be a non-empty path"))
 return e

def require_valid(obj,kind,path="manifest"):
 fn={"profile":validate_profile,"plugin":validate_plugin,"mod":validate_mod}[kind]; errors=fn(obj,path)
 if errors: raise ManifestError(f"invalid {kind} manifest", errors)
 return obj
