"""Read-only survey of NS_SandPush (the Ore map SandStreak effect) and its materials/textures.
Headless:  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
Writes Saved/OreMountains/sandstreak/survey/: T3D exports, PNGs of plain textures, survey.json (referencers, MI params).
"""
import json
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sandstreak/survey/'
SYS = '/Game/DustAndSandStorm/SandStorm/Niagara/NS_SandPush'
info = {'refs': {}, 'materials': {}, 'textures': {}}
reg = unreal.AssetRegistryHelpers.get_asset_registry()


def refs_of(pkg):
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    return [str(r) for r in (reg.get_referencers(pkg, opts) or [])]


def deps_of(pkg):
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    return [str(r) for r in (reg.get_dependencies(pkg, opts) or [])]


def export(obj, fname, t3d=True):
    if obj is None:
        return False
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = OUT + fname
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    if t3d:
        task.exporter = unreal.ObjectExporterT3D()
    return unreal.Exporter.run_asset_export_task(task)


sysobj = unreal.load_asset(SYS)
export(sysobj, 'NS_SandPush.t3d')
info['refs'][SYS] = refs_of(SYS)
seen = set()
todo = [d for d in deps_of(SYS) if d.startswith('/Game/')]
info['sys_deps'] = list(todo)
while todo:
    pkg = todo.pop()
    if pkg in seen:
        continue
    seen.add(pkg)
    obj = unreal.load_asset(pkg)
    if obj is None:
        continue
    name = pkg.split('/')[-1]
    cls = obj.get_class().get_name()
    if cls in ('Material', 'MaterialInstanceConstant', 'MaterialFunction'):
        export(obj, name + '.t3d')
        info['refs'][pkg] = refs_of(pkg)
        m = {'class': cls}
        if cls == 'Material':
            m['blend'] = str(obj.get_editor_property('blend_mode'))
            m['shading'] = str(obj.get_editor_property('shading_model'))
        if cls == 'MaterialInstanceConstant':
            m['parent'] = obj.get_editor_property('parent').get_path_name()
            m['scalars'] = {str(p.parameter_info.name): p.parameter_value for p in obj.get_editor_property('scalar_parameter_values')}
            m['vectors'] = {str(p.parameter_info.name): str(p.parameter_value) for p in obj.get_editor_property('vector_parameter_values')}
            m['textures'] = {str(p.parameter_info.name): (p.parameter_value.get_path_name() if p.parameter_value else None)
                             for p in obj.get_editor_property('texture_parameter_values')}
        info['materials'][pkg] = m
        todo += [d for d in deps_of(pkg) if d.startswith('/Game/')]
    elif cls == 'Texture2D':
        t = {'size': [obj.blueprint_get_size_x(), obj.blueprint_get_size_y()],
             'compression': str(obj.get_editor_property('compression_settings')),
             'srgb': obj.get_editor_property('srgb'), 'vt': obj.get_editor_property('virtual_texture_streaming'),
             'lod_group': str(obj.get_editor_property('lod_group'))}
        plain = ('TC_DEFAULT' in t['compression'] or 'TC_MASKS' in t['compression'] or 'TC_GRAYSCALE' in t['compression']
                 or 'TC_EDITOR_ICON' in t['compression']) and not t['vt']
        t['png'] = bool(plain and export(obj, name + '.png', t3d=False))
        info['textures'][pkg] = t
    else:
        info.setdefault('other', {})[pkg] = cls
        if cls.startswith('Niagara') or cls == 'StaticMesh':
            todo += [d for d in deps_of(pkg) if d.startswith('/Game/')]

json.dump(info, open(OUT + 'survey.json', 'w'), indent=1)
unreal.log('SANDSURVEY done')
