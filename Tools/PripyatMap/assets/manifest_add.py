"""Merge entries into Saved/Pripyat/assets/pripyat_assets.json, reading bounds/class/compile state from the editor.

In the server namespace set PRY_MANIFEST_ENTRIES = [dict(id, kit, asset_path, kind, blocks_vehicles, destructible,
pivot, notes, [mesh_component])] and exec this file. For a Blueprint the bounds are the union of its StaticMesh
component templates' mesh bounds (component relative transforms ignored except location).
"""
import json
import os

import unreal

PRY_MANIFEST = 'D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/pripyat_assets.json'


def _pry_bounds_mesh(mesh, offset=(0, 0, 0)):
    b = mesh.get_bounding_box()
    return [[round(b.min.x + offset[0], 1), round(b.min.y + offset[1], 1), round(b.min.z + offset[2], 1)],
            [round(b.max.x + offset[0], 1), round(b.max.y + offset[1], 1), round(b.max.z + offset[2], 1)]]


def _pry_bp_info(bp):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    boxes, meshes = [], []
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        comp = lib.get_object_for_blueprint(data, bp)
        if isinstance(comp, unreal.StaticMeshComponent):
            mesh = comp.get_editor_property('static_mesh')
            if mesh is None:
                continue
            loc = comp.get_editor_property('relative_location')
            boxes.append(_pry_bounds_mesh(mesh, (loc.x, loc.y, loc.z)))
            meshes.append(mesh.get_path_name().split('.')[0])
    if not boxes:
        return None, meshes
    lo = [min(b[0][i] for b in boxes) for i in range(3)]
    hi = [max(b[1][i] for b in boxes) for i in range(3)]
    return [lo, hi], meshes


def _pry_manifest_add(entries):
    data = {'note': 'Pripyat mission assets (written by Source/Tools/PripyatMap/assets/manifest_add.py). Local cm, '
                    '+X forward, +Z up unless a note says otherwise.', 'assets': []}
    if os.path.isfile(PRY_MANIFEST):
        with open(PRY_MANIFEST, 'r', encoding='utf-8') as handle:
            data = json.load(handle)
    by_id = {a['id']: a for a in data['assets']}
    for entry in entries:
        asset = unreal.load_asset(entry['asset_path'])
        row = dict(entry)
        row['verified_loaded'] = asset is not None
        if asset is None:
            row['bounds_cm'] = None
        elif isinstance(asset, unreal.Blueprint):
            unreal.BlueprintEditorLibrary.compile_blueprint(asset)
            row['class'] = row.get('class') or 'Blueprint'
            row['parent'] = str(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(asset).get_name()) \
                if hasattr(unreal.BlueprintEditorLibrary, 'get_blueprint_parent_class') else row.get('parent')
            row['bounds_cm'], row['meshes'] = _pry_bp_info(asset)
            row['generated_class_ok'] = asset.generated_class() is not None
        elif isinstance(asset, unreal.StaticMesh):
            row['class'] = 'StaticMesh'
            row['bounds_cm'] = _pry_bounds_mesh(asset)
            body = asset.get_editor_property('body_setup')
            geom = body.get_editor_property('agg_geom') if body else None
            row['collision_hulls'] = (len(geom.get_editor_property('convex_elems')) + len(geom.get_editor_property('box_elems'))) if geom else 0
            row['lods'] = asset.get_num_lods()
            row['triangles_lod0'] = asset.get_num_triangles(0)
            row['materials'] = [m.material_interface.get_path_name().split('.')[0] if m.material_interface else None
                                for m in asset.get_editor_property('static_materials')]
        by_id[row['id']] = row
    data['assets'] = sorted(by_id.values(), key=lambda a: (a.get('kit', ''), a['id']))
    tmp = PRY_MANIFEST + '.tmp'
    with open(tmp, 'w', encoding='utf-8') as handle:
        json.dump(data, handle, indent=1)
    os.replace(tmp, PRY_MANIFEST)
    print('manifest: %d assets (%d updated)' % (len(data['assets']), len(entries)))
    for entry in entries:
        r = by_id[entry['id']]
        print(' ', r['id'], r.get('bounds_cm'), 'loaded' if r['verified_loaded'] else 'MISSING')


_pry_manifest_add(globals().get('PRY_MANIFEST_ENTRIES', []))
