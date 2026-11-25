"""Builds BP_OreRadio_Funkbude, a Blueprint child of Rus_Bxp_ContainerHut (the radio outpost target), headless.
Rerunnable; needs the meshes from ue_radio_import.py.

Only component defaults change; the parent's graph stays (BP_InitBuildingExp, SetMaxHealth 250, BP_OnUnitDies:
vertical collapse + DestroyActor, scorch decal), so health, owner, death and destruction are the container hut's.
  BuildingMeshComponent   SM_OreRadio_Funkbude, override materials cleared (the parent overrides M_container_blue)
  ProgressBarWidget       raised over the ridge
  SelectedDecal           scaled to the hut footprint (same margins as on the container)
  bpSelectionArea         box inside the hut walls (the parent's is a 1 m box inside the container)
"""
import os

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/RadioOutpost'
BP_NAME = 'BP_OreRadio_Funkbude'
PARENT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_ContainerHut'
MESH = HOME + '/Meshes/SM_OreRadio_Funkbude'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
# container mesh 320 (X) x 344 (Y) cm, decal scale (30, 500, 410): Y margin 156, X margin 90 (pitch 90: decal Z = world X)
HUT_X, HUT_Y = 530.0, 730.0          # roof footprint incl. overhang (X north-south), roof + wood pile / drum (Y)
DECAL_SCALE = (30.0, HUT_Y + 156.0, HUT_X + 90.0)
PROGRESS_Z = 490.0                   # ridge cap top 437 cm (stove pipe 568)
SEL_LOC = (0.0, 0.0, 150.0)
SEL_SCALE = (6.5, 9.0, 4.0)          # box extent 32 -> half 208 x 288 x 128 cm, inside the walls (225 x 300)
failures = []


def log(*a):
    unreal.log_warning('ORE_RADIO_BP ' + ' '.join(str(x) for x in a))


def fail(msg):
    failures.append(msg)
    log('FAIL', msg)


def save(path):
    if not EAL.save_asset(path, only_if_is_dirty=False):
        fail('save failed ' + path)
    disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()) + \
        path.replace('/Game/', '', 1) + '.uasset'
    if not os.path.isfile(disk):
        fail('not on disk ' + disk)
    else:
        log('saved', disk, os.path.getsize(disk), 'bytes')


def components(bp):
    out = {}
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        d = SDS.k2_find_subobject_data_from_handle(h)
        o = SDL.get_object_for_blueprint(d, bp)
        if o is not None:
            out[o.get_name().replace('_GEN_VARIABLE', '')] = o
    return out


def build():
    path = HOME + '/' + BP_NAME
    if EAL.does_asset_exist(path):
        EAL.delete_asset(path)
    parent_bp = unreal.load_asset(PARENT)
    mesh = unreal.load_asset(MESH)
    if parent_bp is None or mesh is None:
        fail('parent or mesh missing')
        return None
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', parent_bp.generated_class())
    bp = TOOLS.create_asset(BP_NAME, HOME, unreal.Blueprint, factory)
    comps = components(bp)
    log('components', sorted(comps))
    bm = comps.get('BuildingMeshComponent')
    if bm is None:
        fail('no BuildingMeshComponent')
        return None
    bm.set_editor_property('static_mesh', mesh)
    bm.set_editor_property('override_materials', [])
    pb = comps.get('ProgressBarWidget')
    if pb:
        loc = pb.get_editor_property('relative_location')
        pb.set_editor_property('relative_location', unreal.Vector(loc.x, loc.y, PROGRESS_Z))
    else:
        fail('no ProgressBarWidget')
    dc = comps.get('SelectedDecal')
    if dc:
        log('decal size', dc.get_editor_property('decal_size'), 'scale was', dc.get_editor_property('relative_scale3d'))
        dc.set_editor_property('relative_scale3d', unreal.Vector(*DECAL_SCALE))
    else:
        fail('no SelectedDecal')
    sa = comps.get('bpSelectionArea')
    if sa:
        sa.set_editor_property('relative_location', unreal.Vector(*SEL_LOC))
        sa.set_editor_property('relative_scale3d', unreal.Vector(*SEL_SCALE))
    else:
        fail('no bpSelectionArea')
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    return bp


def verify():
    """Read back from the saved class default object and the child's component templates."""
    bp = unreal.load_asset(HOME + '/' + BP_NAME)
    gc = bp.generated_class()
    cdo = unreal.get_default_object(gc)
    bm = cdo.get_editor_property('building_mesh_component') if hasattr(cdo, 'get_editor_property') else None
    sm = bm.get_editor_property('static_mesh') if bm else None
    ad = unreal.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path(HOME + '/' + BP_NAME + '.' + BP_NAME)
    parent = str(ad.get_tag_value('ParentClass'))
    log('class', gc.get_path_name(), 'parent', parent)
    if 'Rus_Bxp_ContainerHut' not in parent:
        fail('parent is not the container hut')
    log('cdo mesh', sm.get_path_name() if sm else None, 'override mats', bm.get_editor_property('override_materials') if bm else None,
        'collision', bm.get_collision_profile_name() if bm else None)
    if sm is None or sm.get_name() != 'SM_OreRadio_Funkbude':
        fail('CDO BuildingMeshComponent does not carry the hut mesh')
    if bm and list(bm.get_editor_property('override_materials')):
        fail('override materials not cleared')
    comps = components(bp)
    for k in ('ProgressBarWidget', 'SelectedDecal', 'bpSelectionArea'):
        c = comps.get(k)
        if c:
            log('template', k, c.get_editor_property('relative_location'), c.get_editor_property('relative_scale3d'))


if __name__ == '__main__' and not globals().get('VERIFY_ONLY'):
    build()
verify()
log('RESULT', 'OK' if not failures else 'FAIL %s' % failures)
