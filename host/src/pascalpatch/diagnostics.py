from __future__ import annotations
import json, os, re, shutil, subprocess
from pathlib import Path
def redact(value):
 text=str(value); home=str(Path.home())
 text=text.replace(home,"<HOME>")
 text=re.sub(r"(?i)([A-Za-z]:)?[/\\][^\s]+\.(?:iso|dol)","<GAME_DATA>",text)
 return text
def report(profile_id,game_hash,plugins,runtime_api,error=None):
 return {"profile_id":redact(profile_id),"game_hash":redact(game_hash),"plugins":[{"id":redact(p.get("id")),"version":redact(p.get("version"))} for p in plugins],"runtime_api":runtime_api,"error":redact(error) if error else None}
def write_report(path,**kwargs):
 p=Path(path); p.parent.mkdir(parents=True,exist_ok=True); p.write_text(json.dumps(report(**kwargs),indent=2,sort_keys=True)+"\n"); return p


def symbolize_native(elf, addresses, tool=None, timeout=3.0):
    """Resolve hexadecimal instruction addresses with addr2line.

    This is deliberately bounded and shell-free. Missing tools, malformed
    addresses, timeouts, and non-zero exits become redacted diagnostic records
    instead of exceptions that could hide the original crash.
    """
    path=Path(elf).expanduser().resolve()
    result=[]
    if not path.is_file(): return [{"address":str(a),"error":"ELF not found"} for a in addresses]
    executable=tool or shutil.which("addr2line")
    if not executable: return [{"address":str(a),"error":"addr2line unavailable"} for a in addresses]
    normalized=[]
    for address in addresses:
        value=str(address).strip()
        if not re.fullmatch(r"(?:0x)?[0-9a-fA-F]+",value):
            result.append({"address":value,"error":"invalid address"})
        else: normalized.append((value,"0x"+value[2:] if value.lower().startswith("0x") else "0x"+value))
    if normalized:
        try:
            completed=subprocess.run([str(executable),"-f","-C","-e",str(path),*[x[1] for x in normalized]],capture_output=True,text=True,timeout=timeout,check=False)
            lines=completed.stdout.splitlines()
            for index,(original,_) in enumerate(normalized):
                function=lines[index*2] if index*2<len(lines) else "??"
                location=lines[index*2+1] if index*2+1<len(lines) else "??:0"
                result.append({"address":original,"function":redact(function),"location":redact(location),**({"error":"addr2line failed"} if completed.returncode else {})})
        except subprocess.TimeoutExpired:
            result.extend({"address":original,"error":"addr2line timeout"} for original,_ in normalized)
        except OSError:
            result.extend({"address":original,"error":"addr2line failed to start"} for original,_ in normalized)
    return result
