"""Read-only survey part 2 for the SandStreak (NS_SandPush) masked rework. Headless commandlet, never the shared editor.
Exports the HDR smoke flipbooks (auto exporter), SM_Sand/SM_Sand2 as OBJ, M_Ribon textures, and the overrides +
shadow flags of the placed ORE_Fx_SandStreak_* actors on Ger_2ndOreMountains.
"""
import json
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sandstreak/survey/'
info = {'exports': {}, 'meshes': {}, 'actors': []}


def export(obj, fname, exporter=None):
    if obj is None:
        return 'none'
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = OUT + fname
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    if exporter is not None:
        task.exporter = exporter
    ok = unreal.Exporter.run_asset_export_task(task)
    return str(ok)


for name in ['T_C_Cam09', 'T_C_Cam05']:
    tex = unreal.load_asset('/Game/DustAndSandStorm/SandStorm/Textures/' + name)
    for ext in ['.exr', '.hdr', '.tga']:
        info['exports'][name + ext] = export(tex, name + ext)
for name in ['SM_Sand', 'SM_Sand2']:
    mesh = unreal.load_asset('/Game/DustAndSandStorm/SandStorm/Meshes/' + name)
    b = mesh.get_bounds()
    info['meshes'][name] = {'origin': str(b.origin), 'extent': str(b.box_extent),
                            'verts': unreal.StaticMeshEditorSubsystem().get_number_verts(mesh, 0) if hasattr(unreal, 'StaticMeshEditorSubsystem') else -1}
    info['exports'][name + '.obj'] = export(mesh, name + '.obj', unreal.StaticMeshExporterOBJ())

ribon = unreal.load_asset('/Game/DustAndSandStorm/SandStorm/Materials/M_Ribon')
info['ribon_textures'] = [t.get_path_name() for t in unreal.MaterialEditingLibrary.get_used_textures(ribon)]

world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains')
for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.NiagaraActor):
    label = actor.get_actor_label()
    comp = actor.niagara_component
    asset = comp.get_asset()
    row = {'label': label, 'asset': asset.get_path_name() if asset else None,
           'loc': str(actor.get_actor_location()), 'rot': str(actor.get_actor_rotation()),
           'scale': str(actor.get_actor_scale3d()), 'cast_shadow': comp.get_editor_property('cast_shadow')}
    if asset and 'SandPush' in asset.get_name():
        vals = {}
        for p in ['SandScale', 'Length', 'AddWhiteColor', 'Emessive', 'ParticleSmokeSizeScale', 'ParticleSpawnRate',
                  'Sand_SpawnRate', 'SmokeSpawnRate', 'SandSmokeSpawnRate', 'Lifetime_Scale', 'Contrast', 'FadeDistance']:
            try:
                vals[p] = comp.get_variable_float(p) if hasattr(comp, 'get_variable_float') else None
            except Exception as exc:
                vals[p] = 'err ' + str(exc)
        row['floats'] = vals
    info['actors'].append(row)

json.dump(info, open(OUT + 'survey2.json', 'w'), indent=1)
unreal.log('SANDSURVEY2 done')
