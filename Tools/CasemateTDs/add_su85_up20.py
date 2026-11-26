"""2026-11-26: let the SU-85 / SU-100 barrel follow its full +20 deg elevation.

AO_Su85 only had Up10 / Down10 / Left15 / Right15 (pitch axis -10..10) while EmbTurret_SU85/100 allow -6..20, so the
barrel stopped rising at 10 deg. This adds Anim_Su85_Up20 (Up10 with the B_Mantlet tilt doubled: local yaw 90 rest,
80 at Up10, 70 at Up20; same additive mesh-space rotation offset on Anim_Su85_TPose), a (0, 20) sample, and widens the
pitch axis to -10..20 with 3 grid divisions so the samples stay on the 10-deg grid.

Run headless: UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script="<this>" -nullrhi
  -DisablePlugins=Fab   (UECP script bridge must load; see memory uecp-headless-script-bridge)
"""
import json

import unreal

W = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/SU85/Meshes/"
ANIM_BP = W + "Weapon/SK_Su85_WeaponSystem_Skeleton_AnimBlueprint"
AO = W + "WeaponAnims/AO_Su85"
TPOSE = W + "WeaponAnims/Anim_Su85_TPose"
UP10 = W + "WeaponAnims/Anim_Su85_Up10"
UP20 = W + "WeaponAnims/Anim_Su85_Up20"
MANTLET = "B_Mantlet"
PITCH_AXIS = dict(axis_index=1, axis_min=-10.0, axis_max=20.0, grid_divisions=3)

AL = unreal.AnimationLibrary
APE = unreal.AnimPoseExtensions
EAL = unreal.EditorAssetLibrary


def log(msg):
    unreal.log("SU85_UP20 " + msg)


def uecp(action, category="animation", **params):
    raw = unreal.UECPScriptBridge.dispatch(category, action, json.dumps(params))
    try:
        return json.loads(raw, strict=False)
    except ValueError:
        return {"raw": raw}


def rot(anim, bone):
    return AL.get_bone_pose_for_frame(anim, bone, 0, False).rotation


def evaluated_mantlet(anim):
    pose = APE.get_anim_pose_at_frame(anim, 0, unreal.AnimPoseEvaluationOptions())
    return APE.get_bone_pose(pose, MANTLET, unreal.AnimPoseSpaces.LOCAL).rotation.rotator()


def author_up20():
    if EAL.does_asset_exist(UP20):
        EAL.delete_asset(UP20)
    tpose, up10 = unreal.load_asset(TPOSE), unreal.load_asset(UP10)
    # Up10 delta in the mantlet's local frame, applied twice.
    delta = rot(up10, MANTLET) * rot(tpose, MANTLET).inversed()
    up20_mantlet = delta * rot(up10, MANTLET)
    anim = EAL.duplicate_asset(UP10, UP20)
    num_frames = AL.get_num_frames(anim)
    rest = {str(b): AL.get_bone_pose_for_frame(anim, b, 0, False) for b in AL.get_animation_track_names(anim)}
    controller = anim.get_editor_property("controller")
    controller.open_bracket(unreal.Text("SU85 Up20"), False)
    controller.set_number_of_frames(unreal.FrameNumber(num_frames), False)
    for bone, t in rest.items():
        keys = num_frames + 1
        q = up20_mantlet if bone == MANTLET else t.rotation
        controller.set_bone_track_keys(bone, [t.translation] * keys, [q] * keys, [t.scale3d] * keys, False)
    controller.close_bracket(False)
    log("frames=%d additive=%s base=%s" % (num_frames, anim.get_editor_property("additive_anim_type"),
                                            anim.get_editor_property("ref_pose_seq").get_name()))
    log("mantlet raw tpose %s up10 %s up20 %s" % (rot(tpose, MANTLET).rotator(), rot(up10, MANTLET).rotator(),
                                                   rot(anim, MANTLET).rotator()))
    log("mantlet evaluated up10 %s up20 %s" % (evaluated_mantlet(up10), evaluated_mantlet(anim)))
    log("Up20 saved=%s" % EAL.save_asset(UP20, False))


def extend_aim_offset():
    log("axis -> %s" % json.dumps(uecp("set_blendspace_axis", blendspace_path=AO, **PITCH_AXIS))[:600])
    info = uecp("get_blendspace_info", blendspace_path=AO)
    if UP20.rsplit("/", 1)[1] not in json.dumps(info):
        log("sample -> %s" % json.dumps(uecp("add_blendspace_sample", blendspace_path=AO, animation_path=UP20,
                                             sample_x=0.0, sample_y=20.0))[:600])
    log("info -> %s" % json.dumps(uecp("get_blendspace_info", blendspace_path=AO))[:2500])
    log("AO saved=%s" % EAL.save_asset(AO, False))


def recompile_anim_bp():
    compiled = uecp("compile_blueprint", category="blueprint", blueprint_path=ANIM_BP)
    log("compile -> %s" % json.dumps(compiled)[:800])
    if compiled.get("error_count") == 0:
        log("AnimBP saved=%s" % EAL.save_asset(ANIM_BP, False))


try:
    author_up20()
    extend_aim_offset()
    recompile_anim_bp()
except Exception as error:
    log("FAILED %s" % error)
unreal.SystemLibrary.quit_editor()
