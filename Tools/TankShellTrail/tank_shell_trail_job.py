"""Headless editor job, run by build_tank_shell_trail.sh.

Creates the empty Niagara system NS_TankShellTrail when missing, then runs the console command
RTS.TankShellTrail.BuildNiagaraSystem, which adds the emitters, writes the HLSL of
Weapons/SmallArmsProjectileManager/TankShellTrail/TankShellHlsl and saves. Run it again after editing the .hlsl files.
"""
import unreal

FOLDER = "/Game/RTS_Survival/ImpactSfxVfx/TankShellTrail"
NAME = "NS_TankShellTrail"
PATH = FOLDER + "/" + NAME

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.log("RTS_TANK_SHELL_TRAIL_JOB exists " + PATH)
else:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    system = tools.create_asset(NAME, FOLDER, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
    unreal.log("RTS_TANK_SHELL_TRAIL_JOB created %s saved=%s"
               % (PATH, unreal.EditorAssetLibrary.save_loaded_asset(system, False)))
unreal.SystemLibrary.execute_console_command(None, "RTS.TankShellTrail.BuildNiagaraSystem")
