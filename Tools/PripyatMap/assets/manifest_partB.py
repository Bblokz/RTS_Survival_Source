"""Manifest rows for Part B (crossings, boardwalk, masts), the existing-asset picks (rail track, silo) and the airfield
look-fix notes."""
_X = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Crossings/'
_M = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/RadioMast/'
_PX = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PripyatCrossings/Preview/'
_PM = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PripyatRadioMast/Preview/'
_NOCOL = ' No collision, no nav effect: units drive/walk on the landscape under it.'
_ROWS = [
    ('railbridge_deck', _X + 'SM_PrypyatRailBridge_Deck', False,
     'centre of the crossing on the lane surface (z 0); 22 m along X (lane direction) x 10 m along Y; top 27 cm',
     'North rail bridge deck overlay: planks across the lane (some missing/charred, landscape shows through), '
     'sleepers + Russian-gauge rails along X (one rail sprung near +X), steel edge stringers. Place at bridge_n, '
     'X along the rail lane, z = lane surface (sink 1-2 cm).' + _NOCOL, 'RailBridge'),
    ('railbridge_truss', _X + 'SM_PrypyatRailBridge_Truss', True,
     'centre of the bottom chord at deck level; 22.6 m along X, 3.45 m tall, 50 cm thick (y -25..25)',
     'Blackened Warren truss side girder (steel). Place at y = +/-525 from the lane centre (outside the 10 m lane), '
     'same X/yaw as the deck. One thin UCX box (blocks vehicles and shells along the lane edge).', 'RailBridge'),
    ('railbridge_truss_broken', _X + 'SM_PrypyatRailBridge_TrussBroken', True,
     'as Truss; +X half (from x 150) has dropped 24 deg, its end ~3.9 m below deck level',
     'Partly collapsed side girder: use on one side (yaw 180 puts the drop at the other end). Dropped half dips into '
     'the bank/water. Two UCX boxes (standing + dropped half).', 'RailBridge'),
    ('railbridge_fallen_span', _X + 'SM_PrypyatRailBridge_FallenSpan', False,
     'span centre, z 0 = water surface; lies on its side, 16 x 5 m, -2.1..+0.7 m',
     'Blackened span lying in the river beside the bridge with sleepers caught in it and purple algae ribbons '
     '(MI_Phytophage_BlackPlum_Solid) trailing downstream along +X. Yaw so +X points downstream. Add '
     'SM_PrypyatBog_AlgaeMat_* / SM_Phytophage_Mat_* around it.' + _NOCOL, 'RailBridge'),
    ('railbridge_abutment', _X + 'SM_PrypyatRailBridge_Abutment', True,
     'block centre; 3.3 x 2.8 m, coping top +0.7 m, footing -3.5 m; fallen coping stones spill to -Y',
     'Masonry abutment for the 4 bridge corners: e.g. (+/-1100, +/-640) from the deck centre, -Y (stone spill) '
     'pointing away from the lane. UCX box (blocks).', 'RailBridge'),
    ('railbridge_pier', _X + 'SM_PrypyatRailBridge_Pier', True,
     'pier centre at the water line; cutwater points -X; 2.2 x 1.4 m, -3.2..+2.3 m',
     'Stone river pier with a torn bearing and girder stub; stand it in the river under/beside the fallen span, -X '
     'upstream. UCX box.', 'RailBridge'),
    ('logbridge_deck', _X + 'SM_PrypyatLogBridge_Deck', False,
     'centre on the lane surface; 22 m along X x 10.4 m along Y; logs half sunk, top 16-22 cm',
     'South log bridge deck: logs across the lane in two staggered rows, a few missing/charred, two plank wheel runs '
     'along X.' + _NOCOL, 'LogBridge'),
    ('logbridge_siderail', _X + 'SM_PrypyatLogBridge_SideRail', True,
     'kerb-log centre line at deck level; 22.4 m along X; piles reach -3.8 m, posts/rail up to +1.1 m',
     'Timber piles + kerb log + broken top rail for each side of the log bridge: place at y = +/-520. Thin UCX box '
     '(blocks along the lane edge).', 'LogBridge'),
    ('logbridge_pilecluster', _X + 'SM_PrypyatLogBridge_PileCluster', False,
     'cluster centre at the water line; 3.7 x 1.8 m; piles -3.2..+2.0 m',
     'Old broken pile cluster with an algae strand, for the water beside the bridge.' + _NOCOL, 'LogBridge'),
    ('corduroy_14m', _X + 'SM_PrypyatCorduroy_14m', False,
     'segment centre on the road surface; 8 m along X (tile every 800 cm), 14.8 m across Y; tops +12 cm',
     'Log corduroy causeway segment: logs across the road, some sunk/tilted. Place in a row along the south causeway '
     '(yaw = road direction, X along the road), 800 cm apart; vary yaw by 180 to break repetition.' + _NOCOL, 'LogBridge'),
    ('corduroy_sunken_10m', _X + 'SM_PrypyatCorduroy_Sunken_10m', False,
     'as Corduroy_14m but 10.8 m across; tops 0-12 cm',
     'Narrow "sunken logs" variant for the 10 m choke (s_logs): more sunk, 20 % missing, more tilt. 800 cm '
     'spacing.' + _NOCOL, 'LogBridge'),
    ('boardwalk_6m', _X + 'SM_PrypyatBoardwalk_6m', False,
     'deck centre; 6 m along X (tile every 600 cm), 4.5 m across Y; deck top +5 cm, piles to -1.3 m',
     'Infantry plank walk on three stringers with pile pairs at x -300 and 0 (the next tile supplies +300). Lay on '
     'the low dyke, X along the walk; 26 tiles for the 155 m walk.' + _NOCOL, 'Boardwalk'),
    ('boardwalk_broken_6m', _X + 'SM_PrypyatBoardwalk_Broken_6m', False,
     'as Boardwalk_6m; the +X half sags 7 deg (end ~0.4 m lower), 30 % planks missing, one stringer broken',
     'Broken/tilted segment: swap in for every 4th-6th tile.' + _NOCOL, 'Boardwalk'),
    ('boardwalk_post', _X + 'SM_PrypyatBoardwalk_Post', False,
     'post foot; 0.2 m pile, -1.3..+0.95 m', 'Lone marker post/pile beside the walk.' + _NOCOL, 'Boardwalk'),
    ('radiomast_leaning', _M + 'SM_PrypyatRadioMast_Leaning', True,
     'mast foot centre on the ground; leans toward +X (5 deg, 13 deg above 55 %); top 27.3 m; guy anchors at 11.5 m '
     'radius at yaw 90/210/330 deg from +X in UE (Y is mirrored from Blender)',
     'Damaged triangular lattice radio mast, one snapped guy coiled on the ground toward +X/-Y. Use for the airfield '
     'ridge AND the ring signals post (one asset, three spots per the lead; vary yaw). UCX only around the footing '
     '(2.2 x 2.2 m, blocks); guys/anchors have no collision. Keep the 23 m wire circle clear of buildings.', 'RadioMasts'),
    ('radiomast_island', _M + 'SM_PrypyatRadioMast_Island', True,
     'mast foot centre; upright 22.5 m (+2.6 m whip), anchors at 11.5 m radius',
     'Intact Soviet lattice mast with three dipole crossarms for the island base (air-only). UCX around the footing.',
     'RadioMasts'),
]
PRY_MANIFEST_ENTRIES = [dict(id=r[0], kit='Crossings' if 'radiomast' not in r[0] else 'RadioMast', asset_path=r[1],
                             kind='static_mesh', blocks_vehicles=r[2], destructible='none', pivot=r[3], notes=r[4],
                             preview=(_PM if r[5] == 'RadioMasts' else _PX) + r[5] + '_Preview.png') for r in _ROWS]
PRY_MANIFEST_ENTRIES += [
    dict(id='existing_rail_track', kit='Existing', asset_path='/Game/RTS_Survival/Meshes/Civilian/Rails/SM_RTS_Railway_Track_01',
         kind='static_mesh', blocks_vehicles=False, destructible='none',
         pivot='segment START (x 0..1000), centred in Y; ballast 7.75 m wide, -0.93..+0.59 m',
         notes='USE THIS for the single-track line on the 14 m north embankment (no new track built). Already in the '
               'project and on Ger_Berlin; tile every 1000 cm along +X. It has ONE convex collision hull over the '
               'ballast: set the placed components to NoCollision (or CanEverAffectNavigation off) if vehicles must '
               'drive over it, and sink it ~40 cm so the ballast reads as part of the embankment. Curves: '
               '/Game/LowPolyForest/Blueprints/Trains/BP_Train_Track_01 has a SplineTrack component (not tested). '
               'Ger_Trains uses SM_RustedRefinery_RailRoad (25.5 m, yellow refinery atlas) - less fitting.'),
    dict(id='existing_kolkhoz_silo', kit='Existing', asset_path='/Game/AtlassedQuarry/SM_Quarry_Silo3', kind='static_mesh',
         blocks_vehicles=True, destructible='none', pivot='foot of the concrete plinth (centre); 9.1 x 9.1 m, 18.1 m tall',
         notes='Use for the kolkhoz grain silo (no new silo built). Shape checked in Blender (Saved/Pripyat/assets/silo_look.png, untextured): a riveted steel grain/feed bin on four legs over a concrete plinth, with a roofed hopper house and a stair - reads as a 1940s-60s rural grain hopper, fits a kolkhoz yard. Its UE material M_SM_Quarry_Silo3 was not viewed. Do NOT use SM_Quarry_Silo4 (its material slot is empty in UE). Destructible alternative: BP_Dest_PostApo_Silo2 (4.7 x 3.7 x 11 m, corrugated, PostApo).'),
]
exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets/manifest_add.py').read())

# Airfield look-fix notes (lead's review 2026-11-14) appended to the existing rows.
import json as _json
_p = 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/pripyat_assets.json'
_d = _json.load(open(_p, encoding='utf-8'))
_FIX_RUNWAY = (' LOOK FIX 2026-11-14: slot 8 (edge-light lens) -> MI_Prypyat_RunwayLightsOff (Emissive_Strength 0, '
               'Emissive_Color black). M_Prypyat_Runway has a second Custom node "ruin pass"; MI_Prypyat_Runway values: '
               'PaintStrength 0.35 (markings ~35 %), Weathering 1.4, RepairStrength 0.9, Wetness 0.5, ConcreteDarken 0.62, '
               'SlabSize 500 (cm), JointWidth 14, JointGrass 0.85 (slab-joint grass/dirt), CrackGrass 0.6 (hairline-crack '
               'mask), MarginGrass 0.7 (margin-dirt mask). Tune on the MI.')
_FIX_RIDGE = (' LOOK FIX 2026-11-14: T_Prypyat_RidgeEarth_base_color re-imported as dry straw grass (~sRGB 150/135/95) '
              'over bare brown earth; T_Prypyat_RidgeConcrete_base_color as dark weathered grey (also darkens the '
              'memorial concrete). No parameter changes (texture swap, both hole masters follow).')
for _a in _d['assets']:
    if _a['id'] == 'airfield_runway' and 'LOOK FIX' not in _a['notes']:
        _a['notes'] += _FIX_RUNWAY
    if _a['id'].startswith(('airfield_ridge', 'airfield_sandbags')) and 'LOOK FIX' not in _a['notes'] and 'ridge' in _a['id']:
        _a['notes'] += _FIX_RIDGE
with open(_p, 'w', encoding='utf-8') as _h:
    _json.dump(_d, _h, indent=1)
print('look-fix notes written')
