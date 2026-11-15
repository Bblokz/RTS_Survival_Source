"""Supply truck / supply point minimap icons: cut-out renders of the game's own meshes with their own materials.

Runs headless, never in the shared editor (run_render.ps1):
    UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -AllowCommandletRendering ...
Reuses the ability-icon capture pipeline (Source/Tools/GlobalAbilityIcons/ue_render_unit.py): parts are spawned on
/Engine/Maps/Templates/Template_Default, lit by the studio lights, captured with a show-only SceneCapture2D (tonemapped
colour + coverage mask + world normals) and destroyed. Nothing is saved but PNGs in
Saved/SupplyPointMinimapIcons/renders. Texture streaming is off for the whole run and every job is rendered twice.

Which jobs run comes from Saved/SupplyPointMinimapIcons/run_config.json: {"jobs": ["Truck_Top", ...] | "all"}.
compose_supply_minimap_icons.py turns the renders into the icons.
"""
import json
import time
import traceback

import unreal

WORK = "D:/UE5Projects/RTS_Survival/Saved/SupplyPointMinimapIcons/"
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/ue_render_unit.py").read())
RENDER_DIR = WORK + "renders"

NOMADIC = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/"
OPEL = NOMADIC + "GerEco/OpelBlitz_Ref/Skeleton/SK_Ger_Opel"
HQ_FILL = NOMADIC + "SKDFZ-7/ResourceAddons/"
BF_FILL = NOMADIC + "BreakthroughHQ/Fills/"
SCAV = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/"
FLAGS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/"

HERO = dict(azimuth=35.0, elevation=26.0, padding=1.0)
TOP = dict(azimuth=90.0, elevation=89.0, padding=1.0)      # image right = world +X
SIDE = dict(azimuth=90.0, elevation=5.0, padding=1.0)


def single(asset, view=HERO, **part):
    return dict(parts=[dict(asset=asset, yaw=part.pop("yaw", 0.0), **part)], **view)


MEGAPHONE = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Megaphone/"

# The truck's component bounds sit off-centre and are smaller than the mesh, so it gets far more room than the props.
TRUCK = dict(padding=1.9, res=2048)
ITEMS = {
    "Metal": BF_FILL + "SM_BF_HQ_MetalAt100",
    "Parts": BF_FILL + "SM_BF_HQ_VehiclePartsAt100",
    "Radixite": BF_FILL + "SM_BF_HQ_RadixiteAt100",
    "RadixiteHQ": HQ_FILL + "SM_GERHQ_RadixiteAt25",
    "RadixiteCore": "/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteCores/SM_RadixiteCore_1",
    "Crate": "/Game/MilitaryBaseCamp/Meshes/SM_CrateBig",
    "FieldSupply": NOMADIC + "BreakthroughArmory/Mesh/SM_BF_Armory_FieldSupply",
}

JOBS = {
    "Truck_Top": single(OPEL, dict(TOP, **TRUCK)),
    "Truck_Hero": single(OPEL, dict(HERO, **TRUCK)),
    "Truck_Side": single(OPEL, dict(SIDE, **TRUCK)),
    # the Soviet banner is the game's only flag mesh; tried hanging and flying
    "Banner_Pitch_Side": single(FLAGS + "SM_Rus_SovietBanner", SIDE, pitch=90.0),
    "Banner_Roll_Side": single(FLAGS + "SM_Rus_SovietBanner", SIDE, roll=90.0),
    "Banner_Pitch_Hero": single(FLAGS + "SM_Rus_SovietBanner", HERO, pitch=90.0),
    "Banner_Roll_Hero": single(FLAGS + "SM_Rus_SovietBanner", HERO, roll=90.0),
    "Banner_Top": single(FLAGS + "SM_Rus_SovietBanner", TOP),
    "BannerCrown_Side": single(MEGAPHONE + "SM_SovietMegaphonePole_BannerCrown", SIDE),
    "BannerCrown_Hero": single(MEGAPHONE + "SM_SovietMegaphonePole_BannerCrown", HERO),
    "Holder_Side": single(FLAGS + "SM_RusBannerHolder", SIDE),
}
for _name, _asset in ITEMS.items():
    _res = dict(res=2048) if _name == "FieldSupply" else {}
    JOBS[_name + "_Top"] = single(_asset, dict(TOP, **_res))
    JOBS[_name + "_Hero"] = single(_asset, dict(HERO, **_res))
    JOBS[_name + "_Side"] = single(_asset, dict(SIDE, **_res))

# ---- faction flags (exploration): the vehicle flag attachments and the faction-selection flag, per atlas index ----
FF = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/FactionFlags/"
ATTACH = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/Attachments/"
FLAG_ACTORS = {
    "AttBanner": ATTACH + "Attachment_Banner",
    "AttPole": ATTACH + "Attachment_FlagPole",
    "SelFlag": "/Game/RTS_Survival/FactionSelection/BP_FactionFlag",
    "DestSoviet": "/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Dest_SovietFlag",
}
for _name, _asset in FLAG_ACTORS.items():
    for _index in range(4):
        JOBS["XF_%s_%d_Side" % (_name, _index)] = dict(parts=[dict(asset=_asset, yaw=0.0)], cpd=_index, **SIDE)
        JOBS["XF_%s_%d_Front" % (_name, _index)] = dict(parts=[dict(asset=_asset, yaw=0.0)], cpd=_index,
                                                         azimuth=0.0, elevation=5.0, padding=1.0)
for _name in ("MI_BreakthroughBanner", "MI_StrikeDivisionBanner", "MI_BreakthroughPole", "MI_StrikeDivisionPole",
              "MI_SovietFlagPole"):
    JOBS["XF_Banner_" + _name] = dict(parts=[dict(asset=FLAGS + "SM_Rus_SovietBanner", yaw=0.0, roll=90.0,
                                                  materials=[(0, FF + _name)])], **SIDE)
    JOBS["XF_Pole_" + _name] = dict(parts=[dict(asset=FF + "SM_FlagPole", yaw=0.0)], mat_all=FF + _name, **SIDE)
JOBS["XF_Banner_Forge"] = dict(parts=[dict(asset=FLAGS + "SM_Rus_SovietBanner", yaw=0.0, roll=90.0,
                                           materials=[(0, FLAGS + "Mat/Emblems/MI_Emblem_DirectorOfForge")])], **SIDE)


# ---- the flags of the final supply point icons ----
# The game's flag is Attachment_FlagPole: SM_FlagPole plus SM_Rus_SovietBanner as its "Flag" component, whose material
# is swapped per faction (MI_<Faction>Pole, parent M_FlagPole, texture parameter "Banner"). The enemy directors have
# banner emblem materials (MI_Emblem_<Director>, parent M_SovietBanner, same "Banner" parameter). A ("MID", parent,
# texture) entry renders with a transient instance of parent with that Banner texture: nothing is saved.
EMBLEMS = FLAGS + "Mat/Emblems/"
FLAG_MATERIALS = {
    "GerStrikeDivision": FF + "MI_StrikeDivisionPole",
    "GerBreakthrough": FF + "MI_BreakthroughPole",
    "SovietStar": FF + "MI_SovietFlagPole",
    "DirectorOfShield": EMBLEMS + "MI_Emblem_DirectorOfShield",
    "DirectorOfForge": EMBLEMS + "MI_Emblem_DirectorOfForge",
    "DirectorOfHarvest": ("MID", EMBLEMS + "MI_Emblem_DirectorOfShield",
                          FLAGS + "Mat/Textures/Emblems/T_Propaganda_DirectorOfHarvest"),
    "AtlasCheck": ("MID", FF + "MI_StrikeDivisionPole", FF + "FactionAtlas"),
}
for _name, _material in FLAG_MATERIALS.items():
    JOBS["Flag_" + _name] = dict(parts=[dict(asset=ATTACH + "Attachment_FlagPole", yaw=0.0)],
                                 comp_mats={"Flag": _material}, **dict(SIDE, res=2048))
    # the banner alone, flat on, as the director emblems were first rendered
    JOBS["FlagFlat_" + _name] = dict(parts=[dict(asset=FLAGS + "SM_Rus_SovietBanner", yaw=0.0, roll=90.0)],
                                     comp_mats={"": _material}, **SIDE)


def resolve_material(spec):
    if isinstance(spec, str):
        return unreal.load_asset(spec)
    _, parent, texture = spec
    instance = unreal.MaterialLibrary.create_dynamic_material_instance(_world(), unreal.load_asset(parent))
    instance.set_texture_parameter_value("Banner", unreal.load_asset(texture))
    return instance


def describe_material(path):
    material = unreal.load_asset(path)
    if material is None:
        return {"path": path, "missing": True}
    out = {"path": path, "class": material.get_class().get_name()}
    lib = unreal.MaterialEditingLibrary
    try:
        if isinstance(material, unreal.MaterialInstanceConstant):
            parent = material.get_editor_property("parent")
            out["parent"] = parent.get_path_name() if parent else None
            base = material.get_base_material()
            out["base"] = base.get_path_name()
            out["textures"] = {str(n): (lambda t: t.get_path_name() if t else None)(
                lib.get_material_instance_texture_parameter_value(material, n))
                for n in lib.get_texture_parameter_names(base)}
            out["scalars"] = {str(n): lib.get_material_instance_scalar_parameter_value(material, n)
                              for n in lib.get_scalar_parameter_names(base)}
        elif isinstance(material, unreal.Material):
            out["texture_params"] = [str(n) for n in lib.get_texture_parameter_names(material)]
            out["scalar_params"] = [str(n) for n in lib.get_scalar_parameter_names(material)]
            out["used_textures"] = [t.get_path_name() for t in lib.get_used_textures(material)]
    except Exception as error:
        out["error"] = str(error)
    return out


def discover():
    """What the flag assets are: written to Saved/SupplyPointMinimapIcons/flag_discovery.json."""
    out = {"materials": [], "actors": {}}
    for name in ("MI_BreakthroughBanner", "MI_StrikeDivisionBanner", "MI_BreakthroughPole", "MI_StrikeDivisionPole",
                 "MI_SovietFlagPole"):
        out["materials"].append(describe_material(FF + name))
    for name in ("MI_Emblem_DirectorOfShield", "MI_Emblem_DirectorOfForge"):
        out["materials"].append(describe_material(FLAGS + "Mat/Emblems/" + name))
    atlas = unreal.load_asset(FF + "FactionAtlas")
    out["FactionAtlas"] = atlas.get_class().get_name() if atlas else None
    spawned = []
    try:
        for name, asset in FLAG_ACTORS.items():
            actors, comps = _stage_parts([dict(asset=asset, yaw=0.0)], spawned)
            rows = []
            for comp in comps:
                mesh = None
                for prop in ("static_mesh", "skeletal_mesh_asset"):
                    try:
                        mesh = comp.get_editor_property(prop)
                        break
                    except Exception:
                        continue
                rows.append({"component": comp.get_name(), "class": comp.get_class().get_name(),
                             "mesh": mesh.get_path_name() if mesh else None,
                             "materials": [m.get_path_name() if m else None for m in comp.get_materials()]})
            out["actors"][name] = rows
            seen = set()
            for row in rows:
                for material in row["materials"]:
                    if material and material not in seen:
                        seen.add(material)
                        out["materials"].append(describe_material(material.split(".")[0]))
    finally:
        _destroy(spawned)
    with open(WORK + "flag_discovery.json", "w", encoding="utf-8") as fh:
        json.dump(out, fh, indent=1)
    log("discovery written")


LOOK = dict(light_intensity=5.0, ambient_intensity=2.0, fov=18.0, res=1024, normals=True)


class AllSockets:
    def __contains__(self, name):
        return True


def log(message):
    unreal.log("[SupplyIcons] " + message)
    with open(WORK + "render_log.txt", "a", encoding="utf-8") as fh:
        fh.write(time.strftime("%H:%M:%S ") + message + "\n")


def write_materials(name, comps):
    """Which materials each rendered mesh carried, for the record of what an icon was made from."""
    out = []
    for comp in comps:
        mesh = None
        for prop in ("static_mesh", "skeletal_mesh_asset"):
            try:
                mesh = comp.get_editor_property(prop)
                break
            except Exception:
                continue
        origin, extent, _ = unreal.SystemLibrary.get_component_bounds(comp)
        out.append({"component": comp.get_name(), "mesh": mesh.get_path_name() if mesh else None,
                    "bounds_origin": [origin.x, origin.y, origin.z - STUDIO_LOCATION.z],
                    "bounds_extent": [extent.x, extent.y, extent.z],
                    "materials": [m.get_path_name() if m else None for m in comp.get_materials()]})
    with open("%s/%s_materials.json" % (RENDER_DIR, name), "w", encoding="utf-8") as fh:
        json.dump(out, fh, indent=1)


def render_job(name):
    job = dict(JOBS[name])
    parts = job.pop("parts")
    cpd = job.pop("cpd", None)          # flag atlas index: custom primitive data 0, as FRTS_FactionFlags sets it
    mat_all = job.pop("mat_all", None)  # one material on every slot
    comp_mats = job.pop("comp_mats", {})  # {component name substring: material spec} on slot 0
    spawned = []
    try:
        actors, comps = _stage_parts(parts, spawned)
        for comp in comps:
            if cpd is not None:
                comp.set_custom_primitive_data_float(0, float(cpd))
            for pattern, spec in comp_mats.items():
                if pattern.lower() in comp.get_name().lower():
                    comp.set_material(0, resolve_material(spec))
            if mat_all:
                for slot in range(comp.get_num_materials()):
                    comp.set_material(slot, unreal.load_asset(mat_all))
        write_materials(name, comps)
        # A commandlet never ticks: block until the shaders are compiled, or materials render as the default checker.
        unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncAssetCompilationFinishAll")
        _capture_staged(name, actors, comps, spawned, sockets=AllSockets(), **dict(LOOK, **job))
    finally:
        _destroy(spawned)


def main():
    with open(WORK + "run_config.json", encoding="utf-8") as fh:
        config = json.load(fh)
    names = config["jobs"]
    if names == "all":
        names = list(JOBS)
    elif isinstance(names, str):      # a prefix, e.g. "XF_"
        names = [n for n in JOBS if n.startswith(names)]
    log("start %s" % names)
    unreal.EditorLoadingAndSavingUtils.load_map("/Engine/Maps/Templates/Template_Default")
    unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
    try:
        if config.get("discover"):
            try:
                discover()
            except Exception:
                log("DISCOVER FAILED\n%s" % traceback.format_exc())
        for attempt in range(int(config.get("passes", 2))):
            for name in names:
                try:
                    render_job(name)
                except Exception:
                    log("JOB %s FAILED\n%s" % (name, traceback.format_exc()))
            log("pass %d done" % (attempt + 1))
    finally:
        unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 1")
    log("done")


main()
