"""Job 2: AnimBp_PanzerWerfer_Tracks = duplicate of AnimBp_Sdkfz-250_Tracks (TrackedAnimationInstance, blend by
ETrackAnimation) on the Panzerwerfer skeleton with its 7 track anims. Run: python pw_uecp.py py pw_common.py pw_animbp.py, then pw_animbp_nodes.py (outside the editor) swaps the
sequence players with UECP set_anim_node_property and compiles."""
import json

assert_no_pie()
before = dirty_names()
SRC_ABP = SDKFZ250 + 'AnimBp_Sdkfz-250_Tracks'
DST_ABP = PW + '/AnimBp_PanzerWerfer_Tracks'
SEQ_MAP = {
    'Anim_Sdkfz-250_Tracks_FWD': 'Anim_PanzerWerfer_Tracks_FWD',
    'Anim_Sdkfz-250_Tracks_BWD': 'Anim_PanzerWerfer_Tracks_BWD',
    'Anim_Sdkfz-250_Tracks_TPose': 'Anim_PanzerWerfer_Tracks_TPose',
    'Anim_Sdkfz-250_Tracks_TurnLeft': 'Anim_PanzerWerfer_Tracks_TurnLeft',
    'Anim_Sdkfz-250_Tracks_TurnRight': 'Anim_PanzerWerfer_Tracks_TurnRight',
    'Anim_Sdkfz-250_Tracks_StationaryLeft': 'Anim_PanzerWerfer_Tracks_StationaryLeft',
    'Anim_Sdkfz-250_Tracks_StationaryRight': 'Anim_PanzerWerfer_Tracks_StationaryRight',
}
if not EAL.does_asset_exist(DST_ABP):
    EAL.duplicate_asset(SRC_ABP, DST_ABP)
abp = load(DST_ABP)
skel = load(MESHES + '/SK_PanzerWerfer_Tracks_Skeleton')
abp.set_editor_property('target_skeleton', skel)
sk = load(MESHES + '/SK_PanzerWerfer_Tracks')
try:
    abp.set_editor_property('preview_skeletal_mesh', sk)
except Exception as e:
    print('preview mesh not set', e)

save(abp)
print('DIRTY AFTER', [d for d in dirty_names() if d not in before])
