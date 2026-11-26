"""2026-11-25: SU-85 / SU-100 / SU-122 gun animation fixes (headless part).

Diagnosis (T3D dumps of the BPs / AnimBPs, anim pose probes):
- All three vehicles' PlaySingleFireAnimation play Montage_StugFIre, a StuG III montage on another skeleton. UE 5.5
  plays it by bone name; the SU AnimBPs take B_Gun from the slot (layered blend, branch B_Gun), so the barrel snaps
  to the StuG's rest offset, ~40 units further forward on the SU-85 (more on the SU-122): "the gun flies forward".
- The SU-85's own Anim_Su85_FireWeapon (only used by the unused Montage_FireSu85) slid B_Gun back ~189 units on a
  ~178 unit gun, so it was unusable too.
- EmbTurret_SU122.ReferenceCasts stored its AnimBP_Su122 cast in a stray "As Anim BP Su 122" variable, never in
  TurretAnimBp; the vehicle's UpdateTargetPitch / SetTurretAngle gate on TurretAnimBp being valid, so the SU-122 gun
  never got a pitch (or yaw).

Fix (run headless: DebugGame editor, -run=pythonscript, -nullrhi, -DisablePlugins=Fab so the UECP script bridge
loads; steps by name as script args, default su85recoil montages turret):
- su122anim / su85recoil: Anim_SU122_Fire + Montage_FireSu122 and Anim_Su85_Recoil + Montage_Su85_Recoil, copies of
  each gun's T-pose with B_Gun pulled back 16 / 14 component units and back at rest by frame 20.
- montages: PlayMontage MontageToPlay on Rus_Vehicle_Su85 / Su100 -> Montage_Su85_Recoil, Su122 -> Montage_FireSu122.
- turret: EmbTurret_SU122.ReferenceCasts gets a Set TurretAnimBp from the AnimBP_Su122 cast (the bridge refuses
  delete_nodes, so the old stray setter stays in the graph, unlinked from the exec chain).
Blueprints are saved only when their compile reports 0 errors.
Dead ends: replace_variable_references left the setter pin typed AnimBP_Su122 (compile error); rewriting the keys of
Anim_Su85_FireWeapon in place did not change its evaluated pose. Both were reverted.
"""
import unreal

R = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/"
OUT = unreal.Paths.project_saved_dir() + "SuGunAnims/fix_log.txt"
TURRET_122 = R + "Su122/EmbTurret_SU122"
SU85_FIRE = R + "SU85/Meshes/WeaponAnims/Anim_Su85_FireWeapon"
SU122_TPOSE = R + "Su122/Anims/Anim_SU122_TPose"
SU122_FIRE = R + "Su122/Anims/Anim_SU122_Fire"
SU122_MONTAGE = R + "Su122/Anims/Montage_FireSu122"
# Component-space pull back of the barrel at the peak, about 8 % of the ~178 unit weapon mesh.
RECOIL_COMPONENT_UNITS = {"SU85": 14.0, "SU122": 16.0}
APE = unreal.AnimPoseExtensions
L = []


def log(msg):
    L.append(msg)
    unreal.log("SU_GUN_FIX " + msg)


def recoil_alpha(frame):
    """0 at rest, 1 at full pull back: kick in 2 frames, hold to 5, ease back to rest by 20."""
    if frame <= 0:
        return 0.0
    if frame <= 2:
        return frame / 2.0
    if frame <= 5:
        return 1.0
    if frame >= 20:
        return 0.0
    t = (frame - 5) / 15.0
    return 1.0 - (3 * t * t - 2 * t * t * t)


def gun_component_x(anim, frame):
    pose = APE.get_anim_pose_at_frame(anim, frame, unreal.AnimPoseEvaluationOptions())
    return APE.get_bone_pose(pose, "B_Gun", unreal.AnimPoseSpaces.WORLD).translation.x


def write_tracks(anim, num_frames, gun_local_back):
    """Keys every bone at its frame-0 pose; B_Gun local Y moves by recoil_alpha * gun_local_back."""
    controller = anim.get_editor_property("controller")
    controller.open_bracket(unreal.Text("SU gun recoil"), False)
    rest = {}
    for bone in unreal.AnimationLibrary.get_animation_track_names(anim):
        rest[str(bone)] = unreal.AnimationLibrary.get_bone_pose_for_frame(anim, bone, 0, False)
    controller.set_number_of_frames(unreal.FrameNumber(num_frames), False)
    for bone, t in rest.items():
        pos, rot, scl = [], [], []
        for f in range(num_frames + 1):
            p = unreal.Vector(t.translation.x, t.translation.y, t.translation.z)
            if bone == "B_Gun":
                p.y += recoil_alpha(f) * gun_local_back
            pos.append(p)
            rot.append(t.rotation)
            scl.append(t.scale3d)
        controller.set_bone_track_keys(bone, pos, rot, scl, False)
    controller.close_bracket(False)


def author_recoil(anim, num_frames, target_units, tag):
    rest_x = gun_component_x(anim, 0)
    # Probe the local->component scale with a unit pull, then key the real one.
    write_tracks(anim, num_frames, 1.0)
    per_local = rest_x - gun_component_x(anim, 4)
    if abs(per_local) < 1e-3:
        raise RuntimeError("%s: B_Gun local Y does not move the barrel along X" % tag)
    write_tracks(anim, num_frames, target_units / per_local)
    xs = [gun_component_x(anim, f) for f in range(0, num_frames + 1, 2)]
    log("%s recoil: rest X %.1f, %.2f component units per local unit, local back %.3f, X by frame %s" % (
        tag, rest_x, per_local, target_units / per_local, ["%.1f" % x for x in xs]))


def build_su122_fire():
    eal = unreal.EditorAssetLibrary
    for path in (SU122_MONTAGE, SU122_FIRE):
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
    anim = eal.duplicate_asset(SU122_TPOSE, SU122_FIRE)
    author_recoil(anim, 24, RECOIL_COMPONENT_UNITS["SU122"], "SU122")
    log("Anim_SU122_Fire saved=%s" % eal.save_asset(SU122_FIRE, False))
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", anim)
    factory.set_editor_property("target_skeleton", anim.get_editor_property("skeleton"))
    folder, name = SU122_MONTAGE.rsplit("/", 1)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimMontage, factory)
    log("Montage_FireSu122 created=%s len=%.2f saved=%s" % (montage is not None, montage.get_play_length() if montage else -1,
                                                           eal.save_asset(SU122_MONTAGE, False)))


def report_parents():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for path in ("SU85/Rus_Vehicle_Su85_Soviet", "SU85/Rus_Vehicle_Su85_Harvest", "Su100/Rus_Vehicle_Su100_Soviet",
                 "Su100/EmbTurret_SU100_Soviet", "Su100/Rus_Vehicle_Su100", "Su122/Rus_Vehicle_Su122"):
        data = registry.get_asset_by_object_path(R + path + "." + path.split("/")[-1])
        log("parent %s = %s" % (path, data.get_tag_value("ParentClass")))


# ---------------------------------------------------------------- graph edits through the UECP script bridge
# The bridge runs headless (-nullrhi, plugin enabled); a Blueprint is only saved when its compile has 0 errors.
import json
import sys

VEHICLE_MONTAGES = {
    R + "SU85/Rus_Vehicle_Su85": R + "SU85/Meshes/WeaponAnims/Montage_Su85_Recoil",
    R + "Su100/Rus_Vehicle_Su100": R + "SU85/Meshes/WeaponAnims/Montage_Su85_Recoil",
    R + "Su122/Rus_Vehicle_Su122": SU122_MONTAGE,
}


def uecp(action, category="blueprint", **params):
    raw = unreal.UECPScriptBridge.dispatch(category, action, json.dumps(params))
    try:
        return json.loads(raw, strict=False)
    except ValueError:
        return {"raw": raw}


def graph_nodes(blueprint, graph):
    result = uecp("get_blueprint_graph", blueprint_path=blueprint, graph_name=graph, detail="full",
                  include_unconnected_pins=True)
    return result.get("nodes", [])


def compile_and_save(blueprint):
    result = uecp("compile_blueprint", blueprint_path=blueprint)
    errors = result.get("error_count", result.get("errors"))
    log("compile %s -> %s" % (blueprint.split("/")[-1], json.dumps(result)[:400]))
    if errors not in (0, None, []):
        log("NOT SAVED %s (compile errors)" % blueprint)
        return False
    saved = unreal.EditorAssetLibrary.save_asset(blueprint, False)
    log("saved %s=%s" % (blueprint.split("/")[-1], saved))
    return saved


def fix_vehicle_montages():
    for blueprint, montage in VEHICLE_MONTAGES.items():
        nodes = [n for n in graph_nodes(blueprint, "EventGraph") if "PlayMontage" in n.get("handle", "") + n.get("title", "").replace(" ", "")]
        log("%s PlayMontage nodes: %s" % (blueprint.split("/")[-1], json.dumps(nodes)[:600]))
        if len(nodes) != 1:
            log("SKIP %s: expected one PlayMontage node" % blueprint)
            continue
        asset = "%s.%s" % (montage, montage.split("/")[-1])
        result = uecp("set_pin_default", blueprint_path=blueprint, graph_name="EventGraph", node_id=nodes[0]["id"],
                      pin_name="MontageToPlay", value=asset)
        log("set_pin_default %s -> %s" % (blueprint.split("/")[-1], json.dumps(result)[:400]))
        compile_and_save(blueprint)


def rewire_su122_turret_anim_reference():
    """ReferenceCasts: cast to AnimBP_Su122 -> set TurretAnimBp (was the stray 'As Anim BP Su 122')."""
    nodes = graph_nodes(TURRET_122, "ReferenceCasts")
    old_set = [n for n in nodes if n.get("handle", "") == "var.set.As Anim BP Su 122"]
    if len(old_set) != 1:
        log("SKIP turret rewire: old setters %s" % [n.get("handle") for n in nodes])
        return
    pins = {p["name"]: p for p in old_set[0].get("pins", [])}
    log("old setter pins: %s" % json.dumps(pins)[:1500])
    exec_in = (pins.get("execute", {}).get("connected_to") or [{}])[0]
    value_in = (pins.get("As Anim BP Su 122", {}).get("connected_to") or [{}])[0]
    exec_out = pins.get("then", {}).get("connected_to") or []
    if not exec_in.get("node_id") or not value_in.get("node_id"):
        log("SKIP turret rewire: setter inputs not linked")
        return
    placed = uecp("place_node", blueprint_path=TURRET_122, graph_name="ReferenceCasts", handle="var.set.TurretAnimBp",
                  id="SetTurretAnimBp", x=old_set[0].get("position", {}).get("x", 0),
                  y=old_set[0].get("position", {}).get("y", 0) + 160)
    log("place_node -> %s" % json.dumps(placed)[:800])
    new_id = placed.get("node_id") or placed.get("id") or placed.get("guid") or (placed.get("nodes") or [{}])[0].get("id")
    connections = [
        {"from": exec_in["node_id"], "from_pin": exec_in["pin"], "to": new_id, "to_pin": "execute"},
        {"from": value_in["node_id"], "from_pin": value_in["pin"], "to": new_id, "to_pin": "TurretAnimBp"},
    ] + [{"from": new_id, "from_pin": "then", "to": e["node_id"], "to_pin": e["pin"]} for e in exec_out]
    log("delete old -> %s" % json.dumps(uecp("delete_nodes", blueprint_path=TURRET_122, graph_name="ReferenceCasts",
                                             node_ids=[old_set[0]["id"]]))[:400])
    log("connect -> %s" % json.dumps(uecp("connect_pins", blueprint_path=TURRET_122, graph_name="ReferenceCasts",
                                          connections=connections))[:800])
    after = [n for n in graph_nodes(TURRET_122, "ReferenceCasts") if n.get("handle", "").startswith("var.set")]
    log("setters after: %s" % json.dumps(after)[:1500])
    compile_and_save(TURRET_122)


SU85_TPOSE = R + "SU85/Meshes/WeaponAnims/Anim_Su85_TPose"
SU85_RECOIL = R + "SU85/Meshes/WeaponAnims/Anim_Su85_Recoil"
SU85_RECOIL_MONTAGE = R + "SU85/Meshes/WeaponAnims/Montage_Su85_Recoil"


def build_recoil_from_tpose(tpose, anim_path, montage_path, target_units, tag):
    """New fire anim = copy of the gun's T-pose, B_Gun pulled back from its own rest, plus a DefaultSlot montage."""
    eal = unreal.EditorAssetLibrary
    for path in (montage_path, anim_path):
        if eal.does_asset_exist(path):
            eal.delete_asset(path)
    anim = eal.duplicate_asset(tpose, anim_path)
    author_recoil(anim, 24, target_units, tag)
    log("%s saved=%s" % (anim_path.split("/")[-1], eal.save_asset(anim_path, False)))
    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", anim)
    factory.set_editor_property("target_skeleton", anim.get_editor_property("skeleton"))
    folder, name = montage_path.rsplit("/", 1)
    montage = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.AnimMontage, factory)
    log("%s created=%s saved=%s" % (name, montage is not None, eal.save_asset(montage_path, False)))


def build_su85_recoil():
    build_recoil_from_tpose(SU85_TPOSE, SU85_RECOIL, SU85_RECOIL_MONTAGE, RECOIL_COMPONENT_UNITS["SU85"], "SU85")


STEPS = {"parents": report_parents, "su85recoil": build_su85_recoil, "montages": fix_vehicle_montages,
         "turret": rewire_su122_turret_anim_reference, "su122anim": build_su122_fire}
for name in (sys.argv[1:] or ["su85recoil", "montages", "turret"]):
    try:
        STEPS[name]()
    except Exception as error:
        log("FAILED %s: %s" % (name, error))
unreal.SystemLibrary.quit_editor()
