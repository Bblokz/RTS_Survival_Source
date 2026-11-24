"""Fresh-load check of the ruin kit Blueprints after a death graph change (headless commandlet, reads only):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash -nullrhi
      -DisablePlugins=Fab,BpGeneratorUltimate -abslog=<log>
Loads BP_BerlinRuin_Base and every BP_BerlinRuin_* child from disk, compiles each and prints its compile status and the
death defaults the builder set. A graph that only compiled in the editor that authored it (for example a class-typed
pin) fails here. Then grep the log:  RUINCOMPILE (one line per Blueprint) and  LogBlueprint: Error.
Nothing is saved.
"""
import unreal

DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints'

def run():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = sorted(str(a.package_name) for a in registry.get_assets_by_path(DIR, recursive=False)
                    if str(a.asset_name).startswith('BP_BerlinRuin_'))
    good = 0
    for path in assets:
        bp = unreal.load_asset(path)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        try:
            status = str(bp.get_editor_property('status'))
        except Exception:  # noqa: BLE001
            status = 'unknown (see LogBlueprint lines)'
        cls = bp.generated_class()
        cdo = unreal.get_default_object(cls)
        name = path.rsplit('/', 1)[1]
        if name == 'BP_BerlinRuin_Base':
            detail = 'base; default SinkOnDeath %s' % cdo.get_editor_property('SinkOnDeath')
        else:
            holes = [c for c in unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem).k2_gather_subobject_data_for_blueprint(bp)]
            detail = ('sink %.0f cm / %.1f s, heaps at %.2f s, small %d large %d, dust %s x%.2f, sound %s, components %d' % (
                cdo.get_editor_property('SinkDeltaZ'), cdo.get_editor_property('SinkTime'),
                cdo.get_editor_property('HeapDustDelay') + cdo.get_editor_property('HeapDustLead'),
                len(cdo.get_editor_property('DeathHeapsSmall')), len(cdo.get_editor_property('DeathHeapsLarge')),
                cdo.get_editor_property('DeathDustVfx').get_name() if cdo.get_editor_property('DeathDustVfx') else None,
                cdo.get_editor_property('DeathDustScale').x,
                cdo.get_editor_property('DeathSound').get_name() if cdo.get_editor_property('DeathSound') else None,
                len(holes)))
        ok = 'ERROR' not in status.upper()
        good += 1 if ok else 0
        unreal.log_warning('RUINCOMPILE %s status %s; %s' % (name, status, detail))
    unreal.log_warning('RUINCOMPILE done: %d Blueprints loaded and compiled (%d with a readable non-error status); the verdict is '
                       'the count of "LogBlueprint: Error" lines in this log, which must be 0' % (len(assets), good))


run()
