"""Read-only check of the mineral-flora trees in a FRESH commandlet process (setters and LOD screen sizes are not
reliable when read back in the run that set them). Writes Saved/OreMountains/flora/trees/ue_verify_report.json.
Checks per piece: LOD count / triangles / screen sizes, Nanite off, collision hulls, slot materials, Blueprint crush type,
nav area (components deduped by path), Tree / MainMesh mesh, Health, the atlas' alpha-coverage settings, and lists every
package the new assets depend on outside the MineralFlora folder (to show nothing points into /DeadForest/ etc.)."""
import json

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/MineralFlora/Trees'
PIECES = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreMineralFlora/Trees/OreFloraTrees_pieces.json'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/flora/trees/ue_verify_report.json'
sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary
problems = []


def components(bp):
    out, seen = {}, set()
    for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
        o = SDL.get_object_for_blueprint(SDL.get_data(h), bp)
        if o is None or not isinstance(o, unreal.ActorComponent) or o.get_path_name() in seen:
            continue
        seen.add(o.get_path_name())
        out[o.get_name().replace('_GEN_VARIABLE', '')] = o
    return out


def deps(package):
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                                 include_searchable_names=False, include_soft_management_references=False,
                                                 include_hard_management_references=False)
    return [str(d) for d in (reg.get_dependencies(package, opts) or [])]


def main():
    pieces = json.load(open(PIECES))['pieces']
    rep = dict(meshes={}, blueprints={}, problems=problems)
    tex = unreal.load_asset(HOME + '/Textures/T_OreFloraTrees_BC')
    rep['atlas'] = dict(alpha_coverage=str(tex.get_editor_property('alpha_coverage_thresholds')),
                        scale_mips=bool(tex.get_editor_property('do_scale_mips_for_alpha_coverage')), srgb=bool(tex.get_editor_property('srgb')),
                        size=[tex.blueprint_get_size_x(), tex.blueprint_get_size_y()])
    outside = set()
    for key, info in pieces.items():
        mesh = unreal.load_asset('%s/Meshes/%s' % (HOME, info['mesh']))
        g = mesh.get_editor_property('body_setup').get_editor_property('agg_geom')
        m = dict(tris=[mesh.get_num_triangles(i) for i in range(mesh.get_num_lods())], screen=[round(v, 3) for v in sub.get_lod_screen_sizes(mesh)],
                 nanite=bool(mesh.get_editor_property('nanite_settings').enabled), hulls=len(g.get_editor_property('convex_elems')),
                 materials=[s.material_interface.get_name() if s.material_interface else None for s in mesh.static_materials])
        rep['meshes'][key] = m
        if m['tris'] != info['triangles']:
            problems.append('%s triangles %s, build wrote %s' % (key, m['tris'], info['triangles']))
        if m['nanite'] or len(m['screen']) != 3 or abs(m['screen'][1] - 0.45) > 0.01:
            problems.append('%s nanite/screen sizes %s %s' % (key, m['nanite'], m['screen']))
        path = '%s/BP_OreFlora_%s' % (HOME, key)
        bp = unreal.load_asset(path)
        if bp is None:
            problems.append('missing ' + path)
            continue
        cdo = unreal.get_default_object(bp.generated_class())
        b = dict()
        for n in ('TankDestructionType', 'CrushDestructionType', 'Health', 'Crushable', 'Overlap Tanks', 'CrushSinkDeltaZ', 'CrushSinkTime', 'CrushTipSpeedScale'):
            try:
                v = cdo.get_editor_property(n)
                b[n] = str(v).split('.')[-1].split(':')[0].strip('<> ') if 'CrushDestructionType' in str(v) else str(v)
            except Exception:
                pass
        crush = b.get('TankDestructionType') or b.get('CrushDestructionType')
        if crush != info['crush']:
            problems.append('%s crush %s, wanted %s' % (key, crush, info['crush']))
        if abs(float(b.get('Health', 0)) - 300.0) > 0.1:
            problems.append('%s Health %s' % (key, b.get('Health')))
        comps = components(bp)
        for n, c in comps.items():
            if isinstance(c, unreal.StaticMeshComponent) and n in ('Tree', 'MainMesh'):
                sm = c.get_editor_property('static_mesh')
                b['mesh_' + n] = sm.get_name() if sm else None
                if sm is None or sm.get_name() != info['mesh']:
                    problems.append('%s %s mesh %s' % (key, n, b['mesh_' + n]))
            if isinstance(c, unreal.NavModifierComponent):
                b['nav_' + n] = c.get_editor_property('area_class').get_name()
            if isinstance(c, unreal.BoxComponent):
                ac = c.get_editor_property('area_class_override')
                b['nav_' + n] = ac.get_name() if ac else None
        navs = [v for k2, v in b.items() if k2.startswith('nav_')]
        if not navs or any(v != info['nav_area'] + '_C' for v in navs):
            problems.append('%s nav areas %s, wanted %s' % (key, navs, info['nav_area']))
        d = deps(path) + deps('%s/Meshes/%s' % (HOME, info['mesh']))
        b['deps_outside'] = sorted(x for x in d if not x.startswith(HOME))
        outside.update(b['deps_outside'])
        rep['blueprints'][key] = b
    rep['dependencies_outside_folder'] = sorted(outside)
    bad = [x for x in outside if '/DeadForest/' in x or '/OrchardTrees/' in x or '/CrushableRocks/' in x]
    if bad:
        problems.append('references into other kits: %s' % bad)
    with open(OUT, 'w') as f:
        json.dump(rep, f, indent=1, default=str)
    unreal.log('FLORA_VERIFY done, %d problems' % len(problems))
    for p in problems:
        unreal.log_warning('FLORA_VERIFY problem: ' + p)


main()
