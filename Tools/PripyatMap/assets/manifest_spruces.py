"""Manifest rows for the Polesie dead spruces (Part A2)."""
_SPH = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/DeadSpruces'
PRY_MANIFEST_ENTRIES = []
for _k, _h in [('01_TallSentinel', 15.5), ('02_BogLean', 12.0), ('03_SnappedCrown', 9.0), ('04_WindFlag', 13.4),
               ('05_OldGiant', 17.2), ('06_DrownedYoung', 7.2), ('07_ForkedSnag', 12.8), ('08_Uprooted', 3.3)]:
    PRY_MANIFEST_ENTRIES.append(dict(
        id='spruce_' + _k, kit='DeadSpruces', asset_path=_SPH + '/BP_PolesieDeadSpruce_' + _k, kind='blueprint',
        parent='duplicate of BP_OreDeadSpruce_Skeleton_11m (AWpoTree child)', blocks_vehicles=True,
        destructible='crushable',
        pivot=('trunk base on the ground, +Z up; root plate at origin, trunk lies along +X (11 m)' if _k == '08_Uprooted'
               else 'trunk base on the ground, +Z up (roots extend 9 cm below)'),
        notes='%.1f m dead Polesie spruce, 3 LODs, MI_Env_DeadTree_1, one UCX trunk box (crush trigger, blocks light '
              'vehicles/infantry). TankDestructionType MediumOrHeavyTank like the Ore kit: medium+heavy tanks crush, '
              'light vehicles path round. Yaw free; the WpoTree BP may randomise scale/yaw like the Ore trees. '
              'Sink roots ~5-10 cm. Opaque bark only (no alpha cards).' % _h))
exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets/manifest_add.py').read())
