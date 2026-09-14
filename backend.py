"""Minimal offline build backend for source checkouts."""
import pathlib, zipfile
NAME="meleemod_host"
VERSION="0.1.0"
def _meta(): return {"name":NAME,"version":VERSION,"requires_dist":[]}
def get_requires_for_build_wheel(config_settings=None): return []
def prepare_metadata_for_build_wheel(metadata_directory,config_settings=None):
 p=pathlib.Path(metadata_directory)/(NAME+"-"+VERSION+".dist-info"); p.mkdir(); (p/"METADATA").write_text(f"Metadata-Version: 2.1\nName: {NAME}\nVersion: {VERSION}\n"); (p/"WHEEL").write_text("Wheel-Version: 1.0\nGenerator: meleemod\nRoot-Is-Purelib: true\nTag: py3-none-any\n"); return p.name
def build_wheel(wheel_directory,config_settings=None,metadata_directory=None):
 out=pathlib.Path(wheel_directory)/(NAME.replace("_","-")+"-"+VERSION+"-py3-none-any.whl");
 with zipfile.ZipFile(out,"w") as z:
  for f in pathlib.Path("host/src").rglob("*.py"): z.write(f, f.relative_to("host/src"))
 return out.name
