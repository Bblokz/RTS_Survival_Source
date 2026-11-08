"""Sky plates for the global ability icons (2026-11-07): real in-game skies captured with a SceneCapture2D.
Runs headless (Development UnrealEditor-Cmd, -run=pythonscript -AllowCommandletRendering), see sky_plates_run.ps1.

Saved/GlobalAbilityIcons/sky/run_config.json:
    {"probe": true,                         dump actors / sky material params of each map to probe_<map>.json
     "shots": [{"map": "/Game/...", "name": "intro_y090_p15", "yaw": 90, "pitch": 15, "fov": 60,
                "loc": [x, y, z] | null (= first cine camera), "sky_mi": "/Game/SkyboxPack_X/...Inst_05" | null,
                "sun": [pitch, yaw] | null}, ...]}
Shots are grouped by map; each map loads once. Output: Saved/GlobalAbilityIcons/sky/plates/<name>.png (1024 FinalColor).
"""
import json
import time
import traceback

import unreal

LOG = "[SkyPlates] "
WORK = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/sky/"
PLATES = WORK + "plates"
RES = 1024


def log(message):
    unreal.log(LOG + message)
    with open(WORK + "run_log.txt", "a", encoding="utf-8") as fh:
        fh.write(time.strftime("%H:%M:%S ") + message + "\n")


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def actors():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()


def finish_compiles(seconds=0):
    unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
    try:
        unreal.AssetCompilingManager  # noqa
    except Exception:
        pass
    unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncAssetCompilationFinishAll")
    unreal.ShaderCompilingManager if hasattr(unreal, "ShaderCompilingManager") else None
    if seconds:
        end = time.time() + seconds
        while time.time() < end:
            unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncAssetCompilationFinishAll")
            time.sleep(2)


def mesh_materials(actor):
    out = []
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = comp.get_editor_property("static_mesh")
        mats = [m.get_path_name() if m else None for m in comp.get_materials()]
        out.append({"comp": comp.get_name(), "mesh": mesh.get_path_name() if mesh else None, "mats": mats,
                    "scale": [comp.get_world_scale().x, comp.get_world_scale().y, comp.get_world_scale().z],
                    "loc": [comp.get_world_location().x, comp.get_world_location().y, comp.get_world_location().z]})
    return out


INTEREST = ("Sky", "Cloud", "Light", "Fog", "Camera", "PostProcess", "Atmosphere", "Sun", "GodRay", "Sunbreak")


def probe(map_path):
    info = {"map": map_path, "actors": [], "mi": {}}
    for a in actors():
        cls = a.get_class().get_name()
        label = a.get_actor_label()
        if not any(k.lower() in (cls + label).lower() for k in INTEREST):
            continue
        loc, rot = a.get_actor_location(), a.get_actor_rotation()
        entry = {"class": cls, "label": label, "loc": [loc.x, loc.y, loc.z], "rot": [rot.pitch, rot.yaw, rot.roll],
                 "meshes": mesh_materials(a)}
        info["actors"].append(entry)
        for m in entry["meshes"]:
            for mat in m["mats"]:
                if mat and mat not in info["mi"]:
                    info["mi"][mat] = mi_params(mat)
    name = map_path.split("/")[-1]
    with open(WORK + "probe_%s.json" % name, "w", encoding="utf-8") as fh:
        json.dump(info, fh, indent=1)
    log("probe %s: %d actors" % (name, len(info["actors"])))


def mi_params(path):
    mat = unreal.load_asset(path)
    out = {}
    if isinstance(mat, unreal.MaterialInstanceConstant):
        for p in mat.get_editor_property("scalar_parameter_values"):
            out["s:" + str(p.parameter_info.name)] = p.parameter_value
        for p in mat.get_editor_property("vector_parameter_values"):
            c = p.parameter_value
            out["v:" + str(p.parameter_info.name)] = [c.r, c.g, c.b, c.a]
        for p in mat.get_editor_property("texture_parameter_values"):
            out["t:" + str(p.parameter_info.name)] = p.parameter_value.get_path_name() if p.parameter_value else None
        parent = mat.get_editor_property("parent")
        out["parent"] = parent.get_path_name() if parent else None
    return out


def first_cine_camera():
    for a in actors():
        if isinstance(a, unreal.CineCameraActor) or isinstance(a, unreal.CameraActor):
            return a.get_actor_location()
    return unreal.Vector(0, 0, 2000)


def skybox_components():
    out = []
    for a in actors():
        for comp in a.get_components_by_class(unreal.StaticMeshComponent):
            mesh = comp.get_editor_property("static_mesh")
            if mesh and "Skybox" in mesh.get_name() or (mesh and "Sky_Sphere" in mesh.get_name()):
                out.append(comp)
    return out


def full_res_textures(material):
    """The sky panoramas stream in at a low mip in a commandlet: load them whole."""
    if not material:
        return
    textures = []
    base = material
    while isinstance(base, unreal.MaterialInstance):
        for p in base.get_editor_property("texture_parameter_values"):
            if p.parameter_value:
                textures.append(p.parameter_value)
        base = base.get_editor_property("parent")
    if isinstance(base, unreal.Material):
        textures += list(unreal.MaterialEditingLibrary.get_used_textures(base))
    for tex in {t.get_path_name(): t for t in textures if isinstance(t, unreal.Texture2D)}.values():
        try:
            if not tex.get_editor_property("never_stream"):
                tex.set_editor_property("never_stream", True)
            log("texture %s %dx%d" % (tex.get_name(), tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))
        except Exception as error:
            log("texture %s: %s" % (tex.get_name(), error))


def set_sun(pitch_yaw):
    for a in actors():
        if isinstance(a, unreal.DirectionalLight):
            a.set_actor_rotation(unreal.Rotator(roll=0, pitch=pitch_yaw[0], yaw=pitch_yaw[1]), False)


def capture(shot):
    w = world()
    loc = unreal.Vector(*shot["loc"]) if shot.get("loc") else first_cine_camera()
    rot = unreal.Rotator(roll=0.0, pitch=float(shot.get("pitch", 10)), yaw=float(shot.get("yaw", 0)))
    cap_actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(
        unreal.SceneCapture2D, loc, rot)
    try:
        cap = cap_actor.capture_component2d
        rt = unreal.RenderingLibrary.create_render_target2d(w, RES, RES, unreal.TextureRenderTargetFormat.RTF_RGBA8,
                                                            unreal.LinearColor(0, 0, 0, 1))
        cap.set_editor_property("texture_target", rt)
        cap.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
        cap.set_editor_property("fov_angle", float(shot.get("fov", 60)))
        cap.set_editor_property("capture_every_frame", False)
        cap.set_editor_property("capture_on_movement", False)
        try:
            cap.set_editor_property("always_persist_rendering_state", True)
        except Exception:
            pass
        flags = [unreal.EngineShowFlagsSetting(show_flag_name=f, enabled=False)
                 for f in ("MotionBlur", "DepthOfField", "Grain", "Vignette")]
        cap.set_editor_property("show_flag_settings", flags)
        if shot.get("exposure") is not None:
            pp = cap.get_editor_property("post_process_settings")
            pp.set_editor_property("override_auto_exposure_method", True)
            pp.set_editor_property("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
            pp.set_editor_property("override_auto_exposure_bias", True)
            pp.set_editor_property("auto_exposure_bias", float(shot["exposure"]))
            pp.set_editor_property("override_auto_exposure_apply_physical_camera_exposure", True)
            pp.set_editor_property("auto_exposure_apply_physical_camera_exposure", False)
            cap.set_editor_property("post_process_settings", pp)
            cap.set_editor_property("post_process_blend_weight", 1.0)
        # several captures so temporal effects (volumetric clouds, TSR) settle; the last one is kept
        for _ in range(int(shot.get("warm", 12))):
            cap.capture_scene()
        unreal.RenderingLibrary.export_render_target(w, rt, PLATES, shot["name"] + ".png")
        log("RENDERED %s" % shot["name"])
    finally:
        cap_actor.destroy_actor()


def main():
    config = json.load(open(WORK + "run_config.json", encoding="utf-8"))
    by_map = {}
    for shot in config.get("shots", []):
        by_map.setdefault(shot["map"], []).append(shot)
    for m in config.get("probe_maps", []):
        by_map.setdefault(m, [])
    for map_path, shots in by_map.items():
        try:
            log("loading %s" % map_path)
            unreal.EditorLoadingAndSavingUtils.load_map(map_path)
            finish_compiles(int(config.get("shader_wait", 60)))
            if map_path in config.get("probe_maps", []):
                probe(map_path)
            originals = {c: c.get_materials() for c in skybox_components()}
            for mats in originals.values():
                for mat in mats:
                    full_res_textures(mat)
            for shot in shots:
                if shot.get("sky_mi"):
                    full_res_textures(unreal.load_asset(shot["sky_mi"]))
            finish_compiles(int(config.get("texture_wait", 20)))
            for shot in shots:
                try:
                    if shot.get("sky_mi") or shot.get("sky_params"):
                        mi = unreal.load_asset(shot["sky_mi"]) if shot.get("sky_mi") else list(originals.values())[0][0]
                        if shot.get("sky_params"):
                            mid = unreal.MaterialInstanceDynamic.cast(
                                unreal.MaterialLibrary.create_dynamic_material_instance(world(), mi))
                            for key, value in shot["sky_params"].items():
                                kind, pname = key.split(":", 1)
                                if kind == "s":
                                    mid.set_scalar_parameter_value(pname, float(value))
                                else:
                                    mid.set_vector_parameter_value(pname, unreal.LinearColor(*value))
                            mi = mid
                        for c in originals:
                            for i in range(len(originals[c])):
                                c.set_material(i, mi)
                        finish_compiles(int(shot.get("mi_wait", 8)))
                    else:
                        for c, mats in originals.items():
                            for i, mat in enumerate(mats):
                                c.set_material(i, mat)
                    if shot.get("sun"):
                        set_sun(shot["sun"])
                    capture(shot)
                except Exception:
                    log("shot failed %s: %s" % (shot.get("name"), traceback.format_exc()))
        except Exception:
            log("map failed %s: %s" % (map_path, traceback.format_exc()))
    log("DONE")


main()
