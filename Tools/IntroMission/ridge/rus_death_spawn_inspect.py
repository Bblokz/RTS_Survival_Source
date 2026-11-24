"""-ExecutePythonScript: exports the Rus building Blueprints that spawn leftovers on death to T3D text (read-only) so
the death-spawn setup can be found. Writes Saved/RidgeRework/t3d/<name>.t3d. 2026-11-18."""
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/RidgeRework/t3d/'
D = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/'
ASSETS = [D + 'Rus_Building_Barracks', D + 'Rus_Building_CoolingTowers', D + 'Rus_Building_FuelStorageLarge',
          D + 'FuelStorage/BP_Dest_RusFuelLarge']
os.makedirs(OUT, exist_ok=True)
for path in ASSETS:
    bp = unreal.load_asset(path)
    name = path.split('/')[-1]
    gen = bp.generated_class()
    cdo = unreal.get_default_object(gen)
    parent = gen.get_name()
    unreal.log_warning(f'RUSINSPECT {name} parent {parent}')
    for target, fname in ((bp, name + '_bp.t3d'), (cdo, name + '_cdo.t3d')):
        task = unreal.AssetExportTask()
        task.set_editor_property('object', target)
        task.set_editor_property('filename', OUT + fname)
        task.set_editor_property('automated', True)
        task.set_editor_property('prompt', False)
        task.set_editor_property('replace_identical', True)
        task.set_editor_property('exporter', unreal.ObjectExporterT3D())
        ok = unreal.Exporter.run_asset_export_task(task)
        unreal.log_warning(f'RUSINSPECT export {fname} {ok}')
unreal.log_warning('RUSINSPECT DONE')
