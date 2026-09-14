from __future__ import annotations
from .errors import CompositionError, ValidationError
from .manifest import validate_plugin, GAME_VERSION

def resolve_plugin_order(plugins):
    errors=[]; by_id={}
    for i,p in enumerate(plugins):
        errors.extend(validate_plugin(p,f"plugins[{i}]"))
        ident=p.get("id")
        if ident in by_id: errors.append(ValidationError(f"plugins[{i}].id","duplicate","duplicate plugin ID"))
        else: by_id[ident]=p
    if errors: raise CompositionError("invalid plugin set",errors)
    # Stable topological ordering. Dependencies must be selected and cycles fail closed.
    order=[]; state={}
    def visit(ident,stack):
        if state.get(ident)==1: raise CompositionError("plugin dependency cycle",[ValidationError("plugins","dependency_cycle"," -> ".join(stack+[ident]))])
        if state.get(ident)==2:return
        if ident not in by_id: raise CompositionError("missing plugin dependency",[ValidationError("plugins","missing_dependency",ident)])
        state[ident]=1
        for dep in by_id[ident].get("dependencies",[]): visit(dep,stack+[ident])
        state[ident]=2; order.append(by_id[ident])
    for p in plugins: visit(p["id"],[])
    return tuple(order)

def compose_static_manifest(plugins,game_version=GAME_VERSION):
    ordered=resolve_plugin_order(plugins)
    if game_version != GAME_VERSION: raise CompositionError("unsupported game version")
    ids=[p["id"] for p in ordered]
    capabilities=sorted({c for p in ordered for c in p.get("capabilities",[])})
    return {"api_version":max((p["api_version"] for p in ordered),default=1),"game_version":game_version,"plugins":[{"id":p["id"],"version":p["version"],"entrypoint":p["entrypoint"]} for p in ordered],"capabilities":capabilities,"link_status":"deferred-until-runtime-integration"}
