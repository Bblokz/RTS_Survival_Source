"""Builds the German rail Pak bunker (breakthrough doctrine) through the editor bridge (UECP direct calls, see uecp.py).

  RailPakBunker/Mesh/SK_Pak40Rail   the Pak 40 with the Tiger rail gun's rails in place of its barrel tube
                                    (pak40rail_build.py), on the Pak 40 skeleton, so the German Pak 40 pit's anim
                                    blueprint, PitOrigin socket and crew drill work as they are
  RailPakBunker/Meshes/SM_GerRailPakBunker
                                    the concrete pillbox with the Pak pit on its roof (railpak_bunker_build.py, cut by
                                    pit_mesh_optimize.py), the German pit materials, complex collision, Nanite off
  RailPakBunker/RTS_BP_EmbTurret_GerRailPak
                                    a copy of the German Pak 40 pit turret with the rail Pak, firing RailGun50MM through
                                    SetupRailgunWeapon (rail projectile, 50 mm rail launch sound)
  RailPakBunker/Ger_BXP_RailPakBunker
                                    a copy of the Pak 40 pit expansion: the bunker mesh, the turret on the roof deck,
                                    type BTX_GerRailPakBunker
  spawner                           the expansion class and its placement preview in RTS_BP_RTSAsyncSpawner
  slug                              the 50 mm rail slug (rail_slug_build.py) with M_RailSlug_Metal and the purple
                                    pulsing M_RailSlug_Glow; the drill's two rounds become slugs, and four spare slugs
                                    ride on PitOrigin (two on the ready crate beside the one the handler picks, two on
                                    the deck between the trails)
  icon                              the build option picture (railpak_bunker_icon.py) as UI texture and slate brush in
                                    GerPits/UI, and the row Train_BTX_GerRailPakBunker of DT_TrainingOptions (picture,
                                    title, tooltip, description); the table is only saved when every other row survives
                                    the JSON round trip unchanged
The training option and the breakthrough HQ layout slot are C++ (UTrainingMenuManager, UTrainingLayoutDataAsset).

Run: python build_railpak_bunker.py [gun,mesh,turret,slug,bxp,register,icon]   (scratchpad uecp.py importable; UECP_PORT picks
an editor other than the 9855 broker's)

The game does not render Nanite: the bunker mesh is imported from the cut SM_GerRailPakBunker_Opt.fbx with Nanite off
(see Docs/Units/GunPitMeshes.md).
"""
import json
import sys
from uecp import call_tool

ROOT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/RailPakBunker'
SOURCE = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/RailPakBunker/Exports'
PAK_SKELETON = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Pak40_75MM/Mesh/Sk_FullAnimPak40TW_Skeleton'
PAK_PHYSICS = '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Pak40_75MM/Mesh/Sk_FullAnimPak40TW_PhysicsAsset'
PAK_PIT_TURRET = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Turrets/RTS_BP_EmbTurret_GerPit_Pak40'
PAK_PIT_BXP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Pak40'
SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
GUN = ROOT + '/Mesh/SK_Pak40Rail'
MESH = ROOT + '/Meshes/SM_GerRailPakBunker'
TURRET = ROOT + '/RTS_BP_EmbTurret_GerRailPak'
BXP = ROOT + '/Ger_BXP_RailPakBunker'
# Slots as Unreal exported the Pak 40 (pak40rail_build.py builds on that export), plus the rails'.
GUN_MATERIALS = {
    'M_Pak40': '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Pak40_75MM/Material/M_Pak40',
    'Pak_40_Wheel_D_Mat': '/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Pak40_75MM/Material/Pak_40_Wheel_D_Mat',
    'RailMaterial': '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/Tiger/Meshes/H1/Textures/'
                    'MI_Tiger1H_PostApo_Rail',
}
BUNKER_MATERIALS = {
    'GerPit_Atlas': '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerBuildingAtlassing/'
                    'M_GerCamoConcretePalet_Atlas',
    'GerPit_Ammo': '/Game/RTS_Survival/Meshes/Props/Ammo/Materials/M_Ger_AmmoBox',
    'GerPit_Sandbag': '/Game/RTS_Survival/Meshes/Props/SandBags/Textures/M_GerSandBagScaffold_Atlas',
    # A tiling bunker concrete (one UV unit is 2 m); the pit atlas's concrete quarter is near-metallic.
    'GerBunker_Concrete': '/Game/RTS_Survival/Meshes/Buildings/Defences/Textures/MI_Bunker_Concrete_Wall_2x2_M_tk3fbcgfw_8K',
    'RailSlug_Metal': ROOT + '/Materials/M_RailSlug_Metal',
    'RailSlug_Glow': ROOT + '/Materials/M_RailSlug_Glow',
}
SLUG = ROOT + '/Meshes/SM_RailSlug'
SLUG_METAL_COLOUR = (0.05, 0.055, 0.065)
SLUG_METAL_ROUGHNESS = 0.35
# The rail gun's purple (its shot and impact effects); the glow pulses between GLOW_STRENGTH -+ GLOW_PULSE.
# No green and little red, and kept under the tone mapper's shoulder: brighter, the bands burn lavender-white
# instead of purple (measured at 4: median capture pixel 227, 159, 244).
SLUG_GLOW_COLOUR = (0.45, 0.0, 1.0)
SLUG_GLOW_STRENGTH = 1.5
SLUG_GLOW_PULSE = 0.5
SLUG_GLOW_SPEED = 3.0
# Spare slugs on PitOrigin (gun space, cm; yaw 180 lies them like the round the handler picks from the crate).
SPARE_SLUGS = {
    'SpareSlug1': ((-265.0, 210.0, 48.2), 180.0),
    'SpareSlug2': ((-265.0, 234.0, 48.2), 180.0),
    'SpareSlug3': ((-250.0, -45.0, 4.6), 0.0),
    'SpareSlug4': ((-250.0, -55.0, 4.6), 0.0),
}
WEAPON_NAME = 'RailGun50MM'
LAUNCH_SOUND = '/Game/RTS_Survival/Sounds/WeaponSounds/RailGuns/50MMRail/Cue_50MMRail_Launch.Cue_50MMRail_Launch'
# The roof deck (railpak_bunker_build.py: roof 250 + the pit deck's 4); the gun stands on it.
DECK_HEIGHT = 254.0
# The block's half width at its foot and the parapet top, for the selection box, decal and nav modifier.
BUNKER_HALF = 560.0
BUNKER_HEIGHT = 330.0
SMOKE_RADIUS = 520.0
SMOKES = 5
DECAL_UNITS = 32.0
EXPANSION_TYPE = 'BTX_GER_RAIL_PAK_BUNKER'
ICON = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/Icons/T_BxpOption_GerRailPakBunker.png'
UI = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/UI'
TEMPLATE_BRUSH = '/Game/RTS_Survival/Blueprints/GameUI/BuildingUI/ExpansionOptions/SB_OptionImage_37mmFLak'
TRAINING_TABLE = '/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions'
TRAINING_ROW = 'Train_BTX_GerRailPakBunker'
DISPLAY_NAME = 'Rail Pak Bunker'
DESCRIPTION = ('A concrete pillbox whose sandbagged roof is the pit of a Pak 40 refitted with rails: a 50 mm rail gun '
               'that outranges the Pak and punches through heavy armour. Its crew hand the slugs up from a ready crate as '
               'the gun turns a full circle. Place it on the map; the HQ builds it there once it is its turn in the queue.')
# SetupRailgunWeapon's parameter; split, its ProjectileWeaponParameters member takes the pit turret's projectile struct.
RAILGUN_PIN = 'RailgunWeaponParameters'


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


def duplicate(source, target):
    # An existing copy is reused: deleting a loaded Blueprint opens a confirmation a windowless editor cannot show.
    run_python(f"""
import unreal
eal = unreal.EditorAssetLibrary
if not eal.does_asset_exist('{target}'):
    print('DUP', eal.duplicate_asset('{source}', '{target}'))
    eal.save_asset('{target}', only_if_is_dirty=False)
""", 'rail pak duplicate ' + target.split('/')[-1])


def import_gun():
    run_python(f"""
import unreal
options = unreal.FbxImportUI()
options.automated_import_should_detect_type = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.import_mesh = True
options.import_as_skeletal = True
options.import_animations = False
options.import_materials = False
options.import_textures = False
options.create_physics_asset = False
options.skeleton = unreal.load_asset('{PAK_SKELETON}')
options.skeletal_mesh_import_data.set_editor_property('import_morph_targets', False)
task = unreal.AssetImportTask()
task.filename = '{SOURCE}/SK_Pak40Rail.fbx'
task.destination_path = '{GUN.rsplit('/', 1)[0]}'
task.destination_name = 'SK_Pak40Rail'
task.automated = True
task.replace_existing = True
task.save = False
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset('{GUN}')
materials = {GUN_MATERIALS!r}
slots = mesh.materials
for slot in slots:
    path = materials.get(str(slot.material_slot_name))
    if path:
        slot.material_interface = unreal.load_asset(path)
    else:
        print('UNMATCHED SLOT', slot.material_slot_name)
mesh.materials = slots
mesh.set_editor_property('physics_asset', unreal.load_asset('{PAK_PHYSICS}'))
print('GUN', mesh.skeleton.get_path_name(), [str(s.material_slot_name) for s in mesh.materials],
      unreal.EditorAssetLibrary.save_asset('{GUN}', only_if_is_dirty=False))
""", 'rail pak gun import')


def import_mesh():
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
task.filename = '{SOURCE}/SM_GerRailPakBunker_Opt.fbx'
task.destination_path = '{MESH.rsplit('/', 1)[0]}'
task.destination_name = 'SM_GerRailPakBunker'
task.automated = True
task.replace_existing = True
task.save = False
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset('{MESH}')
materials = {BUNKER_MATERIALS!r}
for index, slot in enumerate(mesh.static_materials):
    path = materials.get(str(slot.material_slot_name))
    if path:
        mesh.set_material(index, unreal.load_asset(path))
    else:
        print('UNMATCHED SLOT', slot.material_slot_name)
mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',
                                                          unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
mesh.set_editor_property('lod_group', 'LargeProp')
settings = mesh.get_editor_property('nanite_settings')
settings.enabled = False
unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, apply_changes=True)
box = mesh.get_bounding_box()
print('BUNKER', [mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())], box.min, box.max,
      unreal.EditorAssetLibrary.save_asset('{MESH}', only_if_is_dirty=False))
""", 'rail pak bunker mesh import')


# Run in the editor with its constants set in front of it (see build_slug).
SLUG_SCRIPT = r'''
import unreal
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
folder = ROOT_PATH + '/Materials'

def new_material(name):
    path = folder + '/' + name
    if eal.does_asset_exist(path):
        material = unreal.load_asset(path)
        # One pass can leave nodes behind; repeat until the graph is empty.
        for _ in range(8):
            if mel.get_num_material_expressions(material) == 0:
                break
            mel.delete_all_material_expressions(material)
        return material
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

def constant(material, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property('r', value)
    return node

def colour(material, rgb, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property('constant', unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return node

metal = new_material('M_RailSlug_Metal')
mel.connect_material_property(colour(metal, METAL_COLOUR, -400, 0), '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(constant(metal, 1.0, -400, 150), '', unreal.MaterialProperty.MP_METALLIC)
mel.connect_material_property(constant(metal, METAL_ROUGHNESS, -400, 250), '', unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(metal)

glow = new_material('M_RailSlug_Glow')
purple = colour(glow, GLOW_COLOUR, -900, 0)
time = mel.create_material_expression(glow, unreal.MaterialExpressionTime, -1300, 200)
speed = mel.create_material_expression(glow, unreal.MaterialExpressionMultiply, -1100, 200)
mel.connect_material_expressions(time, '', speed, 'A')
speed.set_editor_property('const_b', GLOW_SPEED)
sine = mel.create_material_expression(glow, unreal.MaterialExpressionSine, -900, 200)
mel.connect_material_expressions(speed, '', sine, '')
pulse = mel.create_material_expression(glow, unreal.MaterialExpressionMultiply, -700, 200)
mel.connect_material_expressions(sine, '', pulse, 'A')
pulse.set_editor_property('const_b', GLOW_PULSE)
strength = mel.create_material_expression(glow, unreal.MaterialExpressionAdd, -500, 200)
mel.connect_material_expressions(pulse, '', strength, 'A')
strength.set_editor_property('const_b', GLOW_STRENGTH)
emissive = mel.create_material_expression(glow, unreal.MaterialExpressionMultiply, -300, 100)
mel.connect_material_expressions(purple, '', emissive, 'A')
mel.connect_material_expressions(strength, '', emissive, 'B')
mel.connect_material_property(purple, '', unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(emissive, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(glow)
print('SLUG materials', eal.save_asset(folder + '/M_RailSlug_Metal', False), eal.save_asset(folder + '/M_RailSlug_Glow', False))

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
task = unreal.AssetImportTask()
task.filename = SOURCE_PATH + '/SM_RailSlug.fbx'
task.destination_path = ROOT_PATH + '/Meshes'
task.destination_name = 'SM_RailSlug'
task.automated = True
task.replace_existing = True
task.save = False
task.options = options
tools.import_asset_tasks([task])
mesh = unreal.load_asset(SLUG_PATH)
materials = {'RailSlug_Metal': metal, 'RailSlug_Glow': glow}
for index, slot in enumerate(mesh.static_materials):
    mesh.set_material(index, materials[str(slot.material_slot_name)])
settings = mesh.get_editor_property('nanite_settings')
settings.enabled = False
unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_nanite_settings(mesh, settings, apply_changes=True)
box = mesh.get_bounding_box()
print('SLUG mesh', mesh.get_num_triangles(0), box.min, box.max, eal.save_asset(SLUG_PATH, False))
'''


def build_slug():
    """The slug's two materials, the slug mesh, and the slugs on the turret."""
    constants = (f"ROOT_PATH = {ROOT!r}\nSOURCE_PATH = {SOURCE!r}\nSLUG_PATH = {SLUG!r}\n"
                 f"METAL_COLOUR = {SLUG_METAL_COLOUR!r}\nMETAL_ROUGHNESS = {SLUG_METAL_ROUGHNESS!r}\n"
                 f"GLOW_COLOUR = {SLUG_GLOW_COLOUR!r}\nGLOW_STRENGTH = {SLUG_GLOW_STRENGTH!r}\n"
                 f"GLOW_PULSE = {SLUG_GLOW_PULSE!r}\nGLOW_SPEED = {SLUG_GLOW_SPEED!r}\n")
    run_python(constants + SLUG_SCRIPT, 'rail slug materials and mesh')
    # The drill's rounds become slugs.
    for name in ('Round1', 'Round2'):
        act('set_static_mesh_component', blueprint_path=TURRET, component_name=name, mesh_path=SLUG)
        act('set_component_transform', blueprint_path=TURRET, component_name=name, scale=[1.0, 1.0, 1.0])
    existing = run_python(f"""
import unreal
bp = unreal.load_asset('{TURRET}')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
names = [unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)).get_name()
         for h in sub.k2_gather_subobject_data_for_blueprint(bp)]
print('NAMES', ' '.join(names))
""", 'rail pak turret components')
    for name, (location, yaw) in SPARE_SLUGS.items():
        if name not in existing:
            added = call_tool('call_action', {'action': 'add_component', 'params': {
                'blueprint_path': TURRET, 'component_class': 'StaticMeshComponent', 'component_name': name}})
            text = added['result']['content'][0]['text']
            if '"ok":false' in text and 'already exists' not in text:
                print('FAILED add_component', name, text[:300])
                sys.exit(1)
        act('set_static_mesh_component', blueprint_path=TURRET, component_name=name, mesh_path=SLUG)
        act('set_component_transform', blueprint_path=TURRET, component_name=name, location=list(location),
            rotation=[0.0, yaw, 0.0], scale=[1.0, 1.0, 1.0])
        # The pit crew component attaches tagged components to the gun's socket, so the slugs turn with the gun.
        act('edit_component_property', blueprint_path=TURRET, component_name=name, property_name='ComponentTags',
            value='("PitSocket:PitOrigin")')
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('rail pak turret compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TURRET)


def graph_nodes(bp):
    return act('get_graph_snapshot', blueprint_path=bp, graph_name='EventGraph', view='compact')['nodes']


def find_one(nodes, predicate, label):
    matches = [n for n in nodes if predicate(n)]
    if len(matches) != 1:
        print('FAILED node lookup', label, [n['id'] for n in matches])
        sys.exit(1)
    return matches[0]


def detail(node, key):
    return node.get('detail', {}).get(key)


def swap_to_railgun(bp):
    """Replaces SetupProjectileWeapon with SetupRailgunWeapon, fed the same projectile parameters."""
    nodes = graph_nodes(bp)
    old = [n for n in nodes if n.get('op') == 'call' and detail(n, 'function') == 'SetupProjectileWeapon']
    if not old:
        return
    old = old[0]
    params = find_one(nodes, lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'InitWeaponStateProjectile',
                      'InitWeaponStateProjectile')
    incoming = [n for n in nodes if any(c['node_id'] == old['id'] and c['pin'] == 'execute'
                                        for p in n.get('pins', []) for c in p.get('connected_to', []))]
    outgoing = [c for p in old.get('pins', []) if p['name'] == 'then' for c in p.get('connected_to', [])]
    if not any(n.get('op') == 'call' and detail(n, 'function') == 'SetupRailgunWeapon' for n in nodes):
        act('place_node', blueprint_path=bp, graph_name='EventGraph', handle='fn.CPPTurretsMaster.SetupRailgunWeapon',
            node_id='railgun_setup', x=600, y=400)
    rail_node = find_one(graph_nodes(bp), lambda n: n.get('op') == 'call' and detail(n, 'function') == 'SetupRailgunWeapon',
                         'SetupRailgunWeapon')
    act('split_struct_pin', blueprint_path=bp, graph_name='EventGraph', node_id=rail_node['id'], pin_name=RAILGUN_PIN)
    act('delete_nodes', blueprint_path=bp, graph_name='EventGraph', node_ids=[old['id']], allow_destructive=True)
    connections = [{'from_node': n['id'], 'from_pin': 'then', 'to_node': rail_node['id'], 'to_pin': 'execute'}
                   for n in incoming]
    connections += [{'from_node': rail_node['id'], 'from_pin': 'then', 'to_node': c['node_id'], 'to_pin': c['pin']}
                    for c in outgoing]
    connections.append({'from_node': params['id'], 'from_pin': 'InitWeaponStateProjectile', 'to_node': rail_node['id'],
                        'to_pin': f'{RAILGUN_PIN}_ProjectileWeaponParameters'})
    act('connect_pins', blueprint_path=bp, graph_name='EventGraph', connections=connections)
    print('railgun node', rail_node['id'], 'incoming', [n['id'] for n in incoming], 'outgoing', outgoing)


def build_turret():
    duplicate(PAK_PIT_TURRET, TURRET)
    act('set_skeletal_mesh_component', blueprint_path=TURRET, component_name='Turret', mesh_path=GUN)
    swap_to_railgun(TURRET)
    nodes = graph_nodes(TURRET)
    params = find_one(nodes, lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'InitWeaponStateProjectile',
                      'InitWeaponStateProjectile')
    vfx = find_one(nodes, lambda n: n.get('op') == 'make_struct' and detail(n, 'struct') == 'WeaponVFX', 'WeaponVFX')
    for node, pin, value in ((params, 'WeaponName', WEAPON_NAME), (params, 'ProjectileSystem', 'RailGun'),
                             (vfx, 'LaunchSound', LAUNCH_SOUND)):
        act('set_pin_default', blueprint_path=TURRET, graph_name='EventGraph', node_id=node['id'], pin_name=pin, value=value)
    compiled = act('compile_blueprint', blueprint_path=TURRET)
    print('rail pak turret compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=TURRET)


def build_bxp():
    duplicate(PAK_PIT_BXP, BXP)
    node = find_one(graph_nodes(BXP), lambda n: n.get('op') == 'call' and detail(n, 'function') == 'BP_InitBuildingExp',
                    'BP_InitBuildingExp')
    mesh_ref = f'{MESH}.{MESH.rsplit("/", 1)[1]}'
    for pin, value in (('NewBuildingMesh', mesh_ref), ('NewConstructionMesh', mesh_ref),
                       ('NewBuildingExpansionType', 'BTX_GerRailPakBunker'), ('NewSmokeRadius', f'{SMOKE_RADIUS:.1f}'),
                       ('NewAmountSmokes', str(SMOKES))):
        act('set_pin_default', blueprint_path=BXP, graph_name='EventGraph', node_id=node['id'], pin_name=pin, value=value)
    act('set_static_mesh_component', blueprint_path=BXP, component_name='BuildingMeshComponent', mesh_path=MESH)
    act('edit_component_property', blueprint_path=BXP, component_name='Howitzer', property_name='ChildActorClass',
        value=f'{TURRET}.{TURRET.rsplit("/", 1)[1]}_C')
    act('set_component_transform', blueprint_path=BXP, component_name='Howitzer', location=[0.0, 0.0, DECK_HEIGHT])
    decal_scale = 2.0 * BUNKER_HALF / DECAL_UNITS
    for component, prop, value in (
            ('bpSelectionArea', 'BoxExtent', f'(X={BUNKER_HALF * 0.9:.1f},Y={BUNKER_HALF * 0.9:.1f},Z={BUNKER_HEIGHT * 0.5:.1f})'),
            ('bpSelectionArea', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={BUNKER_HEIGHT * 0.5:.1f})'),
            ('SelectedDecal', 'RelativeScale3D', f'(X=1.0,Y={decal_scale:.2f},Z={decal_scale:.2f})'),
            ('ProgressBarWidget', 'RelativeLocation', f'(X=0.0,Y=0.0,Z={BUNKER_HEIGHT + 260.0:.1f})'),
            ('NavModifier', 'FailsafeExtent', f'(X={BUNKER_HALF:.1f},Y={BUNKER_HALF:.1f},Z={BUNKER_HEIGHT:.1f})')):
        act('edit_component_property', blueprint_path=BXP, component_name=component, property_name=prop, value=value)
    compiled = act('compile_blueprint', blueprint_path=BXP)
    print('rail pak bxp compile', compiled.get('error_count'), compiled.get('errors'))
    act('save_asset', asset_path=BXP)


def register():
    run_python(f"""
import unreal
spawner = unreal.load_asset('{SPAWNER}')
cdo = unreal.get_default_object(spawner.generated_class())
spawner.modify()
expansion_type = unreal.BuildingExpansionType.{EXPANSION_TYPE}
classes = cdo.get_editor_property('building_expansion_map')
classes[expansion_type] = unreal.load_asset('{BXP}').generated_class()
cdo.set_editor_property('building_expansion_map', classes)
previews = cdo.get_editor_property('bxp_preview_mesh_map')
previews[expansion_type] = unreal.load_asset('{MESH}')
cdo.set_editor_property('bxp_preview_mesh_map', previews)
saved = unreal.EditorLoadingAndSavingUtils.save_packages([spawner.get_outermost()], False)
cdo = unreal.get_default_object(spawner.generated_class())
print('REGISTER saved', saved, cdo.get_editor_property('building_expansion_map')[expansion_type],
      cdo.get_editor_property('bxp_preview_mesh_map')[expansion_type].get_name())
""", 'rail pak register')


def localized(key, value):
    escaped = value.replace('\\', '\\\\').replace('"', '\\"')
    return f'NSLOCTEXT("FieldDefenses", "{key}", "{escaped}")'


def icon():
    brush_path = f'{UI}/SB_OptionImage_GerRailPakBunker'
    row = {'Name': TRAINING_ROW,
           'ImageBrush': f"/Script/Engine.SlateBrushAsset'{brush_path}.SB_OptionImage_GerRailPakBunker'",
           'Description': localized(TRAINING_ROW + '_Description', DESCRIPTION),
           'Tooltip': localized(TRAINING_ROW + '_Tooltip', DISPLAY_NAME),
           'Title': localized(TRAINING_ROW + '_Title', f'<Text_Title>{DISPLAY_NAME}</>')}
    run_python(f"""
import json
import unreal
eal = unreal.EditorAssetLibrary
task = unreal.AssetImportTask()
task.filename = '{ICON}'
task.destination_path = '{UI}'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture_path = '{UI}/T_BxpOption_GerRailPakBunker'
texture = unreal.load_asset(texture_path)
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
eal.save_asset(texture_path, False)
if not eal.does_asset_exist('{brush_path}'):
    eal.duplicate_asset('{TEMPLATE_BRUSH}', '{brush_path}')
brush_asset = unreal.load_asset('{brush_path}')
brush = brush_asset.get_editor_property('brush')
brush.set_editor_property('resource_object', texture)
brush_asset.set_editor_property('brush', brush)
eal.save_asset('{brush_path}', False)
row = json.loads({json.dumps(json.dumps(row))})
table = unreal.load_asset('{TRAINING_TABLE}')
original = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
kept = [r for r in original if r['Name'] != row['Name']]
unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(kept + [row]))
round_trip = {{r['Name']: r for r in json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))}}
changed = [r['Name'] for r in kept if round_trip.get(r['Name']) != r]
if changed or row['Name'] not in round_trip:
    raise RuntimeError('DT_TrainingOptions not saved; changed rows: ' + str(changed))
print('ICON row', round_trip[row['Name']]['Title'], 'saved', eal.save_loaded_asset(table, only_if_is_dirty=False))
""", 'rail pak icon and training row')


if __name__ == '__main__':
    steps = sys.argv[1].split(',') if len(sys.argv) > 1 else ['gun', 'mesh', 'turret', 'slug', 'bxp', 'register', 'icon']
    for step in steps:
        {'gun': import_gun, 'mesh': import_mesh, 'turret': build_turret, 'slug': build_slug, 'bxp': build_bxp,
         'register': register, 'icon': icon}[step]()
