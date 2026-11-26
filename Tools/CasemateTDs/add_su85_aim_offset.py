"""2026-11-26: give the SU-85 / SU-100 gun AnimBP an aim offset (it had none, so the gun never visibly aimed).

Mirrors AnimBP_Su122: T-pose sequence -> RotationOffsetBlendSpace (X = Yaw, Y = Pitch of UTurretAnimInstance)
-> LayeredBoneBlend base pose; the fire slot still drives B_Gun only, so recoil plays on top of the aim.

Run headless: UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script="<this> [probe|apply] [AO name]"
  -nullrhi -DisablePlugins=Fab   (UECP script bridge must load; see memory uecp-headless-script-bridge)
probe: prints the aim offsets' samples and the AnimGraph nodes. apply: adds + wires the node, compiles, saves on 0 errors.
"""
import json
import sys

import unreal

W = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/SU85/Meshes/"
ANIM_BP = W + "Weapon/SK_Su85_WeaponSystem_Skeleton_AnimBlueprint"
AIM_OFFSETS = {"AO_Su85": W + "WeaponAnims/AO_Su85", "AO_SU85New": W + "WeaponAnims/AO_SU85New"}


def log(msg):
    unreal.log("SU85_AO " + msg)


def uecp(action, category="animation", **params):
    raw = unreal.UECPScriptBridge.dispatch(category, action, json.dumps(params))
    try:
        return json.loads(raw, strict=False)
    except ValueError:
        return {"raw": raw}


def anim_nodes():
    return uecp("get_anim_graph_nodes", anim_blueprint_path=ANIM_BP).get("nodes", [])


def probe():
    for name, path in AIM_OFFSETS.items():
        ao = unreal.load_asset(path)
        for sample in ao.get_editor_property("sample_data"):
            anim = sample.get_editor_property("animation")
            value = sample.get_editor_property("sample_value")
            mantlet = unreal.AnimationLibrary.get_bone_pose_for_frame(anim, "B_Mantlet", 0, False).rotation.rotator()
            turret = unreal.AnimationLibrary.get_bone_pose_for_frame(anim, "B_Turret", 0, False).rotation.rotator()
            log("%s (%.0f, %.0f) %s additive=%s base=%s mantlet(p%.1f y%.1f r%.1f) turret(p%.1f y%.1f r%.1f)" % (
                name, value.x, value.y, anim.get_name(), anim.get_editor_property("additive_anim_type"),
                anim.get_editor_property("ref_pose_seq").get_name() if anim.get_editor_property("ref_pose_seq") else None,
                mantlet.pitch, mantlet.yaw, mantlet.roll, turret.pitch, turret.yaw, turret.roll))
    log("nodes %s" % json.dumps(anim_nodes())[:4000])


def node_of(nodes, node_class, asset_hint=None):
    found = [n for n in nodes if n.get("node_class") == node_class and (asset_hint is None or asset_hint in json.dumps(n))]
    return found


def apply(ao_name):
    nodes = anim_nodes()
    if node_of(nodes, "AnimGraphNode_RotationOffsetBlendSpace"):
        log("SKIP: AnimBP already has an aim offset node")
        return
    blends = node_of(nodes, "AnimGraphNode_LayeredBoneBlend")
    sequences = node_of(nodes, "AnimGraphNode_SequencePlayer")
    # The base-pose player is the sequence player wired to the layered blend, not the one inside the slot.
    base = [s for s in sequences if blends and blends[0]["node_guid"] in json.dumps(s)]
    if len(blends) != 1 or len(base) != 1:
        log("SKIP: blends=%d base players=%d -> %s" % (len(blends), len(base), json.dumps(nodes)[:2000]))
        return
    pos = base[0].get("position") or {"x": base[0].get("pos_x", 0), "y": base[0].get("pos_y", 0)}
    added = uecp("add_anim_node", anim_blueprint_path=ANIM_BP, node_class="AnimGraphNode_RotationOffsetBlendSpace",
                 position_x=pos.get("x", 0) + 260, position_y=pos.get("y", 0))
    log("add_anim_node -> %s" % json.dumps(added)[:1500])
    ao_guid = added.get("node_guid")
    steps = [
        ("set_anim_node_property", dict(node_guid=ao_guid, property_name="BlendSpace", property_value=AIM_OFFSETS[ao_name])),
        ("bind_anim_pin", dict(node_guid=ao_guid, pin="X", property_path="Yaw")),
        ("bind_anim_pin", dict(node_guid=ao_guid, pin="Y", property_path="Pitch")),
        ("connect_anim_nodes", dict(source_node_guid=base[0]["node_guid"], target_node_guid=ao_guid, target_pin="BasePose")),
        ("connect_anim_nodes", dict(source_node_guid=ao_guid, target_node_guid=blends[0]["node_guid"], target_pin="BasePose")),
    ]
    for action, params in steps:
        log("%s -> %s" % (action, json.dumps(uecp(action, anim_blueprint_path=ANIM_BP, **params))[:600]))
    log("nodes after %s" % json.dumps(anim_nodes())[:4000])
    compiled = uecp("compile_blueprint", category="blueprint", blueprint_path=ANIM_BP)
    log("compile -> %s" % json.dumps(compiled)[:800])
    if compiled.get("error_count") == 0:
        log("saved=%s" % unreal.EditorAssetLibrary.save_asset(ANIM_BP, False))
    else:
        log("NOT SAVED (compile errors)")


args = sys.argv[1:]
try:
    if args and args[0] == "apply":
        apply(args[1] if len(args) > 1 else "AO_Su85")
    else:
        probe()
except Exception as error:
    log("FAILED %s" % error)
unreal.SystemLibrary.quit_editor()
