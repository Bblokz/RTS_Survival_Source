"""Turns Nanite off on the gun pit, pit prop, RedLine fieldwork and team weapon wreck static meshes, and swaps the heavy
sandbag meshes for their cut versions, through the editor bridge (UECP direct calls, see uecp.py).

The game does not render Nanite. A Nanite mesh then draws only its coarse fallback, and with Nanite off it would draw
its full source: up to 110k triangles for a sandbag pit. So the heavy meshes are first cut in Blender by
Marketplace/Scritps/PitOptimize/pit_mesh_optimize.py (bags to about a third, ammo props to half, original normals
kept), which writes <source>_Opt.fbx next to each source. This script imports those over the assets, keeping each
asset's material per slot, collision, LOD group and sockets, and turns Nanite off. The light meshes only get Nanite off.

Run: python optimize_pit_meshes.py [heavy,light]   (scratchpad uecp.py importable; UECP_PORT picks an editor)
"""
import json
import sys
from uecp import call_tool

GAME = '/Game/RTS_Survival/Blueprints'
EXPANSIONS = GAME + '/Buildings/BuildingExpansions'
B4_PIT = GAME + '/Squads/TeamWeapons/Soviet/B4_203MM/Pit/Meshes'
REDLINE = GAME + '/Environment/RedLine/Meshes'
BLENDER = 'D:/Blender/BlenderProjects/BlenderTanks'
GER_EXPORTS = BLENDER + '/CustomMapItems/GerDefenses/Exports'
REDLINE_EXPORTS = BLENDER + '/(KADO-3D)/(EXPORTS)/RedLine'
# Asset -> its cut FBX.
HEAVY = {
    EXPANSIONS + '/GerPits/Meshes/SM_GerPit_Flak': GER_EXPORTS + '/SM_GerPit_Flak_Opt.fbx',
    EXPANSIONS + '/GerPits/Meshes/SM_GerPit_Howitzer': GER_EXPORTS + '/SM_GerPit_Howitzer_Opt.fbx',
    EXPANSIONS + '/GerPits/Meshes/SM_GerPit_Pak': GER_EXPORTS + '/SM_GerPit_Pak_Opt.fbx',
    B4_PIT + '/SM_B4Pit_Sandbags': BLENDER + '/(KADO-3D)/(EXPORTS)/B4_203mm/PitProps/SM_B4Pit_Sandbags_Opt.fbx',
    EXPANSIONS + '/RusDShKPit/Meshes/SM_DShKPit': BLENDER + '/CustomMapItems/SovietsStuff/MGPit/Exports/SM_DShKPit_Opt.fbx',
    EXPANSIONS + '/RusBS3Pit/Meshes/SM_BS3Pit': BLENDER + '/(KADO-3D)/BS3_100mmAT/Exports/SM_BS3Pit_Opt.fbx',
    EXPANSIONS + '/Rus53KPit/Meshes/SM_53KPit': BLENDER + '/(KADO-3D)/45mmAT_53K/Exports/SM_53KPit_Opt.fbx',
    EXPANSIONS + '/Rus53KPit/Meshes/SM_53KSectorPit': BLENDER + '/(KADO-3D)/45mmAT_53K/Exports/SM_53KSectorPit_Opt.fbx',
    REDLINE + '/SM_RL_MGNest': REDLINE_EXPORTS + '/SM_RL_MGNest_Opt.fbx',
    REDLINE + '/SM_RL_GunBay': REDLINE_EXPORTS + '/SM_RL_GunBay_Opt.fbx',
    REDLINE + '/SM_RL_Trench_Straight': REDLINE_EXPORTS + '/SM_RL_Trench_Straight_Opt.fbx',
    REDLINE + '/SM_RL_Trench_Bend': REDLINE_EXPORTS + '/SM_RL_Trench_Bend_Opt.fbx',
    REDLINE + '/SM_RL_TrenchEnd': REDLINE_EXPORTS + '/SM_RL_TrenchEnd_Opt.fbx',
    REDLINE + '/SM_RL_Dugout': REDLINE_EXPORTS + '/SM_RL_Dugout_Opt.fbx',
    REDLINE + '/SM_RL_TurretPillbox': REDLINE_EXPORTS + '/SM_RL_TurretPillbox_Opt.fbx',
}
# Small enough to draw in full: Nanite off only.
LIGHT = [
    EXPANSIONS + '/GerPits/Meshes/SM_GerPit_Clip_37',
    EXPANSIONS + '/GerPits/Meshes/SM_GerPit_Shell_75',
    EXPANSIONS + '/GerPits/Meshes/SM_GerPit_Shell_105',
    EXPANSIONS + '/GerPits/Meshes/SM_GerPit_Shell_150',
    B4_PIT + '/SM_B4Pit_CartHandle',
    B4_PIT + '/SM_B4Pit_ShellRack',
    B4_PIT + '/SM_B4Pit_Rammer',
    B4_PIT + '/SM_B4Pit_Turntable',
    B4_PIT + '/SM_B4Pit_Emplacement',
    B4_PIT + '/SM_B4_Trolley',
    B4_PIT + '/SM_B4_Cartridge',
    B4_PIT + '/SM_B4_Crane',
    GAME + '/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Meshes/SM_ML20Pit_Shell',
    GAME + '/Squads/TeamWeapons/Soviet/ML20_152MM/Mesh/SM_ML20_Destroyed',
    GAME + '/Squads/TeamWeapons/Soviet/BS3_100MM/Mesh/SM_Destroyed_BS3',
    GAME + '/Squads/TeamWeapons/Soviet/53K_45MM/Mesh/SM_Destroyed_53K',
    REDLINE + '/SM_RL_CommTrench',
]

DESCRIBE = """
def describe(mesh):
    box = mesh.get_bounding_box()
    sockets = sorted((s.socket_name, round(s.relative_location.x, 1), round(s.relative_location.y, 1),
                      round(s.relative_location.z, 1)) for s in mesh.get_sockets_by_tag(''))
    return {'tris': [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())],
            'nanite': mesh.get_editor_property('nanite_settings').enabled,
            'box': [round(v, 1) for v in (box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z)],
            'sockets': sockets}

def nanite_off(mesh):
    # Through the subsystem so the mesh rebuilds: setting the property alone leaves the Nanite fallback in place.
    settings = mesh.get_editor_property('nanite_settings')
    settings.enabled = False
    unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, apply_changes=True)
"""


def run_python(code, description):
    result = call_tool('call_action', {'action': 'execute_python', 'params': {'code': code, 'description': description}})
    text = json.loads(result['result']['content'][0]['text'], strict=False)
    output = text.get('output_text') or text.get('result') or ''
    print(output.strip()[:3000])
    if not text.get('ok', True):
        sys.exit(1)
    return output


def swap_heavy(asset, fbx):
    run_python(DESCRIBE + f"""
import unreal
path = '{asset}'
mesh = unreal.load_asset(path)
before = describe(mesh)
materials = {{str(s.material_slot_name): s.material_interface for s in mesh.static_materials}}
collision = mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag')
lod_group = mesh.get_editor_property('lod_group')
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
# The cut mesh carries the original's normals; they must not be recomputed.
data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task = unreal.AssetImportTask()
task.filename = '{fbx}'
task.destination_path = path.rsplit('/', 1)[0]
task.destination_name = path.rsplit('/', 1)[1]
task.automated = True
task.replace_existing = True
task.save = False
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(path)
for index, slot in enumerate(mesh.static_materials):
    material = materials.get(str(slot.material_slot_name))
    if material:
        mesh.set_material(index, material)
    else:
        print('UNMATCHED SLOT', slot.material_slot_name)
mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', collision)
if str(lod_group) != 'None':
    mesh.set_editor_property('lod_group', lod_group)
nanite_off(mesh)
saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
after = describe(unreal.load_asset(path))
print('HEAVY', path.rsplit('/', 1)[1], 'saved', saved)
print('  before', before)
print('  after ', after)
""", 'pit mesh swap ' + asset.rsplit('/', 1)[1])


def disable_light(asset):
    run_python(DESCRIBE + f"""
import unreal
path = '{asset}'
mesh = unreal.load_asset(path)
before = describe(mesh)
mesh.modify()
nanite_off(mesh)
saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
print('LIGHT', path.rsplit('/', 1)[1], 'saved', saved, before['tris'], '->', describe(unreal.load_asset(path))['tris'])
""", 'pit mesh nanite off ' + asset.rsplit('/', 1)[1])


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['heavy', 'light']
    if 'heavy' in steps:
        for heavy_asset, heavy_fbx in HEAVY.items():
            swap_heavy(heavy_asset, heavy_fbx)
    if 'light' in steps:
        for light_asset in LIGHT:
            disable_light(light_asset)
