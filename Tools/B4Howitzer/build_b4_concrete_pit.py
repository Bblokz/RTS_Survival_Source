"""Builds the concrete-apron variants of the B-4 and ML-20 gun pits through the editor bridge (UECP direct calls).

The same guns, turrets, crew, drill, weapons and expansion types as Rus_BXP_B4HowitzerPit and Rus_BXP_ML20Pit; only
the emplacement changes (b4_concrete_pit_build.py): an airfield concrete apron behind thick Soviet sandbag walls with
an ammo niche, to stand on the airfield ground of the player start regions.

  B4_203MM/Pit/Meshes/SM_B4ConcretePit            the apron and the niche: M_RTSGround_WithSand (the airfield ground's
                                                  material), MI_Rus_BunkerConcrete_05, MI_B4_Trailer (cartridges)
  B4_203MM/Pit/Meshes/SM_B4ConcretePit_Sandbags   the walls, MI_Rus_SandBag_01 (from the cut _Opt FBX)
  B4_203MM/Pit/Rus_BXP_B4HowitzerConcretePit      a copy of Rus_BXP_B4HowitzerPit with the two meshes
  ML20_152MM/Pit/Rus_BXP_ML20ConcretePit          a copy of Rus_BXP_ML20Pit with the two meshes
Both meshes import with Nanite off (the game draws no Nanite), complex collision and the LargeProp LOD group.

Run: python build_b4_concrete_pit.py [mesh,bxp]   (scratchpad uecp.py importable; UECP_PORT picks an editor)
"""
import json
import sys
from uecp import call_tool

SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/(KADO-3D)/(EXPORTS)/B4_203mm/PitProps'
B4_PIT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Pit'
ML20_PIT = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit'
MESHES = {
    # Asset name -> source FBX (the sandbags come cut by pit_mesh_optimize.py at Sandbag=0.25).
    'SM_B4ConcretePit': SOURCE + '/SM_B4ConcretePit.fbx',
    'SM_B4ConcretePit_Sandbags': SOURCE + '/SM_B4ConcretePit_Sandbags_Opt.fbx',
}
MATERIALS = {
    'Ground': '/Game/RTS_Survival/Meshes/PresetRegion_PlayerStart/AirfieldGround/Materials/M_RTSGround_WithSand',
    'Concrete': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Materials/MI_Rus_BunkerConcrete_05',
    'Ammo': '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/B4_203MM/Materials/MI_B4_Trailer',
    'Sandbag': '/Game/SandBagPack/Materials/MI_Rus_SandBag_01',
}
PITS = {
    B4_PIT + '/Rus_BXP_B4HowitzerPit': B4_PIT + '/Rus_BXP_B4HowitzerConcretePit',
    ML20_PIT + '/Rus_BXP_ML20Pit': ML20_PIT + '/Rus_BXP_ML20ConcretePit',
}
MESH_DIR = B4_PIT + '/Meshes'
BUILDING_MESH = MESH_DIR + '/SM_B4ConcretePit'
SANDBAG_MESH = MESH_DIR + '/SM_B4ConcretePit_Sandbags'


def act(action, **params):
    result = call_tool('call_action', {'action': action, 'params': params})
    text = result.get('result', {}).get('content', [{}])[0].get('text', '{}')
    data = json.loads(text, strict=False)
    if not data.get('ok', True) or data.get('success') is False:
        print('FAILED', action, json.dumps(params)[:300], '->', text[:900])
        sys.exit(1)
    return data


def run_python(code, description):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': description}})
    text = json.loads(result['result']['content'][0]['text'], strict=False)
    output = text.get('output_text') or text.get('result') or ''
    print(output.strip()[:1500])
    if not text.get('ok', True):
        sys.exit(1)
    return output


def import_meshes():
    for name, fbx in MESHES.items():
        run_python(f"""
import unreal
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.import_mesh = True
options.import_as_skeletal = False
options.import_materials = False
options.import_textures = False
data = options.static_mesh_import_data
data.set_editor_property('combine_meshes', True)
data.set_editor_property('auto_generate_collision', False)
data.set_editor_property('build_nanite', False)
data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task = unreal.AssetImportTask()
task.filename = '{fbx}'
task.destination_path = '{MESH_DIR}'
task.destination_name = '{name}'
task.automated = True
task.replace_existing = True
task.save = False
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
path = '{MESH_DIR}/{name}'
mesh = unreal.load_asset(path)
materials = {MATERIALS!r}
for index, slot in enumerate(mesh.static_materials):
    target = materials.get(str(slot.material_slot_name))
    if target:
        mesh.set_material(index, unreal.load_asset(target))
    else:
        print('UNMATCHED SLOT', slot.material_slot_name)
mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',
                                                          unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
mesh.set_editor_property('lod_group', 'LargeProp')
settings = mesh.get_editor_property('nanite_settings')
settings.enabled = False
unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, apply_changes=True)
box = mesh.get_bounding_box()
print('MESH', '{name}', [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())], box.min, box.max,
      [str(s.material_slot_name) for s in mesh.static_materials], unreal.EditorAssetLibrary.save_asset(path, False))
""", 'concrete pit mesh ' + name)


def find_one(nodes, predicate, label):
    matches = [n for n in nodes if predicate(n)]
    if len(matches) != 1:
        print('FAILED node lookup', label, [n['id'] for n in matches])
        sys.exit(1)
    return matches[0]


def build_bxps():
    for source, target in PITS.items():
        run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if not eal.does_asset_exist('{target}'):
    print('DUP', eal.duplicate_asset('{source}', '{target}'))
    eal.save_asset('{target}', only_if_is_dirty=False)
""", 'concrete pit duplicate ' + target.rsplit('/', 1)[1])
        nodes = act('get_graph_snapshot', blueprint_path=target, graph_name='EventGraph', view='compact')['nodes']
        init = find_one(nodes, lambda n: n.get('op') == 'call' and n.get('detail', {}).get('function') == 'BP_InitBuildingExp',
                        'BP_InitBuildingExp')
        mesh_ref = f'{BUILDING_MESH}.{BUILDING_MESH.rsplit("/", 1)[1]}'
        for pin in ('NewBuildingMesh', 'NewConstructionMesh'):
            act('set_pin_default', blueprint_path=target, graph_name='EventGraph', node_id=init['id'], pin_name=pin,
                value=mesh_ref)
        act('set_static_mesh_component', blueprint_path=target, component_name='BuildingMeshComponent',
            mesh_path=BUILDING_MESH)
        act('set_static_mesh_component', blueprint_path=target, component_name='Sandbags', mesh_path=SANDBAG_MESH)
        compiled = act('compile_blueprint', blueprint_path=target)
        print(target.rsplit('/', 1)[1], 'compile', compiled.get('error_count'), compiled.get('errors'))
        act('save_asset', asset_path=target)
        run_python(f"""
import unreal
bp = unreal.load_asset('{target}')
cdo = unreal.get_default_object(bp.generated_class())
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
meshes = []
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
    if isinstance(obj, unreal.StaticMeshComponent):
        meshes.append(obj.get_name() + '=' + (obj.static_mesh.get_name() if obj.static_mesh else 'None'))
    if isinstance(obj, unreal.ChildActorComponent):
        meshes.append(obj.get_name() + '=' + obj.get_editor_property('child_actor_class').get_name())
print('BXP', bp.get_name(), cdo.get_editor_property('PitExpansionType'), meshes)
""", 'concrete pit check ' + target.rsplit('/', 1)[1])


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['mesh', 'bxp']
    for step in steps:
        {'mesh': import_meshes, 'bxp': build_bxps}[step]()
