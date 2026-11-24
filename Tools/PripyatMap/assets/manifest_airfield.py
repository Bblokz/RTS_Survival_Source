"""Manifest rows for the PrypyatAirfield kit (Part A1). exec in the server namespace."""
_AF = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield'
_HOW = ('Easiest: ue_place_airfield.place_airfield(origin_cm, yaw_degrees, state) in '
        'CustomMapItems/PrypyatAirfield places runway + props + ridges from airfield_placement.json (checked: all 36 '
        'record assets exist, contract validates, integration_result passed). Runway +X = runway direction.')
PRY_MANIFEST_ENTRIES = [
    dict(id='airfield_runway', kit='Airfield', asset_path=_AF + '/Meshes/SM_SovietAirfield_Prypyat', kind='static_mesh',
         blocks_vehicles=True, destructible='none',
         pivot='runway centre on the pad; 200 x 30 m runway along +X, hangars/bays on -Y, z -35..1100',
         notes='Copy of SM_SovietAirfield (whole field 216 x 140 m) with MI_Prypyat_Runway (weathered markings). '
               'Do NOT scale non-uniformly. Flatten one pad; 35 cm buried skirt. Sketch runway is 173 x 20 m at 45 deg: '
               'the mesh is 200 x 30 m, so the pad must be ~216 x 140 m. ' + _HOW),
]
for _n, _h, _note in [('FuelDrums', 300, 'drum stack'), ('ServiceCrates', 240, 'crate stack'),
                      ('WheelChocks', 120, 'low chocks'), ('RepairBarricade', 260,
                      'runway-closed barricade; placement json puts 4 across the runway at x -7000 for the pre-repair state')]:
    PRY_MANIFEST_ENTRIES.append(dict(
        id='airfield_' + _n, kit='Airfield', asset_path=_AF + '/Blueprints/BP_Prypyat_' + _n, kind='blueprint',
        blocks_vehicles=False, destructible='crushable',
        pivot='ground centre of the footprint, +X front', parent='copy of BP_BerlinFurniture_Base (ADestructableEnvActor)',
        notes='%s, health %d. Any tank crushes it (AnyTank), infantry path round (RTS_NavArea_BlockChar box). Shell holes '
              '(WallHoleDamage). Yaw free, scale 1. Rebuild nav after placing.' % (_note, _h)))
for _n in ('AA', 'Pit'):
    PRY_MANIFEST_ENTRIES.append(dict(
        id='airfield_ridge_%s_core' % _n.lower(), kit='Airfield', asset_path=_AF + '/Meshes/SM_PrypyatRidge_%s_Core' % _n,
        kind='static_mesh', blocks_vehicles=True, destructible='none',
        pivot='platform centre at ground; socket PitMount on top for the weapon pit',
        notes='Stable earth platform for an AA battery / weapon pit (one UCX). Place the matching _Ramp and three '
              '_Shell_* Blueprints at the SAME transform (they share the origin).'))
    PRY_MANIFEST_ENTRIES.append(dict(
        id='airfield_ridge_%s_ramp' % _n.lower(), kit='Airfield', asset_path=_AF + '/Meshes/SM_PrypyatRidge_%s_Ramp' % _n,
        kind='static_mesh', blocks_vehicles=True, destructible='none', pivot='same origin as the core',
        notes='Access ramp of the platform; place at the core transform.'))
    for _side in ('North', 'East', 'West'):
        PRY_MANIFEST_ENTRIES.append(dict(
            id='airfield_ridge_%s_shell_%s' % (_n.lower(), _side.lower()), kit='Airfield',
            asset_path=_AF + '/Blueprints/BP_PrypyatRidge_%s_Shell_%s' % (_n, _side), kind='blueprint',
            blocks_vehicles=True, destructible='wall_holes', pivot='same origin as the core',
            parent='ADestructableEnvActor + WallHoleDamage + NavBlock_NN boxes (NavArea_Null)',
            notes='Concrete revetment sector; holes while alive, collapses into pieces at death (nav boxes leave '
                  'with the actor after 4.5 s). Place at the core transform.'))
for _n, _note in [('Straight', '4 m straight sandbag wall'), ('LReturn', 'L-shaped sandbag return'),
                  ('AA_Arc', 'low sandbag arc lip for an AA site (placed at z +140 on the AA ridges)')]:
    PRY_MANIFEST_ENTRIES.append(dict(
        id='airfield_sandbags_' + _n.lower(), kit='Airfield', asset_path=_AF + '/Blueprints/BP_PrypyatSandbags_' + _n,
        kind='blueprint', blocks_vehicles=True, destructible='wall_holes', pivot='ground centre, +X along the wall',
        parent='ADestructableEnvActor + WallHoleDamage + nav boxes', notes=_note + '; Soviet sandbag material.'))
exec(open(r'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets/manifest_add.py').read())
