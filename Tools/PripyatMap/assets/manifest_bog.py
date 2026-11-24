"""Manifest rows for the bog foliage kit (lead's extra item)."""
_B = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/BogFoliage/SM_PrypyatBog_'
_COMMON = (' No collision, no nav (HISM scatter); turn off dynamic shadow on the HISM if wanted. Pivot at the clump '
           'centre on the ground/water line, +Z up, yaw and 0.85-1.2 uniform scale free.')
PRY_MANIFEST_ENTRIES = []
for _n, _note in [
        ('Reed_A', 'Dead straw Phragmites clump, 2.4 m tall, ~1.5-2 m across incl. 3 lodged (near-flat) cards that read from above; card material MI_Prypyat_BogFoliage (masked two-sided, OpacityBoost 2.0, alpha-coverage mips).'),
        ('Reed_B', 'Smaller reed clump, 1.85 m, 2 lodged cards.'),
        ('Cattail_A', 'Cattail/bulrush clump 1.5 m leaves, six chunky brown heads at 1.2-1.8 m.'),
        ('Sedge_A', 'Sedge tussock: opaque straw/olive dome (~1.0 m wide, top 0.57 m) with arching blade cards; base 12 cm below pivot so it sits in shallow water.'),
        ('Sedge_B', 'Larger sedge tussock (~1.45 m wide, 0.8 m).'),
        ('AlgaeMat_A', 'Floating purple Phytophage algae mat, 3.2 x 2.4 m, z 2-5 cm (place at the water surface). Phytophage Mat_01 UVs + MI_Phytophage_VioletShroud (swap to _BlackPlum for darker).'),
        ('AlgaeMat_B', 'Floating algae mat, 4.9 x 3.6 m.'),
        ('Snag_A', 'Dead bog birch snag 2.9 m, leaning, one stub branch, dead-tree bark (MI_Env_DeadTree_1). Base 16 cm below pivot.'),
        ('Snag_B', 'Dead snag 4.2 m, two stub branches.')]:
    PRY_MANIFEST_ENTRIES.append(dict(id='bog_' + _n.lower(), kit='BogFoliage', asset_path=_B + _n, kind='static_mesh',
                                     blocks_vehicles=False, destructible='none', pivot='clump centre on the ground',
                                     notes=_note + _COMMON,
                                     preview='D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PripyatBogFoliage/Preview/BogFoliage_Preview.png'))
exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets/manifest_add.py').read())
