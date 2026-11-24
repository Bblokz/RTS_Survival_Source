"""Read-only second look at the stock metal resources (headless commandlet, -nullrhi): collection settings, the
DT_ResourceSettings rows, the Barrels component templates. Writes Saved/OreMountains/metal/inspect/."""
import json
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/metal/inspect/'
os.makedirs(OUT, exist_ok=True)
R = '/Game/RTS_Survival/Blueprints/Resources/'
out = {}


def t3d(obj, name):
    try:
        task = unreal.AssetExportTask()
        task.object, task.filename, task.automated, task.prompt, task.replace_identical = obj, OUT + name + '.t3d', True, False, True
        task.exporter = unreal.ObjectExporterT3D()
        return bool(unreal.Exporter.run_asset_export_task(task))
    except Exception as e:
        return 'ERR ' + str(e)[:100]


for n in ('GC_MetalResource_BarrelSet1', 'GC_MetalResource_BarrelSet_2', 'GC_MetalResource_Rails'):
    gc = unreal.load_asset(R + 'Metal/Geometry/' + n)
    out[n] = t3d(gc, n)
    unreal.log('FLORA_GC %s %s' % (n, out[n]))

dt = unreal.load_asset(R + 'DT_ResourceSettings')
out['dt_t3d'] = t3d(dt, 'DT_ResourceSettings')
for fn in ('export_data_table_to_json_string', 'export_data_table_to_csv_string'):
    try:
        r = getattr(unreal.DataTableFunctionLibrary, fn)(dt)
        open(OUT + 'DT_ResourceSettings.' + ('json' if 'json' in fn else 'csv'), 'w').write(str(r[1] if isinstance(r, tuple) else r))
        unreal.log('FLORA_DT %s ok' % fn)
    except Exception as e:
        unreal.log('FLORA_DT %s ERR %s' % (fn, str(e)[:200]))
try:
    names = [str(x) for x in unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)]
    out['dt_rows'] = names
    for col in ('ResourceBaseCapacity', 'AmountPerHarvest'):
        try:
            out['dt_' + col] = [str(v) for v in unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dt, col)]
        except Exception as e:
            out['dt_' + col] = 'ERR ' + str(e)[:200]
    unreal.log('FLORA_DT rows %s' % json.dumps(out)[:1500])
except Exception as e:
    unreal.log('FLORA_DT rows ERR %s' % str(e)[:200])

# component templates of the Barrels Blueprint (read only)
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SBL = unreal.SubobjectDataBlueprintFunctionLibrary
bp = unreal.load_asset(R + 'ChildBps/Metal/Resource_Metal_Barrels')
seen = set()
for h in sds.k2_gather_subobject_data_for_blueprint(bp):
    data = sds.k2_find_subobject_data_from_handle(h)
    o = SBL.get_object_for_blueprint(data, bp)
    if o is None or not isinstance(o, unreal.ActorComponent) or o.get_path_name() in seen:
        continue
    seen.add(o.get_path_name())
    unreal.log('FLORA_COMP %s %s %s' % (o.get_name(), o.get_class().get_name(), t3d(o, 'Barrels_tmpl__' + o.get_name())))

# texture average of the stock base colour texture, for the tint maths
try:
    tex = unreal.load_asset(R + 'Metal/Meshes/Materials/Textures/T_Metal_Aluminum_D')
    unreal.log('FLORA_TEX %s srgb=%s size=%sx%s comp=%s' % (tex.get_name(), tex.get_editor_property('srgb'), tex.blueprint_get_size_x(),
                                                         tex.blueprint_get_size_y(), tex.get_editor_property('compression_settings')))
except Exception as e:
    unreal.log('FLORA_TEX ERR %s' % str(e)[:200])
json.dump(out, open(OUT + 'inspect2.json', 'w'), indent=1)
unreal.log('FLORA_INSPECT2_DONE')
