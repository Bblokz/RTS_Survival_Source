"""Manifest rows for the Phytophage-infested drowned trees (phyto_trees_import.py). Exec after the import (headless
pythonscript run or a throwaway editor's command namespace)."""
_PTH = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/Trees'
_PT_PIVOT = ('as SM_RTS_DeadTree_1: trunk base at z 0 with roots to -43 cm, +Z up; swaps in place for a BogTree_0 '
             'instance (scale 0.55-1.05, dz -30, pitch/roll +-12 like the drowned forest)')
_PT_COMMON = ('Two slots: Tree = MI_Env_DeadTree_1 (shared bark, unchanged), Phytophage = MI_PhytoTree (M_PhytoTree: '
              'opaque two-sided, wet gloss + macro wetness, panning normal, Fresnel fungal hairs, very slow sine WPO on '
              'vertex colour R, vein/pod pulse on G; ISM usage on). 3 LODs (1.0/0.45/0.26 like the source), the source '
              "tree's six convex hulls (simple-as-complex). Use as HISM decoration in the bog (no collision needed there) "
              'or the BP_PhytoTree_* wrapper where units can reach it. Set a WPO disable distance on the HISM (the '
              'wrappers do it through WpoDistanceManager).')
_PT_STAGES = [
    ('creeping', 'Creeping', 'Stage 1, early: a tar collar out of the water, mats creeping up the trunk in tongues and '
     'over the tops of the first limbs and the main fork, veins climbing ahead of the mats, a few pod clusters, short '
     'drips. Crown still bare.'),
    ('shrouded', 'Shrouded', 'Stage 2, heavy: trunk under thick mats with spiral veins; the inner crown webbed - taut '
     'sheets bridge every fork and tent between the limbs, torn open in places, ropes across forks with drips, many '
     'swollen and shrivelled pods. Limb ends still out. Reads as a webbed purple crown from the RTS camera.'),
    ('cocooned', 'Cocooned', 'Stage 3, suffocated: one lumpy, sagging cocoon over trunk and crown, only branch tips '
     'out, long drips toward the water, big swollen pods, fat veins over the shroud; a fan of orange Radixite shards '
     'breaks out low on one flank and a pair up in the crown (the Tar-Blight setting into fuel). Reads as a solid blob '
     'from above.'),
]
PRY_MANIFEST_ENTRIES = []
for _id, _st, _desc in _PT_STAGES:
    PRY_MANIFEST_ENTRIES.append(dict(
        id='phytotree_' + _id, kit='Phytophage', asset_path=_PTH + '/SM_PhytoTree_' + _st, kind='static_mesh',
        blocks_vehicles=False, destructible='none', pivot=_PT_PIVOT,
        notes='Drowned dead tree (SM_RTS_DeadTree_1) overgrown by Phytophage. ' + _desc + ' ' + _PT_COMMON,
        preview='D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Phytophage/Trees/Preview/%d_%s_top.png'
                % (['creeping', 'shrouded', 'cocooned'].index(_id) + 1, _st)))
    PRY_MANIFEST_ENTRIES.append(dict(
        id='phytotree_' + _id + '_bp', kit='Phytophage', asset_path=_PTH + '/BP_PhytoTree_' + _st, kind='blueprint',
        parent='duplicate of BP_PolesieDeadSpruce_02_BogLean (AWpoTree child, as the DeadSpruces kit)',
        blocks_vehicles=True, destructible='crushable', pivot=_PT_PIVOT,
        notes='Crushable wrapper of SM_PhytoTree_%s: TreeMesh + Tree component set to the variant; TankDestructionType '
              'MediumOrHeavyTank like the spruce kit (medium and heavy tanks crush it, light vehicles path round); '
              'WpoDistanceManager turns the WPO off with distance. The source tree\'s own wrapper '
              '(BP_Scorch_Dest_DeadTree, GC + wall holes) picks a random tree 1-3 in its construction script, so it '
              'could not carry a fixed variant. Crush/death not play-tested. Rebuild nav after placing.' % _st))
exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets/manifest_add.py').read())
