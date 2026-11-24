"""Unreal editor python: create/refresh BP_RusWallGateA and BP_RusWallGateC (parent AWallGate) by data only.

Components added per Blueprint (under the C++ GateRoot):
  StaticMesh            the frame (SM_RusWallGate<X>_Frame), AWallGate.FrameMeshComponentName
  WallHoleDamage_Frame  holes on the frame (wall concrete + gate steel hole instances, concrete chunks debris)
  WallHoleDamage_Leaf   holes on GateLeafMesh (VAT hole instances; the collapse sound only plays from the frame one)
GateLeafMesh (C++) gets SM_RusWallGate<X>_Leaf. Gate timings, sounds and health are class defaults.
Import as a module or exec; call build_all(). Safe to re-run (existing components are reused by name).
"""
import unreal

EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
ROOT = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates'
WALLS = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls'
CHUNK_DEBRIS = WALLS + '/Meshes/Chunks/SM_Wall_14_Chunks_Debris_%d'
STEEL_DEBRIS = ROOT + '/Holes/SM_RusWallGateSteel_Debris_%d'
COLLAPSE = ('(bCollapseIntoChunksOnDeath=True,DestroyOwnerDelaySeconds=4.500000,DeathSpeedScale=1.300000,'
            'CollapseSound="/Script/Engine.SoundCue\'/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue.Collapse_Cue\'",'
            'CollapseAttenuation="/Script/Engine.SoundAttenuation\'/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse.Att_Collapse\'",'
            'CollapseConcurrency="/Script/Engine.SoundConcurrency\'/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse.SC_Collapse\'")')
COLLAPSE_SILENT = '(bCollapseIntoChunksOnDeath=True,DestroyOwnerDelaySeconds=4.500000,DeathSpeedScale=1.300000)'
GRID = ('(GridAxis=NoGrid,CellSize=150.000000,CellBreakRadiusShare=0.750000,BakeReachShare=0.350000,'
        'GrowRadiusShare=0.700000,MinHoleRadius=12.000000,HoleCentreOutsetShare=0.000000,AreaPerDebrisPiece=4500.000000,'
        'MaxPiecesPerHit=%d,CollapsePieceSpacing=110.000000,MaxCollapsePieces=%d)')
SOUND = '/Game/Aircraft_Sound_Vol1/Aircraft_Sound_Effects/Aircraft_Other_Effects/Cue/Sus4_Iron_Doors_%d_Cue'

GATES = {
    'A': dict(health=1400.0, open_s=2.5, close_s=3.0, open_sound=1, close_sound=2,
              leaf_holes=['MI_RusWallGateA_Leaf_GateSteel_WallHoles'], leaf_debris=STEEL_DEBRIS,
              aim=[(-55, -10, 320), (-865, -10, 320), (-300, -10, 220), (-620, -10, 260)]),
    'C': dict(health=2200.0, open_s=4.0, close_s=4.5, open_sound=4, close_sound=5,
              leaf_holes=['MI_RusWallGateC_Leaf_Concrete_WallHoles', 'MI_RusWallGateC_Leaf_GateSteel_WallHoles'],
              leaf_debris=CHUNK_DEBRIS,
              aim=[(-80, -10, 500), (-990, -10, 500), (-535, -10, 1050), (-400, -10, 250), (-680, -10, 240)]),
}


def handles_by_name(bp):
    out = {}
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        d = SDL.get_data(h)
        out[str(SDL.get_variable_name(d))] = (h, SDL.get_object(d))
    return out


def ensure_component(bp, name, cls, parent_name='GateRoot'):
    comps = handles_by_name(bp)
    if name in comps:
        return comps[name][1]
    params = unreal.AddNewSubobjectParams(parent_handle=comps[parent_name][0], new_class=cls, blueprint_context=bp)
    handle, fail = SDS.add_new_subobject(params)
    if not fail.is_empty():
        raise RuntimeError('add %s failed: %s' % (name, fail))
    SDS.rename_subobject(handle, unreal.Text(name))
    return SDL.get_object(SDL.get_data(handle))


def holes(component, mesh_component_name, materials, debris, collapse, max_pieces, max_collapse):
    component.set_editor_property('wall_mesh_component_name', mesh_component_name)
    component.set_editor_property('damaged_materials', [unreal.load_asset(m) for m in materials])
    component.set_editor_property('debris_meshes', [unreal.load_asset(debris % i) for i in range(8)])
    grid = component.get_editor_property('grid_settings')
    grid.import_text(GRID % (max_pieces, max_collapse))
    component.set_editor_property('grid_settings', grid)
    cs = component.get_editor_property('collapse_settings')
    cs.import_text(collapse)
    component.set_editor_property('collapse_settings', cs)


def build(key):
    cfg = GATES[key]
    name = 'BP_RusWallGate%s' % key
    path = ROOT + '/' + name
    if EAL.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.load_class(None, '/Script/RTS_Survival.WallGate'))
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ROOT, unreal.Blueprint, factory)
    frame = ensure_component(bp, 'StaticMesh', unreal.StaticMeshComponent)
    frame.set_static_mesh(unreal.load_asset('%s/Meshes/SM_RusWallGate%s_Frame' % (ROOT, key)))
    comps = handles_by_name(bp)
    leaf = comps['GateLeafMesh'][1]
    leaf.set_static_mesh(unreal.load_asset('%s/Meshes/SM_RusWallGate%s_Leaf' % (ROOT, key)))
    hf = ensure_component(bp, 'WallHoleDamage_Frame', unreal.WallHoleDamageComponent)
    holes(hf, 'StaticMesh', [ROOT + '/Holes/MI_RusWallGate%s_Frame_Concrete_WallHoles' % key,
                             ROOT + '/Holes/MI_RusWallGate%s_Frame_Steel_WallHoles' % key],
          CHUNK_DEBRIS, COLLAPSE, 12, 60)
    hl = ensure_component(bp, 'WallHoleDamage_Leaf', unreal.WallHoleDamageComponent)
    holes(hl, 'GateLeafMesh', [ROOT + '/Holes/' + m for m in cfg['leaf_holes']], cfg['leaf_debris'],
          COLLAPSE_SILENT, 8, 30)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('health', cfg['health'])
    cdo.set_editor_property('open_duration_seconds', cfg['open_s'])
    cdo.set_editor_property('close_duration_seconds', cfg['close_s'])
    cdo.set_editor_property('open_sound', unreal.load_asset(SOUND % cfg['open_sound']))
    cdo.set_editor_property('close_sound', unreal.load_asset(SOUND % cfg['close_sound']))
    cdo.set_editor_property('gate_sound_attenuation',
                            unreal.load_asset('/Game/RTS_Survival/Sounds/Settings/Attachments/Att_Attachments'))
    cdo.set_editor_property('gate_sound_concurrency',
                            unreal.load_asset('/Game/RTS_Survival/Sounds/Settings/Attachments/SC_Attachments'))
    # weapons aim here (actor space): spread over pillars/towers and the closed leaf, not the origin end
    cdo.set_editor_property('aim_offset_points', [unreal.Vector(*p) for p in cfg['aim']])
    aim = len(cdo.get_editor_property('aim_offset_points'))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = EAL.save_loaded_asset(bp, False)
    names = sorted(handles_by_name(bp))
    unreal.log('GATEBP %s saved=%s comps=%s aim=%s' % (name, saved, names, aim))
    return bp


def build_all():
    return [build(k) for k in GATES]
