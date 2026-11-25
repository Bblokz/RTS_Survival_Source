# Headless probe for the Soviet air supply pad build: mesh bounds, Pe-8 transport sockets, Mosin soldier mesh.
# Run: UnrealEditor-Win64-DebugGame-Cmd <uproject> -run=pythonscript -script=<this file>
import unreal

OUT = unreal.Paths.project_saved_dir() + 'SovietAirSupply/probe.txt'
lines = []


def log(s):
    lines.append(str(s))


def mesh_bounds(path):
    m = unreal.load_asset(path)
    if m is None:
        log('MISSING %s' % path)
        return
    if isinstance(m, unreal.StaticMesh):
        b = m.get_bounding_box()
        log('SM %s min=%s max=%s' % (path.split('/')[-1], b.min, b.max))
    elif isinstance(m, unreal.SkeletalMesh):
        b = m.get_bounds()
        log('SK %s origin=%s extent=%s skel=%s' % (path.split('/')[-1], b.origin, b.box_extent,
                                                 m.skeleton.get_name()))
        try:
            socks = [s.socket_name for s in m.get_editor_property('sockets')] if False else []
        except Exception:
            socks = []
        n = m.num_sockets()
        log('   sockets=%s' % [str(m.get_socket_by_index(i).socket_name) for i in range(n)])
        sk = m.skeleton
        log('   skel sockets=%s' % [str(s.socket_name) for s in sk.get_editor_property('sockets')]
            if hasattr(sk, 'get_editor_property') else '')
    else:
        log('?? %s %s' % (path, type(m)))


for p in [
    '/Game/RTS_Survival/Blueprints/Environment/IntroMission/LandingField/SM_LandingPad_Soviet',
    '/Game/RTS_Survival/Meshes/Buildings/Radar/SM_Antenna1',
    '/Game/RTS_Survival/Meshes/Buildings/Radar/SM_Antenna2',
    '/Game/RTS_Survival/Blueprints/Environment/Prypyat/RadioMast/SM_PrypyatRadioMast_Island',
    '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/RadioOutpost/Meshes/SM_OreRadio_Mast',
    '/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/Meshes/SM_OW_WerkMast',
    '/Game/UVB80/Assets/UVB80/Buildings/Meshes/SM_Antenna',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/AmmoStorageMeshes/SM_Rus_AmmoStorage',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/FuelStorage/SM_Rus_FuelStorage1',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/FuelStorage/SM_Rus_FuelStorage2',
    '/Game/MilitaryAirport/Meshes/Props/SM_Barrel_1_Gasoline',
    '/Game/MillitaryBase/Meshes/SM_Barrel_Gasoline',
    '/Game/MillitaryBase/Meshes/SM_ammoBox1',
    '/Game/MillitaryBase/Meshes/SM_ammoBox2',
    '/Game/MilitaryAirport/Meshes/Props/SM_Crate_1',
    '/Game/MilitaryBaseCamp/Meshes/SM_CrateSmall',
    '/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/PE8Bomber/Mesh/SK_PE8_Bomber',
    '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier',
]:
    try:
        mesh_bounds(p)
    except Exception as e:
        log('ERR %s %s' % (p, e))

# Pe-8 transport BP: class, mesh, components
for bp_path in ['/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/PE8Bomber/Rus_PE8_Aircraft_Transport',
                '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Rus/RusBasic/Rus_Inf_Mosin_Soviet',
                '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Rus/RusBasic/Rus_Inf_Mosin']:
    try:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
        cdo = unreal.get_default_object(cls)
        log('BP %s parent=%s' % (bp_path.split('/')[-1], cls.get_super_class().get_name() if hasattr(cls, 'get_super_class') else '?'))
        for c in cdo.get_components_by_class(unreal.SkeletalMeshComponent):
            m = c.get_editor_property('skeletal_mesh_asset') if hasattr(c, 'skeletal_mesh_asset') else c.skeletal_mesh
            mats = [mi.get_path_name() if mi else None for mi in c.get_materials()]
            log('   SKC %s mesh=%s rel=%s rot=%s mats=%s' % (c.get_name(), m.get_path_name() if m else None,
                                                     c.relative_location, c.relative_rotation, mats))
            if m:
                log('   mesh sockets=%s' % [str(m.get_socket_by_index(i).socket_name) for i in range(m.num_sockets())])
    except Exception as e:
        log('ERR %s %s' % (bp_path, e))

# Mosin soldier walk/idle anims available on the infantry skeleton: what does the squad AnimBP use?
unreal.EditorAssetLibrary  # keep import
with open(OUT, 'w') as f:
    f.write('\n'.join(lines))
print('\n'.join(lines))
