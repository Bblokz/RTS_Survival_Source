"""Agent B (2026-11-12): WallHoleDamage on a two-mesh B-4 / ML-20 pit, holes on the `Sandbags` component only.

Run in the shared editor with globals BP_PATH set, e.g.
  BP_PATH = '/Game/.../Rus_BXP_B4HowitzerPit'; exec(open(<this file>).read())
- Sandbags template: QueryOnly, WorldStatic, ignore every channel, never affects navigation. The weapon trace channel
  responses are copied from BuildingMeshComponent in BeginPlay (owner-dependent, set by BP_InitBuildingExp).
- WallHoleDamage component with FRESH structs (never copied from another template: live references).
Does not save; the caller compiles + saves.
"""
import unreal

SAND_MI = '/Game/SandBagPack/Materials/Holes/MI_RusPit_SandBag_01_WallHoles'
DEBRIS = ['/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Meshes/Holes/SM_Rus_SandBag_Debris_%d' % i
          for i in range(8)]
HOLE_TEXT = ('(MinCalibreMm=20,HoleRadiusPerCalibreMm=0.850000,HoleRadiusPerSqrtDamage=5.000000,'
             'GuaranteedChunkRadius=45.000000,MaxHoleRadius=220.000000,MaxHiddenShareWhileAlive=0.650000)')
GRID_TEXT = ('(GridAxis=NoGrid,CellSize=150.000000,CellBreakRadiusShare=0.750000,BakeReachShare=0.350000,'
             'GrowRadiusShare=0.700000,MinHoleRadius=12.000000,HoleCentreOutsetShare=0.000000,'
             'AreaPerDebrisPiece=4500.000000,MaxPiecesPerHit=12,CollapsePieceSpacing=110.000000,'
             'MaxCollapsePieces=120)')
COLLAPSE_TEXT = ('(bCollapseIntoChunksOnDeath=False,DestroyOwnerDelaySeconds=4.500000,DeathSpeedScale=1.300000,'
                 'CollapseSound="/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue.Collapse_Cue",'
                 'CollapseAttenuation="/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse.Att_Collapse",'
                 'CollapseConcurrency="/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse.SC_Collapse")')

sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
bp = unreal.load_asset(BP_PATH)
handles = sds.k2_gather_subobject_data_for_blueprint(bp)


def find(name):
    for handle in handles:
        data = sds.k2_find_subobject_data_from_handle(handle)
        obj = lib.get_object(data)
        if obj is not None and obj.get_name().replace('_GEN_VARIABLE', '') == name:
            return handle, data, obj
    return None, None, None


# 1. Sandbags collision template.
_, sand_data, sand = find('Sandbags')
sand = lib.get_object_for_blueprint(sand_data, bp)
sand.set_collision_enabled(unreal.CollisionEnabled.QUERY_ONLY)
sand.set_collision_object_type(unreal.CollisionChannel.ECC_WORLD_STATIC)
sand.set_collision_response_to_all_channels(unreal.CollisionResponseType.ECR_IGNORE)
sand.set_editor_property('can_ever_affect_navigation', False)
sand.set_editor_property('generate_overlap_events', False)
print('Sandbags profile=%s enabled=%s obj=%s nav=%s tracePlayer(ch7)=%s traceEnemy(ch5)=%s vis=%s' % (
    sand.get_collision_profile_name(), sand.get_collision_enabled(), sand.get_collision_object_type(),
    sand.get_editor_property('can_ever_affect_navigation'),
    sand.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PLAYER),
    sand.get_collision_response_to_channel(unreal.CollisionChannel.ECC_ENEMY),
    sand.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY)))

# 2. WallHoleDamage component.
hole_handle, hole_data, hole = find('WallHoleDamage')
if hole is None:
    root_handle = handles[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root_handle, new_class=unreal.WallHoleDamageComponent,
                                          blueprint_context=bp)
    hole_handle, fail = sds.add_new_subobject(params)
    print('added component', fail)
    sds.rename_subobject(hole_handle, unreal.Text('WallHoleDamage'))
    hole_data = sds.k2_find_subobject_data_from_handle(hole_handle)
hole = lib.get_object_for_blueprint(hole_data, bp)
hole.set_editor_property('WallMeshComponentName', 'Sandbags')
hole.set_editor_property('DamagedMaterials', [unreal.load_asset(SAND_MI)])
hole.set_editor_property('DebrisMeshes', [unreal.load_asset(p) for p in DEBRIS])
for prop, text in (('HoleSettings', HOLE_TEXT), ('GridSettings', GRID_TEXT), ('CollapseSettings', COLLAPSE_TEXT)):
    fresh = type(hole.get_editor_property(prop))()
    fresh.import_text(text)
    hole.set_editor_property(prop, fresh)
print('hole name=%s mesh=%s mats=%s debris=%d' % (hole.get_name(), hole.get_editor_property('WallMeshComponentName'),
      [m.get_path_name() if m else None for m in hole.get_editor_property('DamagedMaterials')],
      len(hole.get_editor_property('DebrisMeshes'))))
for prop in ('HoleSettings', 'GridSettings', 'CollapseSettings'):
    print(' ', prop, hole.get_editor_property(prop).export_text())
