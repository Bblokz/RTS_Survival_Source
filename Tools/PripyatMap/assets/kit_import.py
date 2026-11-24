"""Generic kit importer for Pripyat Part A kits, built on the PrypyatAirfield importer class (same safety rules:
own TARGET folder only, owner metadata tag, legacy FBX with UCX, only kit assets saved, shared templates checked).

In the server namespace:
    PRY_KIT = 'Village'; PRY_KIT_STAGE = 'init' | 'materials' | 'meshes:a:b' | 'actors:a:b' | 'static'
    exec(open(r'<this file>').read())
Kit definitions are in KITS below. Each mesh spec: name (UE asset name), fbx, role (crushable_prop /
destructible_shell / static), slot_materials (asset path or 'import:<name>' per slot index), health, debris
(asset paths), damage tuning.
"""
import json
import runpy

import unreal

KIT_FILE = r'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PrypyatAirfield/Integration/import_airfield_damage.py'
UCX_REPORT = 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/partA_ucx_report.json'
ROOT = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/'
CMI = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/'
# Child of MI_FarmBuildings with BaseTint 0.64: the village FBX has no vertex colours, so the farm master's
# 0.7 + 0.6 * VertexColor.B term reads 1.3 instead of the farm meshes' ~0.83 (same fix as MI_FarmBuildings_Debris).
FARM_MI = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Village/Materials/MI_Polesie_FarmAtlas'
FARM_DEBRIS = ['/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Farm/Meshes/Debris/SM_FarmHouse_Debris_%d' % i
               for i in range(8)]
WOOD_DEBRIS = ['/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield/Debris/SM_Prypyat_WoodDebris']
CONCRETE = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield/Materials/M_Prypyat_RidgeConcrete'
IRON = '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield/Materials/M_Prypyat_PropMetal'
RUBBLE_DEBRIS = ['/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield/Debris/SM_PrypyatRidge_Rubble']

KITS = {
    'Village': dict(
        materials={'M_Polesie_Overgrowth': {
            'base_color': 'D:/Blender/BlenderProjects/BlenderTanks/GroundPath/ref/T_Ground_Grass_Near_01_BC_s.png',
            'roughness_value': 0.92, 'metallic_value': 0.0, 'base_color_tint': [0.62, 0.66, 0.42]}},
        meshes=[
            dict(name='SM_PolesieLogHouse_A', ucx='SM_LogHouse_ReedThatch_A', role='destructible_shell',
                 slot_materials=[FARM_MI, 'import:M_Polesie_Overgrowth'], health=1400, damage_reduction=8,
                 debris=FARM_DEBRIS, damage={'radius_per_mm': 0.9, 'max_radius_cm': 150}),
            dict(name='SM_PolesieLogHouse_B', ucx='SM_LogHouse_ReedThatch_B', role='destructible_shell',
                 slot_materials=[FARM_MI, 'import:M_Polesie_Overgrowth'], health=1300, damage_reduction=8,
                 debris=FARM_DEBRIS, damage={'radius_per_mm': 0.9, 'max_radius_cm': 150}),
            dict(name='SM_PolesieStorkNestPole_A', ucx='SM_StorkNestPole_A', role='crushable_prop',
                 slot_materials=[FARM_MI, 'import:M_Polesie_Overgrowth'], health=200, debris=WOOD_DEBRIS),
            dict(name='SM_PolesieStorkNestPole_B', ucx='SM_StorkNestPole_B', role='crushable_prop',
                 slot_materials=[FARM_MI, 'import:M_Polesie_Overgrowth'], health=200, debris=WOOD_DEBRIS),
            dict(name='SM_PolesieSweepWell', ucx='SM_WoodenSweepWell', role='crushable_prop',
                 slot_materials=[FARM_MI, 'import:M_Polesie_Overgrowth'], health=220, debris=WOOD_DEBRIS),
        ]),
    'Signs': dict(
        materials={'M_PolesieSigns': {'base_color': CMI + 'PolesieSigns/T_PolesieSigns_Atlas.png',
                                      'roughness_value': 0.78, 'metallic_value': 0.0}},
        meshes=[dict(name='SM_Polesie' + n[3:], ucx=n, role='crushable_prop', slot_materials=['import:M_PolesieSigns'],
                     health=120, debris=WOOD_DEBRIS)
                for n in ('SM_RoadSign_BlankRoad', 'SM_RoadSign_Mozyr', 'SM_RoadSign_Pinsk', 'SM_RoadSign_Stolin',
                          'SM_RoadSign_Turov', 'SM_RailSign_BlankRail', 'SM_RailSign_Kalinkavichy',
                          'SM_RailSign_Luninets')]),
    'Memorial': dict(
        materials={'M_Polesie_MemorialRed': {'base_color': 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/T_Polesie_FadedRed.png',
                                             'roughness_value': 0.85, 'metallic_value': 0.0}},
        meshes=[dict(name='SM_PolesiePartisanMemorial', ucx='SM_PartisanMemorial', role='destructible_shell',
                     slot_materials=[CONCRETE, 'import:M_Polesie_MemorialRed', IRON], health=2500, damage_reduction=12,
                     debris=RUBBLE_DEBRIS, damage={'radius_per_mm': 0.6, 'max_radius_cm': 90},
                     sockets=[{'name': 'AudioLog', 'location_cm': [73.0, 102.0, 70.0], 'yaw_degrees': 90.0}])]),
}

_ns = globals()
_kit_name = _ns.get('PRY_KIT', 'Village')
_kit_stage = _ns.get('PRY_KIT_STAGE', 'init')


def _kit_make(kit_name):
    kit = runpy.run_path(KIT_FILE, run_name='pripyat_kit_' + kit_name)
    base = kit['AirfieldImporter']
    g = base.__init__.__globals__
    g['TARGET'] = ROOT + kit_name
    g['OWNER'] = 'PripyatKit_' + kit_name + '_20261114'
    g['OWNER_TAG'] = 'PripyatKitOwner'
    ucx = json.load(open(UCX_REPORT))
    definition = KITS[kit_name]
    assets = []
    for m in definition['meshes']:
        info = ucx[m['ucx']]
        assets.append(dict(m, fbx=info['fbx'], dimensions_cm=info['size_cm'], triangles=None,
                           collision_hulls=len(info['hulls']), materials=[], import_uniform_scale=1.0))
    manifest = {'materials': definition['materials'], 'assets': assets}

    class KitImporter(base):
        def preflight_engine(self):
            # The airfield preflight also checks the Berlin crush template, sounds and nav area (still valid here).
            base.preflight_engine(self)

        def preflight_destinations(self):
            for name in self.manifest['materials']:
                for suffix in ('', '_WallHoles'):
                    self.ensure_owned_target(g['TARGET'] + '/Materials/' + name + suffix)
            for spec in self.manifest['assets']:
                self.ensure_owned_target(g['TARGET'] + '/Meshes/' + spec['name'])
                if spec['role'] in {'crushable_prop', 'destructible_shell'}:
                    self.ensure_owned_target(g['TARGET'] + '/Blueprints/BP_' + spec['name'][3:])

        def import_mesh(self, specification):
            name = specification['name']
            folder = g['TARGET'] + '/Meshes'
            target = folder + '/' + name
            self.ensure_owned_target(target)
            options = unreal.FbxImportUI()
            for key, value in {'import_mesh': True, 'import_as_skeletal': False, 'import_materials': False,
                               'import_textures': False, 'import_animations': False}.items():
                options.set_editor_property(key, value)
            options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
            data = options.get_editor_property('static_mesh_import_data')
            for key, value in {'combine_meshes': True, 'auto_generate_collision': False, 'one_convex_hull_per_ucx': True,
                               'generate_lightmap_u_vs': False, 'build_nanite': False}.items():
                data.set_editor_property(key, value)
            data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
            task = unreal.AssetImportTask()
            task.filename, task.destination_path, task.destination_name = specification['fbx'], folder, name
            task.automated, task.replace_existing, task.save, task.options = True, True, False, options
            unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 0')
            try:
                self.asset_tools.import_asset_tasks([task])
            finally:
                unreal.SystemLibrary.execute_console_command(None, 'Interchange.FeatureFlags.Import.FBX 1')
            mesh = self.required_asset(target)
            self.assets.set_metadata_tag(mesh, g['OWNER_TAG'], g['OWNER'])
            self.bind_slots(mesh, specification)
            self.finish_mesh(mesh, specification)
            self.imported_meshes[name] = mesh

        def bind_slots(self, mesh, specification):
            """Slot names come from the Blender material names (with .001 suffixes): bind by index instead."""
            slots = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]
            if len(slots) != len(specification['slot_materials']):
                raise RuntimeError('%s: %d slots, expected %d' % (mesh.get_name(), len(slots), len(specification['slot_materials'])))
            specification['materials'] = slots
            for slot, source in zip(slots, specification['slot_materials']):
                if source.startswith('import:'):
                    self.imported_materials[slot] = self.imported_materials[source[7:]]
                else:
                    self.imported_materials[slot] = self.required_asset(source)

        def hole_materials(self, mesh):
            result = []
            for index, slot in enumerate(mesh.get_editor_property('static_materials')):
                source = slot.get_editor_property('material_interface')
                name = 'MI_%s_%d_WallHoles' % (mesh.get_name()[3:], index)
                target = g['TARGET'] + '/Materials/Holes/' + name
                self.ensure_owned_target(target)
                material = self.holes.make_holes_instance(source.get_path_name(), g['TARGET'] + '/Materials/Holes',
                                                          mesh, grid_axis=None, two_sided=True, name=name,
                                                          inner_tint=True, inner_darken=0.6)
                self.validate_hole_material(material)
                self.save_asset(material)
                master = material.get_base_material()
                # A shared master (M_FarmBuildings_WallHoles, the airfield's) is reused as it is, never re-saved.
                if master.get_path_name().startswith(g['TARGET'] + '/'):
                    self.save_asset(master)
                result.append(material)
            return result

        def components(self, blueprint):
            named = {}
            for handle in self.subobjects.k2_gather_subobject_data_for_blueprint(blueprint):
                data = self.subobject_library.get_data(handle)
                template = self.subobject_library.get_object(data)
                if template is None or isinstance(template, unreal.Actor):
                    named.setdefault('__actor__', (handle, data))
                    continue
                named[str(self.subobject_library.get_variable_name(data))] = (handle, data)
                if isinstance(template, unreal.SceneComponent) and self.subobject_library.is_root_component(data):
                    named['__root__'] = (handle, data)
            if '__root__' not in named and 'DefaultSceneRoot' in named:
                named['__root__'] = named['DefaultSceneRoot']
            return named

        def create_blueprint(self, specification):
            blueprint = base.create_blueprint(self, specification)
            if blueprint is not None:
                self.assets.set_metadata_tag(blueprint, g['OWNER_TAG'], g['OWNER'])
            return blueprint

        def setup_holes(self, component, mesh, specification, mesh_component_name):
            # Debris are existing project meshes (asset paths), loaded into the name table the base method reads.
            names = []
            for path in specification['debris']:
                debris = self.required_asset(path)
                self.imported_meshes[debris.get_name()] = debris
                names.append(debris.get_name())
            spec = dict(specification, debris=names)
            base.setup_holes(self, component, mesh, spec, mesh_component_name)

        def validate_actor_configuration(self, blueprint, mesh, specification, mesh_name):
            return base.validate_actor_configuration(self, blueprint, mesh, specification, mesh_name)

    importer = KitImporter(manifest)
    importer.mapping = {}
    return g, importer


def _kit_slice(text, items):
    parts = text.split(':')
    return items[int(parts[1]) if len(parts) > 1 else 0:int(parts[2]) if len(parts) > 2 else len(items)]


_key = '_pry_kit_' + _kit_name
if _kit_stage == 'init' or _key not in _ns:
    _g, _kimp = _kit_make(_kit_name)
    _kimp.preflight_engine()
    _ns[_key] = (_g, _kimp)
    print('init', _kit_name, 'target', _g['TARGET'])
_g, _kimp = _ns[_key]
if _kit_stage in ('materials', 'all'):
    for _name, _cfg in _kimp.manifest['materials'].items():
        _kimp.import_material(_name, _cfg)
        print('material', _name)
if _kit_stage.startswith(('meshes', 'actors')) or _kit_stage == 'all':
    if not all(n in _kimp.imported_materials for n in _kimp.manifest['materials']):
        for _name in _kimp.manifest['materials']:
            _kimp.imported_materials[_name] = _kimp.required_asset(_g['TARGET'] + '/Materials/' + _name)
if _kit_stage.startswith('meshes') or _kit_stage == 'all':
    for _spec in _kit_slice(_kit_stage, _kimp.manifest['assets']):
        _kimp.import_mesh(_spec)
        print('mesh', _spec['name'], _kimp.mesh_checks.get(_spec['name']))
if _kit_stage.startswith('actors') or _kit_stage == 'all':
    _acts = [s for s in _kimp.manifest['assets'] if s['role'] in {'crushable_prop', 'destructible_shell'}]
    for _spec in _kit_slice(_kit_stage, _acts):
        if _spec['name'] not in _kimp.imported_meshes:
            _kimp.imported_meshes[_spec['name']] = _kimp.required_asset(_g['TARGET'] + '/Meshes/' + _spec['name'])
            _kimp.bind_slots(_kimp.imported_meshes[_spec['name']], _spec)
        _res = _kimp.build_actor(_spec)
        print('actor', _spec['name'], json.dumps(_res['configuration_readback']), _res['blueprint_path'])
