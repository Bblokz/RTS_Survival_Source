"""Agent A, shared editor: add WallHoleDamage to the 5 assigned pit BXPs with fresh structs (never copied from
another BP's template), then print the template values. Compile/save are done separately per BP.
Set ONLY = ['key', ...] in globals to limit."""
import unreal

BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/'
TW = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/'
RUS_MI = '/Game/SandBagPack/Materials/Holes/MI_RusPit_SandBag_01_WallHoles'
GER_HOLES = BXP + 'GerPits/Meshes/Holes/'
RUS_DEBRIS = ['/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Meshes/Holes/SM_Rus_SandBag_Debris_%d' % i for i in range(8)]
GER_DEBRIS = [GER_HOLES + 'SM_GerPit_Debris_%d' % i for i in range(8)]
# key: (bp, damaged materials in slot order, debris, area per piece, max pieces, collapse on death)
PITS = {
    'TwinMG42': (BXP + 'GerPits/TwinMG42Pit/Ger_BXP_GerPit_TwinMG42',
                 [GER_HOLES + 'MI_GerPit_GerSandBag_WallHoles', GER_HOLES + 'MI_GerPit_GerAmmoBox_WallHoles'],
                 GER_DEBRIS, 3600.0, 5, False),
    'BS3': (BXP + 'RusBS3Pit/Rus_BXP_BS3Pit', [RUS_MI], RUS_DEBRIS, 4500.0, 12, False),
    'Mortar120': (BXP + 'RusMortarPit/Rus_BXP_120MortarPit', [RUS_MI], RUS_DEBRIS, 4500.0, 12, MORTAR_COLLAPSE
                  if 'MORTAR_COLLAPSE' in globals() else False),
    'B4Stepped': (TW + 'B4_203MM/Pit/Rus_BXP_B4HowitzerSteppedSquarePit', [RUS_MI], RUS_DEBRIS, 4500.0, 12, False),
    'ML20Twin': (TW + 'ML20_152MM/Pit/Rus_BXP_ML20TwinShouldersPit', [RUS_MI], RUS_DEBRIS, 4500.0, 12, False),
}
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary


def find_component(bp, cls):
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = sds.k2_find_subobject_data_from_handle(handle)
        obj = LIB.get_object_for_blueprint(data, bp)
        if obj is not None and isinstance(obj, cls):
            return obj
    return None


def ensure_component(bp):
    existing = find_component(bp, unreal.WallHoleDamageComponent)
    if existing is not None:
        return existing, 'existing'
    root = sds.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.WallHoleDamageComponent,
                                          blueprint_context=bp)
    handle, fail = sds.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError('add failed: %s' % fail)
    sds.rename_subobject(handle, unreal.Text('WallHoleDamage'))
    return LIB.get_object_for_blueprint(sds.k2_find_subobject_data_from_handle(handle), bp), 'added'


def configure(comp, materials, debris, area, max_pieces, collapse):
    comp.set_editor_property('WallMeshComponentName', 'BuildingMeshComponent')
    comp.set_editor_property('DamagedMaterials', [unreal.load_asset(p) if p else None for p in materials])
    comp.set_editor_property('DebrisMeshes', [unreal.load_asset(p) for p in debris])
    hole = unreal.WallChunkHoleSettings()
    hole.import_text('(MinCalibreMm=20,HoleRadiusPerCalibreMm=0.850000,HoleRadiusPerSqrtDamage=5.000000,'
                     'GuaranteedChunkRadius=45.000000,MaxHoleRadius=220.000000,MaxHiddenShareWhileAlive=0.650000)')
    comp.set_editor_property('HoleSettings', hole)
    grid = unreal.WallHoleGridSettings()
    grid.import_text('(GridAxis=NoGrid,CellSize=150.000000,CellBreakRadiusShare=0.750000,BakeReachShare=0.350000,'
                     'GrowRadiusShare=0.700000,MinHoleRadius=12.000000,HoleCentreOutsetShare=0.000000,'
                     'AreaPerDebrisPiece=%f,MaxPiecesPerHit=%d,CollapsePieceSpacing=110.000000,MaxCollapsePieces=120)'
                     % (area, max_pieces))
    comp.set_editor_property('GridSettings', grid)
    col = unreal.WallChunkCollapseSettings()
    col.import_text('(bCollapseIntoChunksOnDeath=%s,DestroyOwnerDelaySeconds=4.500000,DeathSpeedScale=1.300000,'
                    'CollapseSound="/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue.Collapse_Cue",'
                    'CollapseAttenuation="/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse.Att_Collapse",'
                    'CollapseConcurrency="/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse.SC_Collapse")'
                    % ('True' if collapse else 'False'))
    comp.set_editor_property('CollapseSettings', col)


def report(key, comp):
    print('  WallMeshComponentName', comp.get_editor_property('WallMeshComponentName'))
    print('  DamagedMaterials', [m.get_name() if m else None for m in comp.get_editor_property('DamagedMaterials')])
    print('  DebrisMeshes', [m.get_name() if m else None for m in comp.get_editor_property('DebrisMeshes')])
    for name in ('HoleSettings', 'GridSettings', 'CollapseSettings'):
        print('  ', name, comp.get_editor_property(name).export_text())


for key, (path, materials, debris, area, max_pieces, collapse) in PITS.items():
    if 'ONLY' in globals() and key not in ONLY:
        continue
    bp = unreal.load_asset(path)
    comp, state = ensure_component(bp)
    if 'REPORT_ONLY' not in globals():
        configure(comp, materials, debris, area, max_pieces, collapse)
    print('==', key, state, comp.get_name())
    report(key, comp)
