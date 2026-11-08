"""Round 3b render jobs (Icons B agent, 2026-11-05): the guns, buildings, crates, aircraft and rounds of the round-3
guns / buildings / strikes / supplies / effects abilities, as cut-outs for round3b_compose_icons.py.

Loaded by round3b_headless.py (DebugGame-Cmd commandlet with -AllowCommandletRendering, never the shared editor).
Adds its jobs to render_ability_parts.JOBS under an "R3B_" prefix so they never collide with other icon jobs; the
renders land next to the others in Saved/GlobalAbilityIcons/renders.
"""
exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityIcons/render_ability_parts.py").read())

R3B_PREFIX = "R3B_"
TW = "/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/"
PITS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/"
BUNKERS = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/"
SCAV = "/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/"
BF109 = A + "GerAircraft/BF109/Meshes/Bf109_WingAnims"
BF109_DIVE = A + "GerAircraft/BF109/Meshes/TurnAnims/Anim_Bf109_Dive"
ME410_DIVE = A + "GerAircraft/ME410/Meshes/ManeuverAnims/Anim_Me410_Dive"
JU87G = A + "GerAircraft/JU_87VTO/Ju87G/SK_Stuka_VTO_Ju87G"
JU87G_POD = A + "GerAircraft/JU_87VTO/Ju87G/SM_Ju87G_BK37Pod"
CANOPY_GER = PARA + "Mesh/SM_Parachute_German"


class AllSockets:
    """Socket filter for _write_points that keeps every socket of every mesh (the composer picks what it needs)."""

    def __contains__(self, name):
        return True


DIVE = dict(azimuth=35.0, elevation=22.0)
# Payloads hang under their canopies seen from a little below the horizon line, like the paratroopers.
HANGING = dict(azimuth=35.0, elevation=12.0, padding=1.0, res=1024)
# Things on the ground seen from the RTS camera's height.
GROUNDED = dict(azimuth=40.0, elevation=24.0, padding=0.95, res=1024)

R3B_JOBS = {
    # aircraft, in the strafing dive the icon shows (same pose and camera as Ju87_Dive)
    # The dive animations; the wing/take-off animations do not pose these meshes as single-node animations
    # (tried 2026-11-05: reference pose, propeller off its hub). The Bf 109's gear stays out in the dive pose, so it is
    # hidden like the Ho 229's.
    "Bf109_Dive": dict(parts=[dict(asset=BF109, yaw=0.0, pitch=-22.0, roll=-12.0, anim=BF109_DIVE, anim_time=0.3,
                                   hide_bones=("B_WheelsMaster",))], padding=0.7, **DIVE),
    "Me410_Dive": dict(parts=[dict(asset=ME410, yaw=0.0, pitch=-22.0, roll=-12.0, anim=ME410_DIVE, anim_time=0.3)],
                       padding=0.7, **DIVE),
    "Ju87G_Dive": dict(parts=[dict(asset=JU87G, yaw=0.0, pitch=-22.0, roll=-12.0, anim=JU87_FLY),
                              dict(asset=JU87G_POD, attach_to="SkeletalMeshComponent0", attach_socket="BK37_Pod_L"),
                              dict(asset=JU87G_POD, attach_to="SkeletalMeshComponent0", attach_socket="BK37_Pod_R")],
                       padding=0.62, **DIVE),
    # team-weapon guns, deployed
    "Flak88_Gun": dict(parts=[dict(asset=TW + "Flak88Wheels/Mesh/SK_FlakwithWheels", yaw=0.0,
                                   anim=TW + "Flak88Wheels/Animations/Deploy/Anim_Flak88_Deployed", anim_time=0.0)],
                       **HANGING),
    "SFH18_Gun": dict(parts=[dict(asset=TW + "LFH18/Mesh/SK_Ger_SFH18Anim", yaw=0.0,
                                  anim=TW + "LFH18/PackAnims/Anim_SFH18_Deployed", anim_time=0.0)], **HANGING),
    "Pak40_Gun": dict(parts=[dict(asset=TW + "Pak40_75MM/Mesh/Sk_FullAnimPak40TW", yaw=0.0,
                                  anim=TW + "Pak40_75MM/Animations/Deploy/Anim_TWPak40_Deployed", anim_time=0.0)],
                      **HANGING),
    # the canopy alone (cargo chutes are the paratrooper canopy scaled up), and the small crate/flare chute
    "Canopy_Ger": dict(parts=[dict(asset=CANOPY_GER, yaw=-90.0)], **HANGING),
    "FlareChute": dict(parts=[dict(asset=FLARE + "SM_Flare_Parachute", yaw=0.0)], **HANGING),
    # buildings, as Blueprints (pits with their guns, the bunker, the field workshop)
    "Pit_Panzerschreck": dict(parts=[dict(asset=PITS + "Ger_BXP_GerPit_Panzerschreck", yaw=0.0)], **GROUNDED),
    "Pit_Pak40": dict(parts=[dict(asset=PITS + "Ger_BXP_GerPit_Pak40", yaw=0.0)], **GROUNDED),
    "Bunker_Flak88": dict(parts=[dict(asset=BUNKERS + "Flak88Bunker/Ger_BXP_Flak88Bunker", yaw=0.0)], **GROUNDED),
    "FieldWorkshop": dict(parts=[dict(asset="/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/FieldWorkshop/"
                                            "BXP_FieldWorkshop", yaw=0.0)], **GROUNDED),
    # the same buildings seen from below the horizon line, for hanging under canopies
    "Pit_Panzerschreck_Hang": dict(parts=[dict(asset=PITS + "Ger_BXP_GerPit_Panzerschreck", yaw=0.0)], **HANGING),
    "Pit_Pak40_Hang": dict(parts=[dict(asset=PITS + "Ger_BXP_GerPit_Pak40", yaw=0.0)], **HANGING),
    "Bunker_Flak88_Hang": dict(parts=[dict(asset=BUNKERS + "Flak88Bunker/Ger_BXP_Flak88Bunker", yaw=0.0)], **HANGING),
    "FieldWorkshop_Hang": dict(parts=[dict(asset="/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/FieldWorkshop/"
                                                 "BXP_FieldWorkshop", yaw=0.0)], **HANGING),
    # supply drop payloads
    "Crate_Big": dict(parts=[dict(asset="/Game/MilitaryBaseCamp/Meshes/SM_CrateBig", yaw=20.0)], **HANGING),
    "MetalDump": dict(parts=[dict(asset=SCAV + "Misc/SM_ScavObj_MetalDump", yaw=20.0)], **HANGING),
    "VehicleParts": dict(parts=[dict(asset=SCAV + "VehicleParts/Meshes/SM_ScavObj_VehicleParts1", yaw=20.0)],
                         **HANGING),
    "RadixiteCore": dict(parts=[dict(asset="/Game/RTS_Survival/Blueprints/Resources/Crystals/RadixiteCores/"
                                           "SM_RadixiteCore_1", yaw=20.0)], **HANGING),
    "RadixiteDeposit": dict(parts=[dict(asset="/Game/RTS_Survival/Blueprints/Resources/ChildBps/Radixite/"
                                              "Resource_Radixite_Crystal_NoDecal", yaw=0.0,
                                    hide=("GeometryCollection",))], **GROUNDED),
    # rounds
    "RailSlug": dict(parts=[dict(asset=PITS + "RailPakBunker/Meshes/SM_RailSlug", yaw=0.0)], **SIDE_PROP),
}


def _unit_scale_attached(actors):
    """The Stuka's bones carry a x100 import scale, so a part attached to its socket comes out 100x: the runtime
    pods component attaches at world scale 1, and so do we."""
    for actor in actors:
        root = actor.root_component
        if root and root.get_attach_parent():
            root.set_world_scale3d(unreal.Vector(1.0, 1.0, 1.0))


def r3b_render_scene(out_name, parts, **kwargs):
    spawned = []
    try:
        actors, comps = _stage_parts(parts, spawned)
        _unit_scale_attached(actors)
        # A commandlet never ticks: block until the parts' shaders are compiled, or new materials render as the
        # default checker (the Breakthrough concrete did).
        unreal.SystemLibrary.execute_console_command(None, "Editor.AsyncAssetCompilationFinishAll")
        return _capture_staged(out_name, actors, comps, spawned, **kwargs)
    finally:
        _destroy(spawned)


def r3b_run(names=None):
    """Renders the named R3B jobs (all when None) with every socket written to the points file."""
    for name in names or list(R3B_JOBS):
        job = dict(R3B_JOBS[name])
        parts = job.pop("parts")
        try:
            r3b_render_scene(R3B_PREFIX + name, parts, sockets=AllSockets(), **dict(LOOK, **job))
        except Exception as error:
            unreal.log_error("[IconsB] render %s failed: %s" % (name, error))
