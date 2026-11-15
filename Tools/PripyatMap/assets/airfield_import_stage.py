"""Staged driver for PrypyatAirfield/Integration/import_airfield_damage.py through the Pripyat throwaway editor.

The kit's run_import() does ~21 meshes, 17 Blueprints and the hole masters in one call: far over the 2-minute budget
of the shared file-driven server. This driver keeps one AirfieldImporter in the server namespace and runs it in stages:

    python pripyat_cmd.py -c "PRY_AF_STAGE='init'; exec(open(r'<this file>').read())"
    ... 'materials' | 'meshes:0:6' | 'actors:0:3' | 'finish'

Fixes applied here (subclass, the kit file is left as authored; its safety rules are kept):
- legacy FBX import forced around each mesh import (Interchange drops UCX hulls, BerlinFurniture.md);
- the owner tag is written right after an import/create, so a mesh that fails a later check is still ours on a retry.
"""
import json
import runpy

import unreal

KIT_FILE = r'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/PrypyatAirfield/Integration/import_airfield_damage.py'
_ns = globals()
_stage = _ns.get('PRY_AF_STAGE', 'init')


def _pry_af_make():
    kit = runpy.run_path(KIT_FILE, run_name='prypyat_airfield_kit')
    base = kit['AirfieldImporter']
    g = base.__init__.__globals__

    class StagedImporter(base):
        def import_mesh(self, specification):
            name = specification['name']
            folder = g['TARGET'] + ('/Debris' if specification['role'] == 'debris' else '/Meshes')
            target = folder + '/' + name
            self.ensure_owned_target(target)
            options = unreal.FbxImportUI()
            for key, value in {'import_mesh': True, 'import_as_skeletal': False, 'import_materials': False,
                               'import_textures': False, 'import_animations': False}.items():
                options.set_editor_property(key, value)
            options.set_editor_property('mesh_type_to_import', unreal.FBXImportType.FBXIT_STATIC_MESH)
            import_data = options.get_editor_property('static_mesh_import_data')
            for key, value in {'combine_meshes': True, 'auto_generate_collision': False,
                               'one_convex_hull_per_ucx': True, 'generate_lightmap_u_vs': False,
                               'import_uniform_scale': float(specification.get('import_uniform_scale', 1.0))}.items():
                import_data.set_editor_property(key, value)
            import_data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
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
            # The FBX importer drops a slot that no face uses (WheelChocks has no paint faces). Accept that only when
            # the remaining slots keep the manifest's order; every later check then counts the real slots.
            actual = [str(s.get_editor_property('material_slot_name')) for s in mesh.get_editor_property('static_materials')]
            wanted = specification['materials']
            if actual != wanted and all(a in wanted for a in actual) and \
                    [w for w in wanted if w in actual] == actual:
                print('note: %s has no faces on %s; slots %s' % (name, [w for w in wanted if w not in actual], actual))
                specification['materials'] = actual
            self.finish_mesh(mesh, specification)
            self.imported_meshes[name] = mesh

        def components(self, blueprint):
            # The kit took any is_root_component() hit as the root; on the furniture-template copy that can be the
            # WallHoleDamage actor component (no mobility). Only a SceneComponent may be the root (ruin builder rule).
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

    manifest = kit['load_manifest']()
    importer = StagedImporter(manifest)
    importer.mapping = {}
    return kit, g, importer


def _pry_af_slice(text, items):
    parts = text.split(':')
    lo = int(parts[1]) if len(parts) > 1 else 0
    hi = int(parts[2]) if len(parts) > 2 else len(items)
    return items[lo:hi]


if _stage == 'init' or '_pry_af_state' not in _ns:
    _kit, _g, _imp = _pry_af_make()
    _imp.preflight_engine()
    _ns['_pry_af_state'] = (_kit, _g, _imp)
    print('init ok: %d materials, %d assets' % (len(_imp.manifest['materials']), len(_imp.manifest['assets'])))
_kit, _g, _imp = _ns['_pry_af_state']

if _stage == 'materials':
    for _name, _cfg in _imp.manifest['materials'].items():
        _imp.import_material(_name, _cfg)
        print('material', _name)
elif _stage.startswith('meshes'):
    for _spec in _pry_af_slice(_stage, _imp.manifest['assets']):
        _imp.import_mesh(_spec)
        print('mesh', _spec['name'], _imp.mesh_checks.get(_spec['name']))
elif _stage.startswith('loadmeshes'):
    # Re-attach meshes imported by an earlier editor session (after a server restart).
    for _spec in _imp.manifest['assets']:
        _folder = _g['TARGET'] + ('/Debris/' if _spec['role'] == 'debris' else '/Meshes/')
        _imp.imported_meshes[_spec['name']] = _imp.required_asset(_folder + _spec['name'])
        _slots = [str(s.get_editor_property('material_slot_name'))
                  for s in _imp.imported_meshes[_spec['name']].get_editor_property('static_materials')]
        if _slots != _spec['materials'] and [w for w in _spec['materials'] if w in _slots] == _slots:
            _spec['materials'] = _slots
    for _name in _imp.manifest['materials']:
        _imp.imported_materials[_name] = _imp.required_asset(_g['TARGET'] + '/Materials/' + _name)
    print('reattached', len(_imp.imported_meshes), 'meshes')
elif _stage.startswith('actors'):
    _actors = [s for s in _imp.manifest['assets'] if s['role'] in {'crushable_prop', 'destructible_shell'}]
    for _spec in _pry_af_slice(_stage, _actors):
        _imp.mapping[_spec['name']] = _imp.build_actor(_spec)
        print('actor', _spec['name'], json.dumps(_imp.mapping[_spec['name']]['configuration_readback']))
elif _stage == 'finish':
    for _spec in _imp.manifest['assets']:
        if _spec['role'] in {'crushable_prop', 'destructible_shell'}:
            if _spec['name'] not in _imp.mapping:
                # Built by an earlier request whose importer instance is gone: read the saved Blueprint back.
                _bp = _imp.required_asset(_g['TARGET'] + '/Blueprints/BP_' + _spec['name'][3:])
                _mesh = _imp.imported_meshes[_spec['name']]
                _mname = 'MainMesh' if _spec['role'] == 'crushable_prop' else 'StaticMesh'
                _imp.compile_and_check(_bp)
                _imp.mapping[_spec['name']] = {
                    'mesh_path': _mesh.get_path_name().split('.')[0], 'blueprint_path': _bp.get_path_name().split('.')[0],
                    'role': _spec['role'], 'crush_rule': 'AnyTank' if _spec['role'] == 'crushable_prop' else None,
                    'configured': True, 'runtime_verified': False,
                    'configuration_readback': _imp.validate_actor_configuration(_bp, _mesh, _spec, _mname)}
            continue
        _mesh = _imp.imported_meshes[_spec['name']]
        _imp.mapping[_spec['name']] = {'mesh_path': _mesh.get_path_name().split('.')[0], 'blueprint_path': None,
                                       'role': _spec['role'], 'configured': True, 'runtime_verified': False}
    if _imp.template_snapshot(_imp.required_asset(_g['CRUSH_TEMPLATE'])) != _imp.source_snapshot:
        raise RuntimeError('Shared crush template changed unexpectedly; stop and inspect.')
    _missing = [s['name'] for s in _imp.manifest['assets'] if s['name'] not in _imp.mapping]
    _report = {'status': 'imported_templates_pending_runtime_tests' if not _missing else 'incomplete',
               'template_configuration_passed': not _missing, 'missing': _missing,
               'runtime_verified': False, 'target': _g['TARGET'], 'assets': _imp.mapping,
               'mesh_checks': _imp.mesh_checks, 'blueprint_compile_checks': _imp.blueprint_compile_checks,
               'blueprint_compile_verified': all(c['success_verified'] for c in _imp.blueprint_compile_checks.values()),
               'written_assets': sorted(set(_imp.created_paths)),
               'driver': 'Source/Tools/PripyatMap/assets/airfield_import_stage.py (staged, legacy FBX importer)'}
    _g['REPORT'].write_text(json.dumps(_report, indent=2), encoding='utf-8')
    print('finish:', _report['status'], 'missing', _missing, 'written', len(_report['written_assets']))
