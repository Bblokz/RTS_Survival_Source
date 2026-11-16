"""Tank breaches on the Berlin ruin WALL pieces (B1-B6; not the E-series building shells, B7 wall cores or B8 debris
beds): field-texture holes and UWallTankBreachComponent, heavy tanks only, unowned walls (no RTS component).
Run in a DebugGame commandlet (needs RTS.WallBreach.ConfigureRusWall):
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script=<this file>
ruin_blueprints_build.run() calls configure_ruin_walls() at its end, so a kit rebuild keeps the breaches.

- Material: M_BerlinRuin_WallHolesField (field master of M_BerlinRuin) and MI_BerlinRuin_Wall_HolesField, a copy of
  MI_BerlinRuin_Wall_Holes (same overrides, inner tint, two-sided) with no rebar (brick and plaster).
- Wall line: every B piece's wall runs along mesh Y at x = 0, 60 cm thick (the kit's thin NavBlock boxes); the rubble
  heap lies on -X. B5_CornerL is an L: legs at x = 310 (along Y) and y = 310 (along X).
- MeshNoNav keeps the wall mesh out of the navmesh (it already was on 2026-11-16, so no map needed a nav rebuild): the
  navmesh is baked with DynamicModifiersOnly, so only the kit's NavBlock boxes, switched to the heavy-only area at play
  time, may decide who crosses.
- B3 (four-storey facade) and B4 (firewall) do not stand over a tank-sized gap: they die whole (the kit's death).
"""
import unreal

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
KIT = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/'
BP_DIR = KIT + 'Blueprints/'
MESH_DIR = KIT + 'Meshes/'
SOURCE_MASTER = KIT + 'Materials/M_BerlinRuin'
HOLES_MI = KIT + 'Materials/MI_BerlinRuin_Wall_Holes'
FIELD_MI = KIT + 'Materials/MI_BerlinRuin_Wall_HolesField'
DUST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/FX/NS_Deploy_HeavyDust'
SOUND = '/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue'
ATTENUATION = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
CONCURRENCY = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'
WALL_THICKNESS = 60
NAV_HALF_DEPTH = 250
# Brick tears out wider than the hull: 75 cm a side (Rus walls 25) leaves a gap the heavy navmesh (agent radius 200)
# still fits through once a dead remnant blocks everyone outside its gap.
SIDE_MARGIN = 75
# piece: (path or None for x = 0 along the mesh's Y extent, dies whole when the top falls)
WALL_PIECES = {
    'B1_Facade1St_A': (None, False), 'B1_Facade1St_B': (None, False),
    'B2_Facade2St_A': (None, False), 'B2_Facade2St_B': (None, False),
    'B3_Facade4St': (None, True), 'B4_Firewall': (None, True),
    'B5_CornerL': ('310,-350;310,310;-350,310', False),
    'B6_Stub_A': (None, False), 'B6_Stub_B': (None, False),
}


def _load_field_library():
    namespace = {'__name__': 'wall_field_library'}
    exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_field_holes_material.py').read(), namespace)
    return namespace


def ensure_field_material():
    """The ruin field instance: a copy of MI_BerlinRuin_Wall_Holes on the field master of M_BerlinRuin."""
    library = _load_field_library()
    master = library['field_master_for'](SOURCE_MASTER)
    instance = unreal.load_asset(FIELD_MI) if EAL.does_asset_exist(FIELD_MI) else EAL.duplicate_asset(HOLES_MI, FIELD_MI)
    MEL.set_material_instance_parent(instance, master)
    MEL.set_material_instance_scalar_parameter_value(instance, library['REBAR'], 0.0)
    # Broken brick inside the hole: dark red-brown, not the grey concrete spall of the Rus walls.
    MEL.set_material_instance_scalar_parameter_value(instance, library['INNER_DARKEN'], 0.6)
    MEL.set_material_instance_vector_parameter_value(instance, library['INNER_COLOR'],
                                                     unreal.LinearColor(0.13, 0.075, 0.055, 1.0))
    overrides = instance.get_editor_property('base_property_overrides')
    overrides.set_editor_property('override_two_sided', True)
    overrides.set_editor_property('two_sided', True)
    instance.set_editor_property('base_property_overrides', overrides)
    MEL.update_material_instance(instance)
    EAL.save_loaded_asset(instance, False)
    unreal.log('BERLIN_BREACH material ' + instance.get_path_name() + ' on ' + master.get_name())
    return instance


def _straight_path(piece):
    mesh = unreal.load_asset(MESH_DIR + 'SM_BerlinRuin_' + piece)
    box = mesh.get_bounding_box()
    return f'0,{box.min.y:.0f};0,{box.max.y:.0f}'


def configure_ruin_walls(only=None):
    ensure_field_material()
    for piece, (path, kill_on_fall) in WALL_PIECES.items():
        if only and piece not in only:
            continue
        command = (f'RTS.WallBreach.ConfigureRusWall Blueprint={BP_DIR}BP_BerlinRuin_{piece} Materials={FIELD_MI} '
                   f'Path={path or _straight_path(piece)} Thickness={WALL_THICKNESS} HeavyOnly=1 Unowned=1 '
                   f'KillOnFall={1 if kill_on_fall else 0} NavHalfDepth={NAV_HALF_DEPTH} SideMargin={SIDE_MARGIN} MeshNoNav=StaticMesh Dust={DUST} Sound={SOUND} '
                   f'Attenuation={ATTENUATION} Concurrency={CONCURRENCY}')
        unreal.log('BERLIN_BREACH ' + command)
        unreal.SystemLibrary.execute_console_command(None, command)


if __name__ != 'berlin_breach_library':
    configure_ruin_walls()
