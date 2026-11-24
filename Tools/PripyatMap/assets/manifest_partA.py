"""Manifest rows for Part A kits 3-6 (village, signs, memorial, phytophage, wrecks)."""
_R = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/'
_CR = ('Any tank crushes it (AnyTank; copy of BP_BerlinFurniture_Base: overlap -> lean/fall + sink 2 s), infantry path '
       'round (RTS_NavArea_BlockChar box), shell holes while alive. Scale 1, yaw free. Rebuild nav after placing.')
_SH = ('ADestructableEnvActor + WallHoleDamage (procedural shell holes, NoGrid) + NavBlock_NN boxes (NavArea_Null, '
       'dynamic obstacles). Blocks everything while alive; at death mesh hides and throws debris pieces, nav boxes go '
       'with the actor after 4.5 s. Scale 1 (holes are mesh space).')
PRY_MANIFEST_ENTRIES = [
    dict(id='village_loghouse_a', kit='Village', asset_path=_R + 'Village/Blueprints/BP_PolesieLogHouse_A', kind='blueprint',
         blocks_vehicles=True, destructible='wall_holes', pivot='ground centre of the house; ~7.7 x 7.3 m, 4.9 m tall',
         notes='Heavily damaged reed-thatch log house with creepers. Health 1400, DR 8. ' + _SH +
               ' Door side not checked: look once and yaw it towards the road. Purple-stained cottage on burning peat: '
               'surround with SM_Phytophage_Mat/Curtain + smoke.'),
    dict(id='village_loghouse_b', kit='Village', asset_path=_R + 'Village/Blueprints/BP_PolesieLogHouse_B', kind='blueprint',
         blocks_vehicles=True, destructible='wall_holes', pivot='ground centre; ~6.7 x 6.2 m, 4.9 m tall',
         notes='Second log house (missing thatch patch). Health 1300, DR 8. ' + _SH),
    dict(id='village_storknest_a', kit='Village', asset_path=_R + 'Village/Blueprints/BP_PolesieStorkNestPole_A',
         kind='blueprint', blocks_vehicles=False, destructible='crushable', pivot='pole foot on the ground; 5.4 m tall',
         notes='Stork nest pole (empty cartwheel nest). Health 200. ' + _CR),
    dict(id='village_storknest_b', kit='Village', asset_path=_R + 'Village/Blueprints/BP_PolesieStorkNestPole_B',
         kind='blueprint', blocks_vehicles=False, destructible='crushable', pivot='pole foot on the ground; 5.7 m tall',
         notes='Stork nest pole B. Health 200. ' + _CR),
    dict(id='village_sweepwell', kit='Village', asset_path=_R + 'Village/Blueprints/BP_PolesieSweepWell', kind='blueprint',
         blocks_vehicles=False, destructible='crushable',
         pivot='well shaft centre on the ground; the sweep beam/support extends to -X (3.4 m)',
         notes='Wooden sweep well (zhuravel). Health 220. ' + _CR),
    dict(id='memorial_partisan', kit='Memorial', asset_path=_R + 'Memorial/Blueprints/BP_PolesiePartisanMemorial',
         kind='blueprint', blocks_vehicles=True, destructible='wall_holes',
         pivot='foundation centre on the ground; 2.6 x 2.3 m base, obelisk top 5.1 m',
         notes='Partisan obelisk with faded red star and "road builders" lettering; the inscription/star face +Y '
               '(yaw so +Y faces the west road). Mesh socket AudioLog (73, 102, 70) at the iron audio-log box. '
               'Health 2500, DR 12. ' + _SH + ' Materials: airfield ridge concrete, faded red, airfield prop metal.'),
    dict(id='wreck_causeway_block', kit='Wrecks', asset_path=_R + 'Wrecks/SM_PrypyatCausewayWreckBlock',
         kind='static_mesh', blocks_vehicles=True, destructible='none',
         pivot='ground centre; 10.8 m along X, 13 m along Y',
         notes='Burnt Panther/PzIII/T-34/BT-7 pile-up that seals a 10 m causeway choke. One UCX box 10.8 x 3.0 x 2.7 m '
               'across X: yaw so X runs ACROSS the lane (lane along Y). Wreck materials (MI_SavObj_*, M_*_Destroyed).'),
    dict(id='wreck_downed_ju87', kit='Wrecks', asset_path=_R + 'Wrecks/SM_PrypyatDownedJU87', kind='static_mesh',
         blocks_vehicles=True, destructible='none', pivot='ground under the fuselage; 10 x 12 m incl. swept wing',
         notes='Crashed Ju 87 for the downed-pilot sites (crushed gear, folded wing). One simple box collision over the '
               'whole bounds (blocks vehicles; keep it off lanes). Material M_Stuka_JU87_Frame; mesh has 3 UV sets, '
               'texture mapping not checked in a viewport. Pilot SK not imported (skeletal; rescue gameplay not built).'),
]
for _n, _place, _sz in [('Curtain_01', 'hang from fences/eaves/branches; +Z up, sheet in XZ at y<=0', '4 x 2.3 m'),
                        ('Curtain_02', 'as Curtain_01', '4 x 2.3 m'),
                        ('Corner_01', 'window/door corner web; spans +X 3 m', '3 x 2.7 m'),
                        ('Corner_02', 'as Corner_01', '3 x 2.7 m'),
                        ('Wrap_01', 'sleeve round a trunk/pillar/pipe, r 0.68 m', '1.5 x 1.3 x 2.4 m'),
                        ('Wrap_02', 'as Wrap_01', '1.5 x 1.3 x 2.4 m'),
                        ('Mat_01', 'flat on ground/water/hull (algae mat), z 4-21 cm', '3.2 x 3.2 m'),
                        ('Mat_02', 'as Mat_01', '3.2 x 3.2 m'),
                        ('Vein', 'raised strand along +X', '3 m'),
                        ('Pustules', 'small colony cluster', '0.4 m')]:
    PRY_MANIFEST_ENTRIES.append(dict(
        id='phytophage_' + _n.lower(), kit='Phytophage', asset_path=_R + 'Phytophage/SM_Phytophage_' + _n,
        kind='static_mesh', blocks_vehicles=False, destructible='none', pivot=_place,
        notes='Purple Phytophage growth, %s, 3 LODs (1.0/0.25/0.08), no collision. Two-sided masked web material '
              '(MI_Phytophage_VioletShroud; AshMauve and BlackPlum instances exist to swap) or opaque BlackPlum_Solid. '
              'Use ISM/HISM for repeats; disable shadows on actors; mats on water = algae mats. Masked cards not '
              'checked from the RTS camera (mip fade).' % _sz))
for _n in ('RoadSign_BlankRoad', 'RoadSign_Mozyr', 'RoadSign_Pinsk', 'RoadSign_Stolin', 'RoadSign_Turov',
           'RailSign_BlankRail', 'RailSign_Kalinkavichy', 'RailSign_Luninets'):
    _rail = _n.startswith('Rail')
    PRY_MANIFEST_ENTRIES.append(dict(
        id='sign_' + _n.lower(), kit='Signs', asset_path=_R + 'Signs/Blueprints/BP_Polesie' + _n, kind='blueprint',
        blocks_vehicles=False, destructible='crushable',
        pivot='ground centre between the posts; board in the XZ plane, %s wide' % ('3.25 m' if _rail else '2.35 m'),
        notes='Bilingual Cyrillic/Latin %s sign. Board faces -Y in Blender = +Y in UE as imported (check which side '
              'carries the text, then yaw it to face the incoming lane). Health 120. ' % ('rail' if _rail else 'road') + _CR))
exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets/manifest_add.py').read())
