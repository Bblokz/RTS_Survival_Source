"""Headless editor job for the aircraft strafe impact VFX (run by build_strafe_impact_vfx.sh).

  strafe_impact_vfx_job.py build   creates NS_AircraftStrafeImpacts when missing, then runs
                                   RTS.StrafeImpactVfx.BuildNiagaraSystem (adds missing layer emitters, writes the HLSL, saves)
  strafe_impact_vfx_job.py patch   RTS.StrafeImpactVfx.PatchNiagaraSystem only (after editing StrafeImpactHlsl/*.hlsl)
  strafe_impact_vfx_job.py rebuild deletes the system and builds it again (after removing a layer from the table)
The materials are the muzzle VFX ones (M_SmallArmsMuzzleSmoke / M_SmallArmsMuzzleGlow); build those first.
"""
import sys

import unreal

FOLDER = "/Game/RTS_Survival/ImpactSfxVfx/StrafeImpacts"
SYSTEM_NAME = "NS_AircraftStrafeImpacts"


def log(text):
    unreal.log("RTS_STRAFE_IMPACT_JOB " + text)


POCK_MATERIAL = "M_StrafeBulletPock"


def remove_pock_material():
    """The Niagara bullet pocks are gone (decals read better and last); pocks are decals now
    (UAircraftStrafeImpactBatcher::AddBulletPock), so the material this job once made is deleted."""
    path = FOLDER + "/" + POCK_MATERIAL
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log("deleted %s ok=%s" % (path, unreal.EditorAssetLibrary.delete_asset(path)))


def ensure_system():
    system_path = FOLDER + "/" + SYSTEM_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(system_path):
        log("exists " + system_path)
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    system = tools.create_asset(SYSTEM_NAME, FOLDER, unreal.NiagaraSystem, unreal.NiagaraSystemFactoryNew())
    log("created %s saved=%s" % (system_path, unreal.EditorAssetLibrary.save_loaded_asset(system, False)))


def delete_system():
    system_path = FOLDER + "/" + SYSTEM_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(system_path):
        log("deleted %s ok=%s" % (system_path, unreal.EditorAssetLibrary.delete_asset(system_path)))
        # A deleted asset's name lingers until garbage collection; the new system takes the same name.
        unreal.SystemLibrary.collect_garbage()


args = sys.argv[1:]
if not args or args[0] not in ("build", "patch", "rebuild"):
    raise SystemExit("usage: strafe_impact_vfx_job.py build | patch | rebuild")
if args[0] == "rebuild":
    # A fresh system from the layer table: the way to drop an emitter (see ReportEmittersNotInLayers).
    delete_system()
if args[0] in ("build", "rebuild"):
    ensure_system()
    unreal.SystemLibrary.execute_console_command(None, "RTS.StrafeImpactVfx.BuildNiagaraSystem")
    # After the build: the system no longer references the material once Ground_Pock is removed.
    remove_pock_material()
else:
    unreal.SystemLibrary.execute_console_command(None, "RTS.StrafeImpactVfx.PatchNiagaraSystem")
