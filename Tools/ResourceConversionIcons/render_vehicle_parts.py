"""Renders the vehicle parts scavenge meshes as cut-outs for compose_conversion_icons.py (commandlet only, never the
shared editor; run_render.ps1 starts it). Uses the global ability icon renderer (ue_render_unit.py via
render_ability_parts.py) on the empty template map; the renders land in Saved/GlobalAbilityIcons/renders as RCV_<job>_*.

The R3B_VehicleParts render is seen almost edge-on and reads as a dark sliver at 180 px, so these look down at the
pile from higher up, like the radixite in the Harvest / ReturnCargo icons.
"""
import unreal

STUDIO_MAP = "/Engine/Maps/Templates/Template_Default"
SCAV = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/Meshes/"
LOG = "[ConvIcons] "

unreal.EditorLoadingAndSavingUtils.load_map(STUDIO_MAP)
unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 0")
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/render_ability_parts.py").read())

TOP = dict(elevation=38.0, padding=0.95, res=1024, light_intensity=9.0, ambient_intensity=3.0)
RCV_JOBS = {
    "VehicleParts1": dict(parts=[dict(asset=SCAV + "SM_ScavObj_VehicleParts1", yaw=20.0)], azimuth=40.0, **TOP),
    "VehicleParts2": dict(parts=[dict(asset=SCAV + "SM_ScavObj_VehicleParts2", yaw=20.0)], azimuth=40.0, **TOP),
    "VehicleParts3": dict(parts=[dict(asset=SCAV + "SM_SavObj_VehicleParts3", yaw=20.0)], azimuth=40.0, **TOP),
    "VehicleParts4": dict(parts=[dict(asset=SCAV + "SM_ScavObj_VehicleParts4", yaw=20.0)], azimuth=40.0, **TOP),
}

# A commandlet never ticks: the first capture can come out before shaders and textures are in, so render twice.
for attempt in range(2):
    for name, job in RCV_JOBS.items():
        job = dict(job)
        parts = job.pop("parts")
        unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncAssetCompilationFinishAll")
        try:
            render_scene("RCV_" + name, parts, sockets=POINTS, **dict(LOOK, **job))
            unreal.log(LOG + "RENDERED RCV_%s pass %d" % (name, attempt + 1))
        except Exception as error:
            unreal.log_error(LOG + "render %s failed: %s" % (name, error))
unreal.SystemLibrary.execute_console_command(None, "r.TextureStreaming 1")
