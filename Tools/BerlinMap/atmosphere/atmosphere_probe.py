"""Read-only probe for the Berlin atmosphere work (headless commandlet). Writes Saved/Berlin/atmosphere/probe.json and
T3D exports of the candidate fire systems. Nothing is saved.
"""
import json
import os
import re

import unreal

OUT = "D:/UE5Projects/RTS_Survival/Saved/Berlin/atmosphere/probe/"
os.makedirs(OUT, exist_ok=True)
R = {}


def log(text):
    unreal.log("ATM_PROBE " + str(text))


def props(obj, names):
    out = {}
    for name in names:
        try:
            out[name] = str(obj.get_editor_property(name))
        except Exception as error:  # noqa: BLE001
            out[name] = "ERR %s" % error
    return out


pp = unreal.load_asset("/Game/RTS_Survival/Blueprints/FowSystem/Materials/PostProcess/MPP_FogOfWar_01")
R["fow_pp"] = props(pp, ["material_domain", "blendable_location", "blendable_priority", "blend_mode", "blendable_output_alpha"])
R["refraction_methods"] = [n for n in dir(unreal.RefractionMode)] if hasattr(unreal, "RefractionMode") else "no RefractionMode"
R["tlm"] = [n for n in dir(unreal.TranslucencyLightingMode) if n.startswith("TLM")]

SYSTEMS = {
    "GroundFire": "/Game/RTS_Survival/SpecialEffects/Fire/RTS_GroundFire_0",
    "EngineFire": "/Game/RTS_Survival/SpecialEffects/M5VFXVOL2/Niagara/Reference/Fireloop/RTS_EngineFire_0",
    "Bonefire": "/Game/RTS_Survival/SpecialEffects/M5VFXVOL2/Niagara/Reference/Fireloop/2_Nbonefire_pt",
    "Fire05": "/Game/RTS_Survival/SpecialEffects/Fire/N_RTS_Fire05_DitherAA",
    "Fire08": "/Game/RTS_Survival/SpecialEffects/Fire/N_RTS_Fire08_DitherAA",
    "SmokeyFire1": "/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1/Niagara/NS_SmokeyFire1",
    "LargeTankOnFire": "/Game/RTS_Survival/SpecialEffects/AllExplosions/TankExpl/NS_LargeTank_OnFire",
    "RadixSmoking": "/Game/RTS_Survival/SpecialEffects/RadixiteVFX/NS_Radixite_Smoking1",
    "Litter": "/Game/RTS_Survival/Blueprints/Environment/Berlin/Ambient/Litter/NS_BerlinStreetLitter",
}
R["systems"] = {}
for key, path in SYSTEMS.items():
    system = unreal.load_asset(path)
    if system is None:
        R["systems"][key] = "MISSING"
        continue
    task = unreal.AssetExportTask()
    task.object = system
    task.filename = OUT + key + ".t3d"
    task.exporter = unreal.ObjectExporterT3D()
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    ok = unreal.Exporter.run_asset_export_task(task)
    info = {"export": ok, "path": path}
    try:
        raw = open(OUT + key + ".t3d", "rb").read()
        text = raw.decode("utf-16") if raw[:2] in (b"\xff\xfe", b"\xfe\xff") else raw.decode("utf-8", "replace")
        info["size"] = len(text)
        info["emitters"] = sorted(set(re.findall(r'Name="?([A-Za-z0-9_ ]+)"?,Instance=', text)))[:30]
        info["handles"] = sorted(set(re.findall(r'EmitterHandles\(\d+\)=\(.*?Name="([^"]+)"', text)))[:30]
        info["sim_targets"] = re.findall(r"SimTarget=(\w+)", text)[:20]
        info["renderers"] = sorted(set(re.findall(r"Begin Object Class=/Script/Niagara\.(Niagara\w*RendererProperties)", text)))
        info["materials"] = sorted(set(re.findall(r"Material=\"?[^,\n\"]*?'\"?(/[^'\"]+)", text)))[:20]
        info["collision"] = len(re.findall(r"Collision", text))
        info["fixed_bounds"] = re.findall(r"FixedBounds=\(([^\n]{0,160})", text)[:6]
        info["stateless"] = len(re.findall(r"NiagaraStatelessEmitter", text))
        info["enabled"] = re.findall(r'EmitterHandles\(\d+\)=\((?:.*?)bIsEnabled=(\w+)', text)[:20]
    except Exception as error:  # noqa: BLE001
        info["read_error"] = str(error)
    R["systems"][key] = info

# textures that could carry the smoke
R["textures"] = {}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for folder in ("/Game/RTS_Survival/Blueprints/Environment/AmbientFx", "/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1/Textures",
               "/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1", "/Game/RTS_Survival/SpecialEffects/MsvFx"):
    for data in registry.get_assets_by_path(folder, True):
        cls = str(data.asset_class_path.asset_name)
        if cls in ("Texture2D",):
            R["textures"][str(data.package_name)] = cls
R["textures_count"] = len(R["textures"])
if len(R["textures"]) > 80:
    R["textures"] = dict(list(R["textures"].items())[:80])
puffs = unreal.load_asset("/Game/RTS_Survival/Blueprints/Environment/AmbientFx/T_AmbientDust_Puffs")
if puffs is not None:
    R["puffs"] = dict(x=puffs.blueprint_get_size_x(), y=puffs.blueprint_get_size_y(), srgb=str(puffs.get_editor_property("srgb")),
                      comp=str(puffs.get_editor_property("compression_settings")))

# hero ruin pieces: mesh bounds (asset bounds, for reading only)
R["ruins"] = {}
for data in registry.get_assets_by_path("/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit", True):
    if str(data.asset_class_path.asset_name) == "StaticMesh":
        mesh = data.get_asset()
        box = mesh.get_bounding_box()
        R["ruins"][str(data.asset_name)] = [round(v) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)]

# the smoke pall field function: names of inputs and outputs
field = unreal.load_asset("/Game/RTS_Survival/Materials/Sky/MF_BerlinSmokePallField")
R["field_inputs"] = [str(e.get_editor_property("input_name")) for e in unreal.ObjectIterator(unreal.MaterialExpressionFunctionInput)
                     if e.get_path_name().startswith(field.get_path_name() + ":")]
R["field_outputs"] = [str(e.get_editor_property("output_name")) for e in unreal.ObjectIterator(unreal.MaterialExpressionFunctionOutput)
                      if e.get_path_name().startswith(field.get_path_name() + ":")]
mpc = unreal.load_asset("/Game/RTS_Survival/Materials/Sky/MPC_BerlinSmokePall")
R["pall_params"] = {str(p.get_editor_property("parameter_name")): p.get_editor_property("default_value")
                    for p in mpc.get_editor_property("scalar_parameters")}
R["material_props"] = [n for n in dir(unreal.MaterialProperty) if n.startswith("MP_")]
R["has_light_renderer_class"] = hasattr(unreal, "NiagaraLightRendererProperties")
json.dump(R, open(OUT + "probe.json", "w"), indent=1, default=str)
log("DONE")
