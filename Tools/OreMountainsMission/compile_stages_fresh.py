"""Fresh-load compile check of the Ore stage Blueprints (headless commandlet; compile errors show as
'Error: [Compiler]' lines in the log). Run:
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nullrhi
"""
import unreal

BASE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/'
NAMES = ['Mission_Ore_Base', 'Mission_Ore_Opening', 'Mission_Ore_KeepHQ', 'Mission_Ore_Convoy1_FuelRun',
         'Mission_Ore_Convoy2_Weighbridge', 'Mission_Ore_LightCounterattack', 'Mission_Ore_Convoy3_AmmoSplit',
         'Mission_Ore_Convoy4_Hunters', 'Mission_Ore_Convoy5_Cases', 'Mission_Ore_HeavyCounterattack',
         'Mission_Ore_Convoy6_Heavy', 'Mission_Ore_RadioOutpost', 'Mission_Ore_RecoverCases', 'Mission_Ore_DestroyBaseB',
         'Mission_Ore_BaseA', 'Mission_Ore_Battery', 'Mission_Ore_Ending', 'BPML_OreMission']
unreal.log_warning('ORE_STAGE_CHECK_BEGIN')
for name in NAMES:
    bp = unreal.load_asset(BASE + name)
    if bp is None:
        unreal.log_error('ORE_STAGE_MISSING ' + name)
        continue
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cls = bp.generated_class()
    unreal.log_warning('ORE_STAGE_COMPILED {} class={}'.format(name, cls.get_name() if cls else None))
unreal.log_warning('ORE_STAGE_CHECK_END')
