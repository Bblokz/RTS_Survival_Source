"""Job 1b: skeletal running gear, track anims and physics asset. Run: python pw_uecp.py py pw_common.py pw_import_sk.py"""
assert_no_pie()
before = dirty_names()
M = lambda n: MATS + '/' + n
# ---------------- skeletal running gear + track anims ----------------
AT.import_asset_tasks([fbx_task(EXP + 'SK_PanzerWerfer_Tracks.fbx', MESHES, 'SK_PanzerWerfer_Tracks', skeletal_ui())])
sk = load(MESHES + '/SK_PanzerWerfer_Tracks')
skel = sk.get_editor_property('skeleton')
print('SK skeleton', skel.get_path_name())
mats = sk.get_editor_property('materials')
slot_mi = {'M_PanzerWerfer_FrontWheel': M('MI_PanzerWerfer_FrontWheel'), 'M_PanzerWerfer_Wheels': M('MI_PanzerWerfer_Wheels'),
           'M_LeftTrack': M('MI_PanzerWerfer_Tracks'), 'M_RightTrack': M('MI_PanzerWerfer_Tracks')}
for i in range(len(mats)):
    m = mats[i]
    slot = str(m.get_editor_property('material_slot_name'))
    if slot in slot_mi:
        m.set_editor_property('material_interface', load(slot_mi[slot]))
        mats[i] = m
    print('  SK slot', i, slot)
sk.set_editor_property('materials', mats)
b = sk.get_bounds()
print('SK bounds', b.origin, b.box_extent)

# Anims: the legacy importer names them <Mesh>_Anim_...; move them to Anims/ under the project names.
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for ad in reg.get_assets_by_path(MESHES, recursive=False):
    if str(ad.asset_class_path.asset_name) != 'AnimSequence':
        continue
    src = str(ad.package_name)
    short = src.split('/')[-1]
    for key, new in ANIM_RENAMES.items():
        if short.endswith(key):
            dst = ANIMS + '/' + new
            if EAL.does_asset_exist(dst):
                EAL.delete_asset(dst)
            EAL.rename_asset(src, dst)
            a = load(dst)
            print('  anim', new, round(a.get_play_length(), 3), a.get_editor_property('skeleton').get_name())
            save(a)

# Physics asset: one body on the root bone 'Tracks' with wheel spheres (TrackPhysicsMovement rule), from the 250's.
pa_path = MESHES + '/PA_PanzerWerfer_Tracks'
if not EAL.does_asset_exist(pa_path):
    EAL.duplicate_asset(SDKFZ250 + 'Meshes/SK_Sdkfz_250_Tracks_PhysicsAsset', pa_path)
pa = load(pa_path)
spheres = []
for side, y in (('L', -88.0), ('R', 88.0)):
    for i, x in enumerate((155.6, 20.0, -110.0, -250.0)):
        spheres.append('(Center=(X=%.3f,Y=%.3f,Z=43.381862),Radius=45.000000,Name="%s%d")' % (x, y, side, i + 1))
body = None
for i in range(64):
    body = unreal.find_object(None, pa.get_path_name() + ':SkeletalBodySetup_%d' % i)
    if body:
        break
print('PA body', body.get_name() if body else None, body.get_editor_property('bone_name') if body else None)
agg = body.get_editor_property('agg_geom')
agg.import_text('(SphereElems=(%s))' % ','.join(spheres))
body.set_editor_property('agg_geom', agg)
save(pa)
sk.set_editor_property('physics_asset', pa)
save(sk)
save(skel)
print('DIRTY AFTER', [d for d in dirty_names() if d not in before])
print('IMPORT DONE')
