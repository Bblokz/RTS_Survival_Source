# Copies UnitMaster casual anims (carry, walk, grab, drop) onto the InfantryMaster skeleton for the air supply workers.
# Both skeletons have the same 68 bones and reference pose (probe_skeletons.py, 2026-11-24), so local keys copy 1:1;
# the infantry-only Shovel_mount bone keeps its reference pose.
# Headless: UnrealEditor-Win64-DebugGame-Cmd <uproject> -run=pythonscript -script=<this file>
import unreal

CASUAL = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/CasualAnims/'
SRC_MESH = CASUAL + 'SKMesh_UnitMaster'
TGT_MESH = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier'
TEMPLATE = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/Movement/Anim_SquadUnit_Jog'
OUT_DIR = '/Game/RTS_Survival/Blueprints/Environment/SovietAirSupply/Anims/'
JOBS = {
    'AN_AirSupply_CarryWood': 'Anim_Holding_Wood',
    'AN_AirSupply_CarryBucket': 'Anim_Bucket_Walk_Forward',
    'AN_AirSupply_Walk': 'Anim_Male_Walk',
    'AN_AirSupply_PickUp': 'Anim_Grabbing_Bucket',
    'AN_AirSupply_PutDown': 'Anim_Dropping_Bucket',
    'AN_AirSupply_IdleHold': 'Anim_Bucket_Idle',
}
LOG = unreal.Paths.project_saved_dir() + 'SovietAirSupply/copy_anims.txt'
lines = []
lib = unreal.AnimationLibrary
eal = unreal.EditorAssetLibrary
A = unreal.AnimPoseExtensions

src_mesh = unreal.load_asset(SRC_MESH)
tgt_skel = unreal.load_asset(TGT_MESH).skeleton
tgt_ref = A.get_reference_pose(tgt_skel)
tgt_bones = [str(b) for b in A.get_bone_names(tgt_ref)]

for out_name, src_name in JOBS.items():
    seq = unreal.load_asset(CASUAL + src_name)
    track_names = [str(n) for n in lib.get_animation_track_names(seq)]
    # The template is 30 fps and its controller only takes multiples/factors of that: resample by time at 30 fps.
    fps = 30
    frame_rate = unreal.FrameRate(fps, 1)
    length = seq.get_play_length()
    num_frames = max(1, int(round(length * fps)))
    num_keys = num_frames + 1
    per_bone = {b: ([], [], []) for b in tgt_bones}
    for frame in range(num_keys):
        t = min(length, frame / float(fps))
        poses = lib.get_bone_poses_for_time(seq, track_names, t, False, src_mesh)
        local = dict(zip(track_names, poses))
        for b in tgt_bones:
            t = local.get(b)
            if t is None:
                t = A.get_bone_pose(tgt_ref, b, unreal.AnimPoseSpaces.LOCAL)
            per_bone[b][0].append(t.translation)
            per_bone[b][1].append(t.rotation)
            per_bone[b][2].append(unreal.Vector(1, 1, 1))
    out_path = OUT_DIR + out_name
    if eal.does_asset_exist(out_path):
        eal.delete_asset(out_path)
    new_seq = eal.duplicate_asset(TEMPLATE, out_path)
    ctrl = new_seq.controller
    ctrl.open_bracket(unreal.Text('Copy ' + src_name))
    ctrl.set_frame_rate(frame_rate)
    ctrl.set_number_of_frames(unreal.FrameNumber(num_keys - 1))
    ctrl.remove_all_bone_tracks()
    for b in tgt_bones:
        ctrl.add_bone_curve(unreal.Name(b))
        pos, rot, scl = per_bone[b]
        ctrl.set_bone_track_keys(unreal.Name(b), pos, rot, scl)
    ctrl.close_bracket()
    new_seq.set_editor_property('enable_root_motion', False)
    eal.save_asset(out_path, only_if_is_dirty=False)
    lines.append('%s <- %s frames=%d rate=%s len=%.2f (src %.2f)' % (
        out_name, src_name, lib.get_num_frames(new_seq), frame_rate, new_seq.get_play_length(),
        seq.get_play_length()))

with open(LOG, 'w') as fh:
    fh.write('\n'.join(lines))
