# Builds /Game/RTS_Survival/Blueprints/Environment/SovietAirSupply/BP_SovietAirSupplyPad (parent ASovietAirSupplyPad):
# intro landing pad, radio mast, fuel + munition storage, two unarmed Mosin workers, barrel/crate cargo, Pe-8 transport.
# Re-runnable: an existing BP is updated in place (layout values below are the source of truth).
# Headless: UnrealEditor-Win64-DebugGame-Cmd <uproject> -run=pythonscript -script=<this file>
import unreal

FOLDER = '/Game/RTS_Survival/Blueprints/Environment/SovietAirSupply/'
BP_NAME = 'BP_SovietAirSupplyPad'
LOG = unreal.Paths.project_saved_dir() + 'SovietAirSupply/build_bp.txt'
ANIMS = FOLDER + 'Anims/'

ASSETS = {
    'pad': '/Game/RTS_Survival/Blueprints/Environment/IntroMission/LandingField/SM_LandingPad_Soviet',
    'mast': '/Game/RTS_Survival/Meshes/Buildings/Radar/SM_Antenna2',
    'fuel_storage': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/FuelStorage/SM_Rus_FuelStorage1',
    'munition_storage': '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/AmmoStorageMeshes/SM_Rus_AmmoStorage',
    'worker': '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier',
    'worker_mat': '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/Atlas/M_RusSoldier_Soviet',
    'barrel': '/Game/MilitaryAirport/Meshes/Props/SM_Barrel_1_Gasoline',
    'crate': '/Game/MillitaryBase/Meshes/SM_ammoBox1',
    'aircraft': '/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/PE8Bomber/Rus_PE8_Aircraft_Transport.Rus_PE8_Aircraft_Transport_C',
    'walk': ANIMS + 'AN_AirSupply_Walk',
    'carry': ANIMS + 'AN_AirSupply_CarryWood',
    'pick': ANIMS + 'AN_AirSupply_PickUp',
    'put': ANIMS + 'AN_AirSupply_PutDown',
    'idle': '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/TeamWeaponAnims/Idle/Anim_TW_StandingIdle',
}

# Local layout in cm; the pad is 21 m square around the origin, the plane flies in along +X.
LAYOUT = {
    'pad_mesh': ((0, 0, 0), 0),
    'landing_point': ((0, 0, 0), 0),
    'radio_mast_mesh': ((-1450, -1550, 0), 30),
    'fuel_storage_mesh': ((-700, 1950, 0), 0),
    'munition_storage_mesh': ((700, 1950, 0), 0),
    'fuel_storage_point': ((-700, 1560, 0), 90),
    'munition_storage_point': ((700, 1560, 0), 90),
    'worker_post_a': ((-180, 1380, 0), -90),
    'worker_post_b': ((180, 1380, 0), -90),
}

lines = []


def log(s):
    lines.append(str(s))
    unreal.log('AIRSUPPLY_BP ' + str(s))


def load(key):
    path = ASSETS[key]
    a = unreal.load_class(None, path) if key == 'aircraft' else unreal.load_asset(path)
    if a is None:
        log('MISSING %s %s' % (key, path))
    return a


eal = unreal.EditorAssetLibrary
bp_path = FOLDER + BP_NAME
if eal.does_asset_exist(bp_path):
    bp = unreal.load_asset(bp_path)
    log('updating existing %s' % bp_path)
else:
    factory = unreal.BlueprintFactory()
    factory.set_editor_property('parent_class', unreal.SovietAirSupplyPad)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BP_NAME, FOLDER[:-1], unreal.Blueprint, factory)
    log('created %s' % bp_path)

cdo = unreal.get_default_object(bp.generated_class())


def comp(name):
    c = cdo.get_editor_property(name)
    if c is None:
        log('NO COMPONENT %s' % name)
    return c


def place(name):
    loc, yaw = LAYOUT[name]
    c = comp(name)
    c.set_editor_property('relative_location', unreal.Vector(*loc))
    c.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=float(yaw)))


for name in LAYOUT:
    place(name)

comp('pad_mesh').set_editor_property('static_mesh', load('pad'))
comp('radio_mast_mesh').set_editor_property('static_mesh', load('mast'))
comp('fuel_storage_mesh').set_editor_property('static_mesh', load('fuel_storage'))
comp('munition_storage_mesh').set_editor_property('static_mesh', load('munition_storage'))
comp('fuel_cargo_pile').set_editor_property('static_mesh', load('barrel'))
comp('munition_cargo_pile').set_editor_property('static_mesh', load('crate'))

worker_mesh = load('worker')
worker_mat = load('worker_mat')
for name in ('worker_mesh_a', 'worker_mesh_b'):
    w = comp(name)
    w.set_editor_property('skeletal_mesh_asset', worker_mesh)
    w.set_editor_property('override_materials', [worker_mat])
    w.set_editor_property('cast_shadow', True)

visuals = cdo.get_editor_property('worker_visuals')
visuals.set_editor_property('walk_anim', load('walk'))
visuals.set_editor_property('carry_anim', load('carry'))
visuals.set_editor_property('idle_anim', load('idle'))
visuals.set_editor_property('pick_up_anim', load('pick'))
visuals.set_editor_property('put_down_anim', load('put'))
visuals.set_editor_property('walk_speed', 150.0)
visuals.set_editor_property('carry_speed', 115.0)
visuals.set_editor_property('handle_cargo_seconds', 1.4)
visuals.set_editor_property('hand_over_fraction', 0.55)
visuals.set_editor_property('mesh_yaw_offset', -90.0)
# Carried items hang off the mesh root (mesh space: +Y forward, +Z up), held in front of the chest.
visuals.set_editor_property('carry_socket_name', 'None')
visuals.set_editor_property('fuel_carry_transform', unreal.Transform(
    location=unreal.Vector(0.0, 38.0, 78.0), rotation=unreal.Rotator(roll=90.0, pitch=0.0, yaw=0.0),
    scale=unreal.Vector(0.62, 0.62, 0.62)))
visuals.set_editor_property('munition_carry_transform', unreal.Transform(
    location=unreal.Vector(0.0, 36.0, 92.0), rotation=unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0),
    scale=unreal.Vector(0.75, 0.75, 0.75)))
visuals.set_editor_property('cargo_pile_offset', unreal.Vector(-450.0, 380.0, 0.0))
visuals.set_editor_property('cargo_pile_spacing', 115.0)
cdo.set_editor_property('worker_visuals', visuals)

settings = cdo.get_editor_property('settings')
settings.set_editor_property('aircraft_class', load('aircraft'))
cdo.set_editor_property('settings', settings)

unreal.BlueprintEditorLibrary.compile_blueprint(bp)
saved = eal.save_asset(bp_path, only_if_is_dirty=False)
log('saved=%s' % saved)

# Read back from the compiled class to prove the values stuck.
cdo2 = unreal.get_default_object(bp.generated_class())
log('readback pad=%s worker=%s aircraft=%s walk=%s' % (
    cdo2.get_editor_property('pad_mesh').static_mesh.get_name() if cdo2.get_editor_property('pad_mesh').static_mesh else None,
    cdo2.get_editor_property('worker_mesh_a').get_editor_property('skeletal_mesh_asset'),
    cdo2.get_editor_property('settings').get_editor_property('aircraft_class'),
    cdo2.get_editor_property('worker_visuals').get_editor_property('walk_anim')))
with open(LOG, 'w') as fh:
    fh.write('\n'.join(lines))
