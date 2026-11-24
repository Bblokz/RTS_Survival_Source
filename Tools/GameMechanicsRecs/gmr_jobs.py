"""Generates every GameMechanicsRecs job file (Jobs/<Category>_<Name>.json) from compact scene specs.

    python gmr_jobs.py            # write all jobs
    python gmr_jobs.py --list     # print the job names per batch

Scenes use the camera grammar of gmr_shots.py; change a rule there and regenerate. Unit choices follow the Shorts
rules: no German resistance infantry (Mauser/MG34/STG/JagerTrupp/LMG) and no Steel Fist squad.
"""
import argparse
import json
import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from gmr_shots import (base_job, crane, ev, follow, key, orbit, pan, push, shot, sweep, track)  # noqa: E402

JOBS_DIR = pathlib.Path(__file__).resolve().parent / "Jobs"

# --- Unit catalogue ---------------------------------------------------------------------------------------------------
V = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
S = "/Game/RTS_Survival/Blueprints/Squads/"
N = "/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/"
P = "/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/"
U = {
    # German armour
    "PzIVH": V + "Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_H", "PzIIIJ": V + "Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_J",
    "StugIII": V + "Ger_Medium/Stug/Ger_Vehicle_StugIII", "PantherG": V + "Ger_Heavy/PantherG/Ger_Vehicle_PantherG",
    "PantherD": V + "Ger_Heavy/PantherD/Ger_Vehicle_PantherD", "Tiger": V + "Ger_Heavy/Tiger/Ger_Vehicle_TigerH1",
    "TigerII": V + "Ger_Heavy/TigerII/Ger_Vehicle_TigerII", "JagdPanther": V + "Ger_Heavy/JagdPanther/Ger_Vehicle_JagdPanther",
    "Puma": V + "Ger_ArmoredCar/Puma/Ger_ArmoredCar_Puma", "Luchs": V + "Ger_Light/PanzerII_Luchs/Ger_Vehicle_PanzerII_Luchs",
    "Hetzer": V + "Ger_Light/Hetzer/Ger_Vehicle_Hetzer", "E50": V + "Ger_Heavy/E50/Ger_Vehicle_E50",
    "PzIVG": V + "Ger_Medium/PanzerIV/Ger_Vehicle_PanzerIV_G",
    "IS1Heat": V + "Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-1_Soviet",  # the Soviet-faction IS-1, firing HEAT shells
    "JagdPzIV": V + "Ger_Medium/JagdPanzerIV70/Ger_Vehicle_JagdPanzerIV70", "Pz38T": V + "Ger_Light/Panzer38T/Ger_Vehicle_Panzer38T",
    "Sdkfz251": V + "Ger_ArmoredCar/Sdkfz-250/Ger_ArmoredCar_Sdkfz250", "Brummbar": V + "Ger_Medium/Brumbar/Ger_Vehicle_Brummbar",
    # Soviet armour
    "T3485": V + "Rus_Medium/T34/Rus_Vehicle_T3485", "T3476": V + "Rus_Medium/T34/Rus_Vehicle_T3476",
    "IS2": V + "Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-2", "IS3": V + "Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-3",
    "KV1": V + "Rus_Heavy/KV1/Rus_Vehicle_KV-1E", "KV2": V + "Rus_Heavy/KV2/Rus_Vehicle_KV-2",
    "SU100": V + "Rus_Medium/Su100/Rus_Vehicle_Su100", "SU85": V + "Rus_Medium/SU85/Rus_Vehicle_Su85",
    "T70": V + "Rus_Light/T70/Rus_Vehicle_T-70", "BT7": V + "Rus_Light/BT-7/Rus_Vehicle_BT-7",
    "SU76": V + "Rus_Light/SU76/Rus_Vehicle_SU76", "BA14": V + "Rus_ArmoredCar/BA-11/Rus_ArmoredCar_BA14",
    "ISU152": V + "Rus_Heavy/ISU152/Rus_Vehicle_ISU152", "T44": V + "Rus_Medium/T44/Rus_Vehicle_T44_100L",
    # German infantry (allowed list only)
    "Scavengers": S + "Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers",
    "MedicScav": S + "Ger/Barracks_Infantry/Scavengers/Squad_Ger_MedicScavengers",
    "Snipers": S + "Ger/Barracks_Infantry/FemaleSniper/Squad_Ger_FemaleSniper",
    "Flame": S + "Ger/Barracks_Infantry/FlameSquad/Squad_Ger_FlameTroopers",
    "Grenadiers": S + "Ger/T2/PanzerGrenadiers/Squad_Ger_PanzerGrenadiers",
    "Pioneers": S + "Ger/T2/SturmPioneer/Squad_Ger_SturmPioneers",
    "Kommando": S + "Ger/EliteInfantry/SturmKommando/Squad_Ger_SturmCommando",
    "IronStorm": S + "Ger/EliteInfantry/IronStorm/Squad_Ger_IronStorm",
    "LightBringers": S + "Ger/EliteInfantry/LightBringers/Squad_Ger_LightBringers",
    "Vultures": S + "Ger/ArmoryUnlocked/Squad_Ger_HazmatVultures",
    "HazSnipers": S + "Ger/ArmoryUnlocked/Squad_Ger_HazmatSniperTeam",
    # Soviet infantry
    "Mosin": S + "Rus/Rus_Squad_Mosin", "SVT": S + "Rus/Rus_Squad_SVT", "PTRS": S + "Rus/Rus_Squad_PTRS",
    "HazEng": S + "Rus/Rus_Squad_HazmatEngineers", "HazBaz": S + "Rus/Rus_Squad_HazmatBazooka",
    "RedHammer": S + "Rus/T2/Rus_T2Squad_RedHammer", "ToxicGuard": S + "Rus/T2/Rus_T2Squad_ToxicGuard",
    "Korovin": S + "Rus/AssaultSquads/Rus_Squad_KorovinAssault", "FlameROKS": S + "Rus/AssaultSquads/Rus_Squad_FlameROKS",
    "ArmoredMosin": S + "Rus/Rus_Squad_ArmoredMosin",
    # Nomadic trucks
    "HQTruck": N + "SKDFZ-7/Ger_HQ_Truck", "RefTruck": N + "GerEco/OpelBlitz_Ref/Ger_Refinery_Truck",
    "BarracksTruck": N + "GerT1/OpelBlitz_Barracks/Ger_Barracks_Truck", "ForgeTruck": N + "GerT1/Opel_LightForge/Ger_Forge_Truck",
    "MechTruck": N + "GerT1/MechanizedDepot/Ger_MechanizedDepot_Truck", "MedTruck": N + "GerT2/GerMedFactory/Ger_MedFactory_Truck",
    "VaultTruck": N + "GerEco/Opel_MetalVault/Ger_MetalVault_Truck",
    "GammaTruck": N + "GerEco/OpelBlitz_RadiationAbsorber/Ger_GammaFacility_Truck",
    "ArmoryTruck": N + "GerT2/Armory/Ger_Armory_Truck",
    "CommsTruck": N + "GerT2/CommunicationsCenter/Ger_CommunicationCenter_Truck",
    "AirbaseTruck": N + "GerT2/AircraftBuilding/Ger_Airbase_Truck",
    "ExperimentalTruck": N + "GerT3/Ger_Experimental_Truck",
    # German pits
    "PitPak40": P + "Ger_BXP_GerPit_Pak40", "PitFlak37": P + "Ger_BXP_GerPit_Flak37", "PitMortar": P + "Ger_BXP_GerPit_Mortar",
    "PitTwinMG": P + "TwinMG42Pit/Ger_BXP_GerPit_TwinMG42", "PitLeFH": P + "Ger_BXP_GerPit_LeFH18",
}

SKY = {
    "storm": "/Game/SkyboxPack_X/Materials/Instances/M_Flowmap_Inst_01.M_Flowmap_Inst_01",
    "dust": "/Game/SkyboxPack_X/Materials/Instances/M_Flowmap_Inst_02.M_Flowmap_Inst_02",
    "haze": "/Game/SkyboxPack_X/Materials/Instances/M_Flowmap_Inst_03.M_Flowmap_Inst_03",
    "dusk": "/Game/SkyboxPack_X/Materials/Instances/M_Flowmap_Inst_04.M_Flowmap_Inst_04",
    "sunset": "/Game/SkyboxPack_X/Materials/Instances/M_Flowmap_Inst_05.M_Flowmap_Inst_05",
    "fire": "/Game/SkyboxPack_X/Materials/Instances/M_Flowmap_Inst_06.M_Flowmap_Inst_06",
}


def spawn(t, tag, unit, offset, yaw, player=1, count=1, columns=0, spacing=1200.0, **settings):
    e = ev(t, "Spawn", tag, ClassPath=U[unit], Player=player, Offset=offset, Yaw=float(yaw), Count=count,
           Columns=columns, Spacing=float(spacing))
    if settings:
        e["Settings"] = settings
    return e


# --- Batch 1: base building ---------------------------------------------------------------------------------------------

BASE_TRUCKS = [  # tag, truck, deploy spot (anchor frame), deploy order time. A deep U (critic round 1): the long
    # axis runs along X so 16:9 sees it side-on and 9:16 looks into its open end; 30 m between neighbours.
    ("B_HQ", "HQTruck", (4000, 0), 1.2),
    ("B_Ref", "RefTruck", (1000, -1500), 2.4),
    ("B_Bar", "BarracksTruck", (1000, 1500), 0.8),
    ("B_Forge", "ForgeTruck", (-2000, -1500), 1.6),
    ("B_Mech", "MechTruck", (-2000, 1500), 2.0),
]
DRIVE_IN = 1500.0  # cm each truck drives toward the camera side before it deploys


def base_expansion(name, map_key, anchor, anchor_yaw, sky, pan_vcamera=(-4500, -6000, 2600), **extra):
    """Five nomadic trucks roll in toward the camera side and unfold together, never overlapping.
    pan_vcamera = the 9:16 PanAcross vantage (the desert one stands higher, behind a ridge)."""
    job = base_job(name, "BaseExpansion", map_key, anchor, anchor_yaw, 34, SkyMaterial=SKY[sky], **extra)
    job["Events"].append(ev(-3, "Resources", Value=200000))
    for tag, truck, spot, order_time in BASE_TRUCKS:
        start = (spot[0] + DRIVE_IN, spot[1])
        job["Events"].append(spawn(-1, tag, truck, start, 180))
        job["Events"].append(ev(order_time, "Deploy", tag, Value=DRIVE_IN, Settings={"Yaw": 180},
                                Text="Building" if tag == "B_HQ" else ""))
    job["Shots"] = [
        # Round 2 retimed the shots so the wide holds the whole unfold wave (bursts start ~8.5 s); round 3 added
        # lead room, kept the near Forge/Mech row inside the 9:16 wides and filled the empty 9:16 ground.
        follow("Convoy", 0, 6, "tag:B_Forge", 25, 70, pitch=10, dist=3000, focal=40, lag=0.8, look=(-400, 0, 300),
               vyaw=(25, 40), vpitch=15, vdist=3000, vfocal=38, vlook=(-200, 0, 520)),
        orbit("AllAtOnce", 6, 12, "", 270, 255, pitch0=36, pitch1=40, dist0=7000, dist1=8000, focal0=22, focal1=22,
              off=(600, -600, 0), look=(400, -300, 300), vyaw=(190, 175), vpitch=(52, 56), vdist=(7600, 7600),
              vfocal=(18, 18), vlook=(-600, 600, 300)),
        orbit("RefUnfold", 12, 18, "tag:B_Ref", 230, 255, pitch0=10, pitch1=14, dist0=2600, dist1=2500, focal0=32,
              aperture=4.0, off=(400, 0, 0), look=(0, 0, 300), vyaw=(230, 240), vpitch=(20, 22), vdist=(3300, 3000),
              vfocal=28, vlook=(-300, 0, 450)),
        # Forge -> Mech only (the HQ in the middle made the tilt bob); from further back both pads stay in frame.
        # 9:16 stands at the end of the row so the pan becomes a near-to-far tilt.
        sweep("PanAcross", 18, 25.5, "tag:B_Forge", "tag:B_Mech", (-7500, 0, 2200), focal=30,
              look=(0, 0, 200), vcamera_offset=pan_vcamera, vfocal=26, off0=(0, 750, 0), off1=(0, -750, 0)),
        crane("Reveal", 25.5, 34, "", 80, 24, 32, 7500, 9500, 28, 22, yaw1=110, off=(1000, 0, 0),
              vyaw=(170, 185), vpitch=(28, 44), vdist=(7800, 9800), vfocal=(21, 18), vlook=(-900, 0, 150)),
    ]
    return job


# Expansion positions relative to the HQ / refinery (anchor frame), read from the GMR_EXPANSION log lines.
HQ_RADAR = (930, -320, 0)
HQ_PLATFORM = (-180, -580, 0)
HQ_HARVESTER = (-60, 990, 0)
REF_CONVERTER = (512, 535, 0)  # logged spot (round 2 used the slot guess)


def bxp_hq(name, map_key, anchor, anchor_yaw, sky, **extra):
    """The HQ stands (deployed during a long warm-up); radar, platform and harvester are built on it, each filmed
    on its own spot."""
    job = base_job(name, "BuildingExpansion", map_key, anchor, anchor_yaw, 34, SkyMaterial=SKY[sky],
                   WarmupSeconds=10.5, **extra)
    job["Events"] += [
        ev(-3, "Resources", Value=300000),
        spawn(-2.5, "HQ", "HQTruck", (0, 0), 180),
        ev(-2.3, "Deploy", "HQ", Settings={"Yaw": 180, "Instant": 1}),
        # In the warm-up (still first, so still slot 0): its 15 s build finishes inside the opening Harvester shot.
        ev(-2.0, "Expand", "HQ", Text="GerHQHarvester"),
        ev(0.6, "Expand", "HQ", Text="GerHQPlatform"),
        ev(0.9, "Expand", "HQ", Text="GerHQRadar"),
        spawn(16.0, "Escort", "PzIVH", (-5200, -2600), 20),
        ev(16.2, "Move", "Escort", TargetOffset=(-1800, -1900), TargetYaw=0, Text="Move"),
    ]
    # One continuous move instead of five cut shots (the user found the cut version not smooth): the camera keeps
    # orbiting the HQ in one direction, visiting each expansion as it finishes (harvester ~3 s, platform ~15.8 s,
    # radar ~21 s) at the angles the critic rounds validated, and ends on a rising reveal. Linear clock: the
    # Catmull-Rom keys give the ease.
    def k(t, off, yaw, pitch, dist, focal, look=(0, 0, 150), vpitch=0.0, vdist=0.0, vfocal=0.0, vlook=None):
        return key(t, "tag:HQ", yaw, pitch, dist, focal, off, look, vpitch=vpitch, vdist=vdist, vfocal=vfocal,
                   vlook=vlook)
    keys = [
        # Keys spaced for a near-even ~10 deg/s orbit (the first continuous take whipped 85 deg in 3.5 s between the
        # platform and the radar).
        k(0.0, HQ_HARVESTER, 105, 18, 2300, 35, vpitch=16, vdist=2500, vlook=(0, 0, 450)),
        k(4.0, HQ_HARVESTER, 140, 19, 2300, 35, vpitch=18, vdist=2500, vlook=(0, 0, 450)),
        k(8.0, (0, 0, 0), 185, 25, 4200, 30, vpitch=27, vdist=4600, vlook=(500, 0, 500)),
        k(12.0, HQ_PLATFORM, 230, 22, 3300, 35, vpitch=20, vdist=3400, vlook=(0, 0, 450)),
        k(17.0, HQ_PLATFORM, 278, 22, 3200, 35, vpitch=20, vdist=3300, vlook=(0, 0, 450)),
        k(21.5, HQ_RADAR, 335, 16, 3000, 32, look=(0, 0, 600)),
        k(25.5, HQ_RADAR, 372, 18, 2800, 32, look=(0, 0, 600)),
        k(34.0, (500, 0, 0), 462, 32, 5800, 26, vpitch=30, vdist=6000, vlook=(0, 0, 500)),
    ]
    # No automatic occlusion lift: its sudden tilts read as bumps, and the angles on this path were validated clear.
    job["Shots"] = [shot("AroundTheHQ", 0, 34, "Orbit", keys, ease="Linear", occlusion=False)]
    return job


def bxp_refinery(name, map_key, anchor, anchor_yaw, sky, **extra):
    """A refinery stands; its converter, Kaskade distillation column and flak stand go up."""
    job = base_job(name, "BuildingExpansion", map_key, anchor, anchor_yaw, 34, SkyMaterial=SKY[sky],
                   WarmupSeconds=13.0, **extra)
    job["Events"] += [
        ev(-3, "Resources", Value=300000),
        spawn(-2.5, "Ref", "RefTruck", (0, 0), 180),
        ev(-2.3, "Deploy", "Ref", Settings={"Yaw": 180, "Instant": 1}),
        # Started in the warm-up so its 15 s build finishes inside the Converter shot (~4 s); still first, slot 0.
        ev(-2.0, "Expand", "Ref", Text="RefConverter"),
        ev(0.6, "Expand", "Ref", Text="RefFlakStand"),
        ev(0.7, "Expand", "Ref", Text="RefKaskade"),
    ]
    job["Shots"] = [
        # From the converter's outer side, the other builds rising behind it; no occlusion lift (the rising flak
        # stand wireframe between camera and converter tipped the camera to top-down in round 2).
        orbit("Converter", 0, 9, "tag:Ref", 70, 25, pitch0=12, pitch1=16, dist0=3400, dist1=3100, focal0=30,
              off=REF_CONVERTER, look=(0, 0, 400), aperture=5.0, vpitch=(16, 20), vdist=(2800, 2600),
              vlook=(0, 0, 550), occlusion=False),
        # The Kaskade and flak stand spawn at the refinery origin and finish ~25.8 s, inside the FlakStand shot.
        push("Kaskade", 9, 18, "tag:Ref", 40, 14, 3600, 3500, 26, 28, pitch1=18, look=(250, -300, 450),
             occlusion=False, vdist=(3400, 3300), vfocal=(24, 26), vlook=(480, -580, 600)),
        # Round 4: the reveal moves to the middle so the clip ends on the flak stand's finish (~26.5 s burst) and
        # its gun crew manning it (~32.3 s).
        crane("Reveal", 18, 25, "tag:Ref", 200, 20, 24, 3600, 5000, 28, 26, yaw1=212, look=(0, 0, 600),
              vpitch=(20, 26), vdist=(3200, 4000), vlook=(0, 0, 900)),
        orbit("FlakStand", 25, 34, "tag:Ref", 300, 260, pitch0=18, pitch1=22, dist0=3400, dist1=3400, focal0=30,
              look=(350, -100, 700), aperture=4.0, vdist=(3600, 3300), vlook=(0, 0, 450), occlusion=False),
    ]
    return job


def batch_base():
    return [
        base_expansion("BaseExpansion_Berlin", "Berlin", (13542, 45944, 0), -90, "storm"),
        # Soviet infantry fired into the base from ~23 s in round 3: clear a wider ring.
        base_expansion("BaseExpansion_Desert", "Desert", (-12825, 3675, 0), -10, "dust", pan_vcamera=(-4500, -6000, 3600),
                       ClearUnitsRadius=40000.0, ClearPlayers=[2],
                       LaunchArgs=["-SurvivalAutoStart", "-SurvivalStartIndex=0", "-SurvivalGrace=3600"]),
        # 15 m clear of the burnt bus parked against the HQ at (11194, 26528) (it clipped the HQ and harvester).
        bxp_hq("BXP_HQ_Berlin", "Berlin", (9694, 26528, 0), -90, "storm"),
        bxp_refinery("BXP_Refinery_Ore", "Ore", (-17085, 8040, 0), 0, "dusk"),
    ]


# --- Batches 2 and 3: vehicle battles and mixed battles ------------------------------------------------------------------

def battle_shots(variant, gap, ger="Ger", sov="Sov", hero=None, close=1.0):
    """Five-shot battle coverage; variant flips the flank so consecutive battles do not look the same.
    close < 1 brings every distance in (infantry fights). VB critic round 1: every 16:9 camera stays on the `side`
    flank (yaws near 180 are written 180 - k * side, no unscaled constants), pitches >= 14 except the hero, 9:16 ground
    shots look ahead along the action (vlook) and the wides are tighter."""
    side = 1 if variant % 2 == 0 else -1
    hero = hero or f"live:{ger}"
    c = close
    return [
        follow("Advance", 0, 6, f"tag:{ger}", 180 - 28 * side, 180 - 12 * side, pitch=16, dist=2400 * c, focal=30,
               vpitch=28, vdist=2300 * c, vfocal=24, vlook=(1500, 0, 150)),
        orbit("EnemyLine", 6, 12, f"tag:{sov}", 32 * side, 12 * side, pitch0=15, pitch1=18, dist0=2500 * c,
              dist1=2200 * c, focal0=32, vpitch=(24, 26), vdist=(2300 * c, 2100 * c), vfocal=(24, 24),
              vlook=(-1500, 0, 150)),
        orbit("Clash", 12, 20, "", 90 * side + 12 * side, 90 * side - 10 * side, pitch0=24, pitch1=27,
              dist0=gap * 0.9, dist1=gap * 0.82, focal0=24,
              vyaw=(180 - 22 * side, 180 - 10 * side), vpitch=(32, 34), vdist=(gap * 0.75, gap * 0.7), vfocal=(24, 24)),
        orbit("Hero", 20, 26, hero, 180 - 25 * side, 180 - 50 * side, pitch0=11, pitch1=13, dist0=1600 * c,
              dist1=1500 * c, focal0=40, aperture=4.0, look=(700, 0, 180), vpitch=(17, 19),
              vdist=(1800 * c, 1700 * c), vfocal=(30, 30), vlook=(900, 0, 150)),
        crane("Aftermath", 26, 32, "", 180 - 30 * side, 22, 36, gap * 0.75, gap * 0.9, 28, 28, yaw1=180 - 50 * side,
              vpitch=(30, 40), vdist=(gap * 0.7, gap * 0.95)),
    ]


def battle(name, category, map_key, anchor, anchor_yaw, sky, ger, sov, gap=7000, variant=0, close=1.0,
           voices=(), duration=32, hero=None, hide=(), **extra):
    """ger / sov: (unit, count, (x, y) offset of the group centre from its line, columns).
    Germans start on -X, Soviets on +X; both attack-move toward the middle at once.
    hide = ((class or label prefix, (x, y), radius cm), ...): runtime-only declutter of trees in the lens."""
    job = base_job(name, category, map_key, anchor, anchor_yaw, duration, SkyMaterial=SKY[sky], **extra)
    for prefix, centre, radius in hide:
        job["Events"].append(ev(-3.5, "Hide", Text=prefix, Offset=centre, Radius=float(radius)))
    for unit, count, offset, columns in ger:
        job["Events"].append(spawn(-1.5, "Ger", unit, (-gap / 2 + offset[0], offset[1]), 0, player=1, count=count,
                                   columns=columns, spacing=1300))
    for unit, count, offset, columns in sov:
        job["Events"].append(spawn(-1.5, "Sov", unit, (gap / 2 + offset[0], offset[1]), 180, player=2, count=count,
                                   columns=columns, spacing=1300))
    job["Events"] += [
        ev(0.3, "AttackMove", "Ger", TargetOffset=(gap * 0.12, 0), TargetYaw=0, Text="Attack"),
        ev(0.5, "AttackMove", "Sov", TargetOffset=(-gap * 0.12, 0), TargetYaw=180),
    ]
    for t, tag, line in voices:
        job["Events"].append(ev(t, "Voice", tag, Text=line))
    job["Shots"] = battle_shots(variant, gap, hero=hero, close=close)
    return job


def batch_vehicle_battles():
    cat = "VehicleBattles"
    return [
        battle("VB_Berlin_Alexanderplatz", cat, "Berlin", (11194, 26528, 0), -90, "storm",
               [("PantherG", 3, (0, 0), 3)], [("T3485", 4, (0, 0), 4)], variant=0),
        # Round 1 spawned the Soviets on the viaduct deck: moved 40 m onto the open sand beyond the ruin.
        battle("VB_Ore_Viaduct", cat, "Ore", (3685, 1052, 0), 0, "dusk",
               [("Tiger", 1, (0, 0), 1), ("PzIVH", 2, (-900, 0), 2)], [("IS2", 1, (0, 0), 1), ("T3485", 2, (900, 0), 2)],
               variant=2),
        # Round 1 put the Germans on an aircraft revetment: 15 m further along the axis and a shorter gap.
        battle("VB_Pripyat_Airfield", cat, "Pripyat", (5000, 4000, 0), 90, "storm",
               [("TigerII", 1, (0, 0), 1), ("StugIII", 2, (-900, 0), 2)], [("KV1", 2, (0, 0), 2), ("SU100", 1, (900, 0), 1)],
               variant=2, gap=6000),
        # Round 1 stood in a dead-pine stand: moved to the open field of candidate C1.
        battle("VB_OderWarthe_Heath", cat, "OderWarthe", (-15000, -15000, 0), 0, "storm",
               [("JagdPanther", 1, (0, 0), 1), ("PantherD", 2, (-900, 0), 2)], [("IS3", 1, (0, 0), 1), ("T3485", 2, (900, 0), 2)],
               variant=3),
        battle("VB_Desert_Plain", cat, "Desert", (-12000, -2000, 0), 30, "dust",
               [("Puma", 2, (0, 0), 2), ("Luchs", 1, (-900, 0), 1)], [("BT7", 2, (0, 0), 2), ("T70", 1, (900, 0), 1), ("BA14", 1, (900, 1500), 1)],
               variant=4, gap=6500, LaunchArgs=["-SurvivalAutoStart", "-SurvivalStartIndex=0", "-SurvivalGrace=3600"]),
        battle("VB_Kessel_Farmstead", cat, "Kessel", (11151, 7151, 0), 45, "dusk",
               [("PzIVH", 3, (0, 0), 3)], [("T3476", 3, (0, 0), 3), ("KV2", 1, (1100, 0), 1)], variant=5,
               hide=(("BP_OrchardTree", (3800, -1100), 1800),)),
        battle("VB_Berlin_ReichstagForecourt", cat, "Berlin", (1200, -15500, 0), 90, "fire",
               [("E50", 1, (0, 0), 1), ("Tiger", 1, (-1300, 600), 1)], [("IS3", 1, (0, 0), 1), ("ISU152", 1, (900, -1600), 1), ("T44", 1, (900, 1600), 1)],
               variant=6),
        battle("VB_Pripyat_Checkpoint", cat, "Pripyat", (-7500, -26250, 0), 0, "storm",
               [("Hetzer", 1, (0, -650), 1), ("JagdPzIV", 1, (0, 650), 1), ("StugIII", 1, (-1300, 0), 1)],
               [("SU85", 1, (0, 0), 1), ("T3476", 2, (900, 0), 2), ("SU76", 1, (1500, 1800), 1)], variant=7,
               hide=(("BP_Scorch_Dest_DeadTree", (-4000, -600), 4000),)),
    ]


MIXED_VOICES = [(0.35, "GerInf", "Move"), (7.0, "GerInf", "Attack"), (13.5, "Ger", "AttackExcited"),
                (19.0, "GerInf", "Confirm"), (24.0, "GerInf", "Attack")]


def mixed(name, map_key, anchor, anchor_yaw, sky, ger_inf, ger_veh, sov_inf, sov_veh, variant, gap=5600, hide=(),
          **extra):
    """Squads lead, vehicles support; German squads are ordered with voice lines and talk in combat.
    Critic round 1: every camera keeps to the `side` flank, never behind its subject's vehicle row, squad 0 spawns on
    the camera side. Round 2: the German vehicle keeps its own lane behind the squads, 9:16 ground shots look ~12 m
    past the subject along the camera bearing (vlook = (-k cos yaw, -k sin yaw, z)), wider firefight (the lines stop
    ~30-35 m apart). hide = ((class or label prefix, (x, y), radius cm), ...)."""
    cat = "MixedBattles"
    job = base_job(name, cat, map_key, anchor, anchor_yaw, 32, SkyMaterial=SKY[sky], **extra)
    for prefix, centre, radius in hide:
        job["Events"].append(ev(-3.5, "Hide", Text=prefix, Offset=centre, Radius=float(radius)))
    side = 1 if variant % 2 == 0 else -1
    for i, unit in enumerate(ger_inf):
        job["Events"].append(spawn(-1.5, "GerInf", unit, (-gap / 2 + 400, -side * (i - (len(ger_inf) - 1) / 2) * 1200), 0))
    for i, unit in enumerate(ger_veh):
        job["Events"].append(spawn(-1.5, "Ger", unit, (-gap / 2 - 1100, -1200 * side + (i - (len(ger_veh) - 1) / 2) * 1800),
                                   0))
    for i, unit in enumerate(sov_inf):
        job["Events"].append(spawn(-1.5, "SovInf", unit, (gap / 2 - 400, -side * (i - (len(sov_inf) - 1) / 2) * 1200), 180,
                                   player=2))
    for i, unit in enumerate(sov_veh):
        job["Events"].append(spawn(-1.5, "Sov", unit, (gap / 2 + 1100, (i - (len(sov_veh) - 1) / 2) * 1800), 180, player=2))
    job["Events"] += [
        ev(0.3, "AttackMove", "GerInf", TargetOffset=(gap * 0.08, 0), TargetYaw=0, Text="Move"),
        # Its own lane, behind the squads (it used to drive through squad 0 and fill the hero shot).
        ev(0.6, "AttackMove", "Ger", TargetOffset=(-gap * 0.3, -1200 * side), TargetYaw=0, Text="Attack"),
        ev(0.5, "AttackMove", "SovInf", TargetOffset=(-gap * 0.08, 0), TargetYaw=180),
        ev(0.7, "AttackMove", "Sov", TargetOffset=(gap * 0.05, 0), TargetYaw=180),
    ]
    for t, tag, line in MIXED_VOICES[1:]:
        job["Events"].append(ev(t, "Voice", tag, Text=line, Settings={"ForceVoice": 1}))
    job["Shots"] = [
        orbit("SquadsAdvance", 0, 7, "tag:GerInf[0]", 35 * side, 15 * side, pitch0=11, pitch1=14, dist0=2300, dist1=2000,
              focal0=32, look=(0, 0, 120), vpitch=(16, 18), vdist=(2100, 1900), vfocal=(28, 28),
              vlook=((-983, -688 * side, 150), (-1159, -311 * side, 150))),
        orbit("Armour", 7, 12, "live:Ger", 105 * side, 80 * side, pitch0=14, pitch1=16, dist0=2000, dist1=1900,
              focal0=36, aperture=4.5, vpitch=(17, 19), vdist=(2200, 2100), vfocal=(26, 26),
              vlook=((311, -1159 * side, 150), (-208, -1182 * side, 150))),
        orbit("EnemyCharge", 12, 18, "tag:SovInf", 100 * side, 125 * side, pitch0=18, pitch1=21, dist0=2300, dist1=2100,
              focal0=30, look=(0, 0, 120), vyaw=(140 * side, 155 * side), vpitch=(20, 22), vdist=(2100, 1900),
              vfocal=(26, 26), vlook=((919, -771 * side, 150), (1088, -507 * side, 150))),
        orbit("Firefight", 18, 26, "", 90 * side + 10 * side, 90 * side - 8 * side, pitch0=22, pitch1=25,
              dist0=gap * 0.95, dist1=gap * 0.9, focal0=28, off=(-400, 0, 0),
              vyaw=(180 - 12 * side, 180 - 6 * side), vpitch=(33, 35), vdist=(gap * 0.8, gap * 0.75), vfocal=(28, 28)),
        orbit("SquadHero", 26, 32, "tag:GerInf[0]", 85 * side, 50 * side, pitch0=12, pitch1=15, dist0=1700, dist1=1500,
              focal0=40, aperture=4.0, look=(0, 0, 110), vyaw=(40 * side, 25 * side), vpitch=(16, 17), vdist=(1900, 1700),
              vfocal=(30, 30), vlook=((-613, -514 * side, 130), (-725, -338 * side, 130))),
    ]
    return job


DESERT_ARGS = {"LaunchArgs": ["-SurvivalAutoStart", "-SurvivalStartIndex=0", "-SurvivalGrace=3600"]}


def batch_mixed():
    return [
        mixed("MB_Berlin_PotsdamerPlatz", "Berlin", (-11375, -14111, 0), 0, "storm",
              ["Grenadiers", "Pioneers"], ["PzIVH"], ["Mosin", "Mosin"], ["T3485"], 0, gap=6400,
              hide=(("BP_Scorch_Dest_DeadTree", (1000, 2300), 1500),)),
        mixed("MB_Ore_Checkpoint", "Ore", (133, 7705, 0), 90, "dusk",
              ["Scavengers", "IronStorm"], ["StugIII"], ["HazEng", "Mosin"], ["T3476"], 1),
        mixed("MB_Pripyat_RiverRoad", "Pripyat", (21500, -24400, 0), 90, "storm",
              ["Kommando", "Grenadiers"], ["PantherG"], ["RedHammer", "Mosin"], ["SU85"], 2),
        # Round 2: the Germans started on an 18 m bank and detoured 72 m; moved 40 m along +X (scouted first).
        mixed("MB_OderWarthe_Heathland", "OderWarthe", (-4000, -25000, 0), 0, "storm",
              ["Flame", "Pioneers"], ["Hetzer"], ["Mosin", "Mosin"], ["BT7"], 3),
        mixed("MB_Desert_Oasis", "Desert", (10300, 1000, 0), 200, "haze",
              ["LightBringers", "Scavengers"], ["Puma"], ["ToxicGuard", "Mosin"], ["T70"], 4, **DESERT_ARGS),
        mixed("MB_Kessel_Convoy", "Kessel", (-7, 15906, 0), 90, "dusk",
              ["Vultures", "Scavengers"], ["PzIIIJ"], ["Korovin", "Mosin"], ["KV1"], 4),
        # Round 2: every +Y camera stood behind a ruin at (15000, 10000); 20 m along -Y (scouted before rendering).
        mixed("MB_Berlin_TramLine", "Berlin", (15000, 8000, 0), 0, "storm",
              ["Grenadiers", "Snipers"], ["Tiger"], ["Mosin", "PTRS"], ["IS2"], 6),
        mixed("MB_Ore_RadixiteField", "Ore", (-3819, 13080, 0), 90, "dusk",
              ["HazSnipers", "Scavengers"], ["Luchs"], ["SVT", "ArmoredMosin"], ["T3485"], 7,
              hide=(("Resource_Radixite_Crystal", (850, -900), 900),)),
    ]


# --- Batch 4: aircraft strafing runs ---------------------------------------------------------------------------------

AIR = "/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/"


# Default air-strike cut (critic round 1 timings: first fire ~12-15 s on Ore / Pripyat / Desert).
AIR_PLAN = (("Column", 0, 6), ("Inbound", 6, 10.5), ("StrafeRun", 10.5, 18.5), ("Wreckage", 18.5, 25),
            ("Aftermath", 25, 32))


def strike_orbit(name, start, end, yaw0=120, yaw1=95, dist0=4200, dist1=3800):
    """Elevated orbit locked on the column (~21 m up): whatever heading the run takes (the ability flies to the line's
    midpoint from a random map-edge point), the column stays in the lower half with sky above for the aircraft."""
    return orbit(name, start, end, "tag:Column", yaw0, yaw1, pitch0=30, pitch1=28, dist0=dist0, dist1=dist1, focal0=26,
                 look=(0, 0, 600), vpitch=(36, 34), vdist=(dist0 + 400, dist1 + 400), vfocal=(20, 20))


def air_strike(name, map_key, anchor, anchor_yaw, sky, ability, targets, aircraft_class, aircraft=0, column_start=-3000.0,
               move_to=4500.0, ability_time=1.0, plan=AIR_PLAN, column_pitch=24, column_yaw=(200, 190),
               inbound_dist=3200, aftermath_yaw=(200, 228), duration=32, **extra):
    """A Soviet column moves along +X; a German air-support ability strafes along the column."""
    job = base_job(name, "AirStrikes", map_key, anchor, anchor_yaw, duration, SkyMaterial=SKY[sky], **extra)
    column_x = float(column_start)
    column_gap = 1500.0
    for i, (unit, count) in enumerate(targets):
        for _ in range(count):
            job["Events"].append(spawn(-1.5, "Column", unit, (column_x, (i % 2) * 500 - 250), 0, player=2))
            column_x -= column_gap
    settings = {"Repeat": 1, "ClaimSeconds": 30}
    if aircraft:
        settings["Aircraft"] = aircraft
    job["Events"] += [
        ev(0.2, "Move", "Column", TargetOffset=(move_to, 0), TargetYaw=0),
        ev(ability_time, "Ability", "Air", Text=ability, Offset=(-1500, 0), TargetOffset=(2500, 0), Settings=settings,
           ClassPath=AIR + aircraft_class),
    ]
    # The tag centroid of a pair is empty sky between the aircraft, so aircraft shots track the lead aircraft (Air[0]).
    makers = {
        "Column": lambda a, b: follow("Column", a, b, "tag:Column", column_yaw[0], column_yaw[1], pitch=column_pitch,
                                      dist=2800 + (column_pitch - 18) * 50, focal=30, vpitch=column_pitch + 6,
                                      vdist=2600, vfocal=22),
        "Inbound": lambda a, b: follow("Inbound", a, b, "tag:Air[0]", 205, 150, pitch=14, dist=inbound_dist, focal=34,
                                       lag=0.08, look=(0, 0, -500), vpitch=20, vdist=inbound_dist + 400, vfocal=24),
        "Approach": lambda a, b: follow("Approach", a, b, "tag:Air[0]", 150, 205, pitch=14, dist=inbound_dist,
                                        focal=34, lag=0.08, look=(0, 0, -500), vpitch=20, vdist=inbound_dist + 400,
                                        vfocal=24),
        "StrafeRun": lambda a, b: strike_orbit("StrafeRun", a, b),
        "Overflight": lambda a, b: strike_orbit("Overflight", a, b, 140, 120, 4600, 4200),
        "SecondPass": lambda a, b: strike_orbit("SecondPass", a, b, 95, 75, 4400, 4000),
        "Wreckage": lambda a, b: orbit("Wreckage", a, b, "tag:Column", 60, 20, pitch0=24, pitch1=28, dist0=3000,
                                       dist1=2800, focal0=30, vpitch=(28, 30), vdist=(3200, 3000), vfocal=(22, 22)),
        "Aftermath": lambda a, b: crane("Aftermath", a, b, "tag:Column", aftermath_yaw[0], 24, 40, 3000, 5000, 28, 22,
                                        yaw1=aftermath_yaw[1], vpitch=(28, 44), vdist=(3200, 5400), vfocal=(20, 18)),
    }
    job["Shots"] = [makers[kind](a, b) for kind, a, b in plan]
    return job


def batch_air():
    return [
        # The cannon Stukas circle first: the first pass only hits ~29.5 s after the ability (critic round 1).
        air_strike("AS_Berlin_Ju87G", "Berlin", (8000, 15000, 0), 45, "storm", "GA_GerJu87GTankBusters",
                   [("T3485", 2), ("IS2", 1), ("SU100", 1)], "JU_87VTO/Ju87G/Ger_Aircraft_StukaJu87G_Airborne",
                   duration=39, column_pitch=30, aftermath_yaw=(160, 130),
                   plan=(("Column", 0, 6), ("Inbound", 6, 12.5), ("Overflight", 12.5, 20), ("Approach", 20, 27.5),
                         ("StrafeRun", 27.5, 34.5), ("Aftermath", 34.5, 39))),
        air_strike("AS_Pripyat_Horten", "Pripyat", (21500, -22500, 0), 90, "storm", "GA_2xHortentrafing",
                   [("BT7", 2), ("Mosin", 2), ("T3476", 1)], "HO229/Ger_Aircraft_Horten229_Airborne",
                   column_start=-1000.0, move_to=3200.0, ability_time=3.0, inbound_dist=3800,
                   plan=(("Column", 0, 6), ("Inbound", 6, 10.5), ("StrafeRun", 10.5, 18.5), ("SecondPass", 18.5, 26),
                         ("Aftermath", 26, 32))),
        air_strike("AS_Ore_Ju87", "Ore", (-100, 2000, 0), 90, "dusk", "GA_3xJu87Strafing",
                   [("Mosin", 2), ("BT7", 1), ("BA14", 1)], "JU_87VTO/Ger_Aircraft_StukaJu87_Airborne", column_pitch=26),
        air_strike("AS_Desert_Bf110", "Desert", (-5000, 5000, 0), 60, "dust", "GA_2xBf110Strafing",
                   [("T3476", 2), ("HazEng", 1), ("T70", 1)], "BF109/Ger_Aircraft_BF110Airborne", duration=36,
                   column_yaw=(160, 170),
                   plan=(("Column", 0, 6), ("Inbound", 6, 12.5), ("StrafeRun", 12.5, 19.5), ("Wreckage", 19.5, 24.5),
                         ("SecondPass", 24.5, 31.5), ("Aftermath", 31.5, 36)), **DESERT_ARGS),
    ]


# --- Batch 5: trenches and the supply point ---------------------------------------------------------------------------

# The German field trench kit does not draw in these renders (every piece invisible, the Soviet kit draws; found
# 2026-11-17 with top-down probes), so the trench jobs dig the Soviet kit until the German meshes are fixed: set 0 then.
TRENCH_KIT = 1


def trench_defence(name, map_key, anchor, anchor_yaw, sky, vpitch_add=0, **extra):
    """Three German pits get joined by dug trenches, infantry occupies them, then a Soviet wave tries the line."""
    job = base_job(name, "Trenches", map_key, anchor, anchor_yaw, 44, SkyMaterial=SKY[sky], **extra)
    job["Events"] += [
        spawn(-2.5, "Pits", "PitTwinMG", (0, -2800), 0),
        spawn(-2.5, "Pits", "PitPak40", (300, 0), 0),
        # Yaw 110 turns the mortar pit's rear trench port toward the Pak 40 pit (at yaw 0 it faced away and B stalled).
        spawn(-2.5, "Pits", "PitMortar", (-700, 2800), 110),
        # The Pak 40 pit has one trench port: A ends on it and B (from the mortar pit) meets A there. Pass 1 started B
        # on that port and the planner looped four corners in place.
        ev(0.5, "Trench", "TrenchA", TargetTag="Pits", Value=0.32,
           Settings={"From": 0, "To": 1, "Bend": -400, "Kit": TRENCH_KIT}),
        ev(1.2, "Trench", "TrenchB", TargetTag="Pits", Value=0.32,
           Settings={"From": 2, "To": 1, "Bend": -300, "Kit": TRENCH_KIT}),
        # Pass 1 spawned them at 9.0 s, the frame the follow shot opened on: the pivot fell back to the anchor and
        # the shot showed an empty field. They wait out of the Dig shot behind the line and set off at 8.5 s.
        # Critic round 1: closer (they never reached the trench on camera) and both squads in trench A, so the
        # garrison reads as men in a trench instead of specks spread over 45 m.
        spawn(-1.5, "GerInf", "Grenadiers", (-2600, -1500), 0),
        spawn(-1.5, "GerInf", "Pioneers", (-2600, 1500), 0),
        ev(8.5, "Garrison", "GerInf", TargetTag="TrenchA", Text="Move", TargetYaw=0, Index=0),
        ev(8.6, "Garrison", "GerInf", TargetTag="TrenchA", Index=1),
        # Pass 2: closer, out of the tree line on the Oder-Warthe map.
        spawn(14.0, "SovInf", "Mosin", (6200, -1800), 180, player=2),
        spawn(14.0, "SovInf", "Mosin", (6200, 1800), 180, player=2),
        spawn(14.0, "SovInf", "HazEng", (7000, 0), 180, player=2),
        spawn(14.0, "Sov", "T3476", (8200, 0), 180, player=2),
        ev(14.5, "AttackMove", "SovInf", TargetOffset=(600, 0), TargetYaw=180),
        ev(15.0, "AttackMove", "Sov", TargetOffset=(1800, 0), TargetYaw=180),
        ev(22.0, "Voice", "GerInf", Text="Attack", Settings={"ForceVoice": 1}),
        ev(30.0, "Voice", "GerInf", Text="AttackExcited", Settings={"ForceVoice": 1}),
    ]
    va = vpitch_add
    job["Shots"] = [
        # Wide on the middle pit, held until both trenches are dug (~11 s); 9:16 turns the line bottom-to-top.
        orbit("Dig", 0, 11, "tag:Pits[1]", 200, 185, pitch0=40, pitch1=34, dist0=5800, dist1=4600, focal0=28,
              look=(-300, 0, 100), vyaw=(265, 255), vpitch=(48, 42), vdist=(6200, 5000), vfocal=(20, 20)),
        follow("ManTheLine", 11, 16, "tag:GerInf[0]", 200, 150, pitch=24, dist=1600, focal=30,
               vpitch=28 + va, vdist=1700, vfocal=22),
        # From behind the line: the trench in the foreground, the attackers beyond.
        orbit("SovietsCome", 16, 22, "", 195, 185, pitch0=20, pitch1=22, dist0=4200, dist1=3600, focal0=30,
              off=(1500, 0, 0), vpitch=(28 + va, 30 + va), vdist=(5000, 4600), vfocal=(20, 20)),
        orbit("InTheTrench", 22, 30, "tag:GerInf[0]", 210, 235, pitch0=14, pitch1=14, dist0=1100, dist1=1100, focal0=35,
              look=(600, 0, 80), aperture=4.0, vpitch=(18, 18), vdist=(1300, 1300), vfocal=(26, 26)),
        orbit("Assault", 30, 37, "", 120, 80, pitch0=24, pitch1=26, dist0=3600, dist1=3200, focal0=30, off=(1200, 0, 0),
              vyaw=(200, 190), vpitch=(28, 30), vdist=(5000, 4600), vfocal=(20, 20)),
        crane("HoldTheLine", 37, 44, "tag:Pits[1]", 190, 18, 34, 3200, 5000, 28, 26, yaw1=150,
              vpitch=(24, 40), vdist=(3400, 5400)),
    ]
    return job


def supply_point_berlin():
    """German infantry takes the Berlin supply point: the flag rises, the crane loads an Opel truck from the HQ."""
    # Pass 1 ran 46 s and the truck only reached the crane at 35 s: the point starts Soviet-owned, so the capture is
    # lower + raise (2 x CaptureDuration). Shorter timings and closer start/base put every beat inside 40 s:
    # infantry in range ~5 s, flag down/up ~5-13 s, truck out of the base ~14 s, at the crane ~19 s, loaded ~25 s.
    job = base_job("Supply_Berlin_TowerCrane", "SupplyPoints", "Berlin", (0, 0, 0), 0, 44,
                   SkyMaterial=SKY["storm"], AnchorActor="BP_SupplyPoint_TowerCrane", ClearUnitsRadius=6000.0)
    job["Events"] += [
        ev(-3, "Resources", Value=200000),
        spawn(-2.6, "HQ", "HQTruck", (-5200, 2600), 0),
        ev(-2.5, "Deploy", "HQ", Settings={"Yaw": 0, "Instant": 1}),
        ev(-2.0, "Capture", "SP", Settings={"CaptureDuration": 4, "SuppliesReadyTime": 1, "TruckLoadingTime": 6},
           TargetOffset=(-3400, 2200)),
        ev(-1.8, "Claim", "Truck", ClassPath="/Game/RTS_Survival/Blueprints/GroundVehicles/SupplyTruck/BP_Ger_OpelSupplyTruck",
           Settings={"ClaimSeconds": 60}),
        spawn(-1.5, "GerInf", "Scavengers", (-2500, -900), 0),
        spawn(-1.5, "GerInf", "Grenadiers", (-2500, 700), 0),
        spawn(-1.5, "GerVeh", "Puma", (-3800, -1600), 0),
        ev(0.5, "Move", "GerInf", TargetOffset=(-300, -200), TargetYaw=0, Text="Move"),
        # The Puma screens the far flank, away from the truck lane and the cameras.
        ev(0.8, "Move", "GerVeh", TargetOffset=(-1400, -2300), TargetYaw=20),
        ev(13.0, "Voice", "GerInf", Text="Confirm", Settings={"ForceVoice": 1}),
    ]
    # Critic round 1 (measured beats): flag down ~4-7 s, up ~7-11 s, truck out ~17 s, parked ~26 s, loaded ~31 s,
    # rolling away ~36 s.
    job["Shots"] = [
        crane("TheCrane", 0, 4, "tag:SP", 200, 24, 16, 3800, 3200, 24, 28, yaw1=215, look=(0, 0, 500),
              vpitch=(40, 30), vdist=(4200, 3600)),
        # Locked off where the flag change reads (the follow swung onto the crane and missed the German flag rising).
        pan("FlagChange", 4, 12, "tag:SP", "tag:SP", 126, pitch=15, dist=2420, focal=30, look=(-460, -150, 200),
            vpitch=19, vdist=2330, vfocal=22),
        orbit("Secured", 12, 18, "tag:SP", 195, 215, pitch0=18, pitch1=18, dist0=3200, dist1=2800, focal0=30,
              look=(0, 0, 700), vpitch=(24, 24), vdist=(3400, 3000), vfocal=(22, 22)),
        follow("TruckArrives", 18, 25, "tag:Truck", 70, 90, pitch=22, dist=2600, focal=30, lag=0.4, vpitch=26,
               vdist=2500, vfocal=22),
        orbit("Loading", 25, 34.5, "tag:Truck", 215, 250, pitch0=34, pitch1=34, dist0=2100, dist1=2100, focal0=32,
              look=(0, 0, 250), vpitch=(36, 36), vdist=(2400, 2400), vfocal=(22, 22)),
        pan("SuppliesOut", 34.5, 44, "tag:Truck", "tag:Truck", 30, pitch=18, dist=2800, focal=30, vpitch=24, vfocal=22),
    ]
    return job


def batch_trench_supply():
    return [
        trench_defence("TR_OderWarthe_Line", "OderWarthe", (-19000, -24000, 0), 0, "storm"),
        trench_defence("TR_Ore_Bench", "Ore", (-17085, 8040, 0), 0, "dusk", vpitch_add=8),
        supply_point_berlin(),
    ]


# --- Batch 6: points of interest -----------------------------------------------------------------------------------------

def poi(name, map_key, anchor, anchor_yaw, sky, size, yaw0=200, patrol=("Scavengers", (-1.5, -1.3), (1.5, -1.0)),
        tank=None, look_height=600, tank_path=((-1.6, 0.9), (1.4, 0.7)), low=False, reveal_yaw=None, ground_z=0.0,
        pitch_add=0.0, scale=None, hide=(), **extra):
    """A landmark: establishing arc, a tilt from a passing patrol up to the landmark, then a rising reveal.
    size = landmark radius cm; reveal_yaw = where the reveal starts (ends 40 deg further); ground_z = how far the anchor
    sits above the ground (an anchor that dropped onto a roof); pitch_add lifts the establishing pitch (horizon band);
    scale = a replacement Scale shot; hide = ((class/label prefix, world (x, y), radius), ...) hidden for this take."""
    clear = extra.pop("ClearUnitsRadius", size * 3)
    job = base_job(name, "POI", map_key, anchor, anchor_yaw, 22, SkyMaterial=SKY[sky], ClearUnitsRadius=clear, **extra)
    job["Events"] += hide_events(anchor, anchor_yaw, hide)
    unit, start, end = patrol
    job["Events"] += [
        spawn(-1.5, "Patrol", unit, (start[0] * size, start[1] * size), 0),
        ev(0.4, "Move", "Patrol", TargetOffset=(end[0] * size, end[1] * size), TargetYaw=0),
    ]
    if tank:
        job["Events"] += [
            spawn(-1.5, "Tank", tank, (tank_path[0][0] * size, tank_path[0][1] * size), 0),
            ev(0.6, "Move", "Tank", TargetOffset=(tank_path[1][0] * size, tank_path[1][1] * size), TargetYaw=0),
        ]
    look = (0, 0, look_height)
    ground = (0, 0, -ground_z)
    reveal_yaw = yaw0 - 70 if reveal_yaw is None else reveal_yaw
    # Tall landmarks (low=True) are shot from near the ground looking up, closer, so they tower over the frame.
    est_pitch = ((3, 7) if low else (16, 20))
    est_pitch = (est_pitch[0] + pitch_add, est_pitch[1] + pitch_add)
    est_dist = (size * 2.0, size * 1.8) if low else (size * 3.2, size * 3.0)
    job["Shots"] = [
        orbit("Establish", 0, 9, "", yaw0, yaw0 - 45, pitch0=est_pitch[0], pitch1=est_pitch[1], dist0=est_dist[0],
              dist1=est_dist[1], focal0=26, off=ground, look=look, vpitch=(est_pitch[0] + 4, est_pitch[1] + 4),
              vdist=(est_dist[0] * 1.05, est_dist[1] * 1.05), vfocal=(20, 20)),
        # The camera stands ~7 m up beside the patrol and tilts from it up to the landmark (pass 1 followed the patrol
        # and never showed the landmark; decorative rubble and foliage have no collision, so it stays high).
        scale or pan("Scale", 9, 15, "tag:Patrol", "", 270, pitch=14, dist=3000, focal=24,
                     off1=(0, 0, look_height - ground_z), look=(0, 0, 150), vpitch=18, vfocal=18),
        crane("Reveal", 15, 22, "", reveal_yaw, 10 if low else 16, 28 if low else 40, size * (2.2 if low else 2.8),
              size * (3.2 if low else 4.0), 30, 24, yaw1=reveal_yaw - 40, off=ground, look=look,
              vpitch=(14 if low else 20, 34 if low else 46), vdist=(size * 2.4, size * (3.4 if low else 4.4))),
    ]
    return job


def batch_poi():
    return [
        poi("POI_Berlin_Reichstag", "Berlin", (8100, -16700, 0), 180, "fire", 4200, look_height=1200, pitch_add=4,
            patrol=("Scavengers", (-1.5, -1.8), (1.5, -1.7))),
        poi("POI_Berlin_BrandenburgGate", "Berlin", (0, -8300, 0), 0, "storm", 2600, look_height=900, yaw0=295,
            reveal_yaw=130, pitch_add=4),
        # 2026-11-17 retake: the low hero orbit (low=True, R 3600) drove through the ruins south of the tower and the
        # Scale camera (bearing 270) stood in the ruin 60 m west; scouted top-down: normal height like the Zoo tower,
        # closer, and the Scale camera behind the patrol on the open -Y strip, turning to the tower.
        poi("POI_Berlin_FlakTower", "Berlin", (22750, 46000, 0), 180, "storm", 2800, look_height=1200,
            ground_z=2305.0, pitch_add=4,
            scale=pan("Scale", 9, 15, "tag:Patrol", "", 180, pitch=14, dist=3000, focal=24, off1=(0, 0, 1200 - 2305.0),
                      look=(0, 0, 150), vpitch=18, vfocal=18)),
        poi("POI_Berlin_ForgeStation", "Berlin", (363, 1382, 0), 0, "storm", 2500, look_height=500, pitch_add=4,
            ClearPlayers=[2]),
        poi("POI_Berlin_PowerStation", "Berlin", (13800, 35000, 0), 90, "fire", 3500, look_height=900),
        poi("POI_Ore_Viaduct", "Ore", (3685, -2948, 0), 0, "dusk", 4500, tank="PzIVH", look_height=1500, yaw0=295,
            reveal_yaw=130),
        poi("POI_Ore_MinersShelter", "Ore", (-23242, 10730, 0), 90, "dusk", 900, look_height=400, low=True, pitch_add=4),
        # Re-staged on the hangar the candidate scout found (pass 1 framed an empty apron at (7500, 6000)).
        poi("POI_Pripyat_Hangars", "Pripyat", (-3000, 2900, 0), 90, "storm", 3500, look_height=900),
        poi("POI_Pripyat_RailBridge", "Pripyat", (27500, 7645, 0), 90, "storm", 2400, look_height=500, reveal_yaw=40,
            patrol=("Grenadiers", (-0.12, -2.2), (-0.12, -0.6)), tank="StugIII", tank_path=((-0.12, -1.6), (-0.12, 1.4)),
            scale=follow("Scale", 9, 15, "tag:Tank", 110, 70, pitch=14, dist=3000, focal=26)),
        poi("POI_OderWarthe_Werk1", "OderWarthe", (8800, -4300, 0), 0, "storm", 2500, look_height=500,
            patrol=("Scavengers", (-1.2, -0.7), (1.2, -0.6))),
        poi("POI_Kessel_DC3Crash", "Kessel", (-19672, 2250, 0), 0, "dusk", 1700, look_height=400, yaw0=160, pitch_add=4),
        poi("POI_Desert_Oasis", "Desert", (15500, -3600, 0), 0, "haze", 4200, look_height=300,
            patrol=("Scavengers", (-1.5, -1.9), (1.5, -1.8)), **DESERT_ARGS),
    ]


# --- Custom: a large battle on the Reichstag forecourt ---------------------------------------------------------------

def reichstag_battle():
    """German 4x Panzer IV G, 2x Jagdpanzer IV, 2x Tiger H1 and 6 Scavenger squads against 4x T-34/76, 2x T-34/85 and
    2x IS-1 (Soviet, HEAT) on the forecourt, spread wider than the house battles (~22 m between vehicles, 24 m between
    squads, 80 m between the lines). The battle runs parallel to the facade; every camera stands on the far side
    (+Y) so the Reichstag is the backdrop. 9:16 ground shots look ~12 m past the subject along the camera bearing."""
    gap = 8000.0
    front, back = -gap / 2, gap / 2
    # Scouted: 15 m further from the ruin behind the Soviet rows (their back rows stood inside it).
    job = base_job("MB_Berlin_ReichstagBattle", "MixedBattles", "Berlin", (-800, -17000, 0), 90, 40,
                   SkyMaterial=SKY["fire"], ClearUnitsRadius=12000.0)
    # Runtime only: the dead trees on the camera side of the square.
    events = [ev(-3.5, "Hide", Text="BP_Scorch_Dest_Dead", Offset=(0, 3000), Radius=9000.0),
              # The fallen trunks (cover logs) that crossed the Tiger shot and the aftermath foreground.
              ev(-3.5, "Hide", Text="BP_Cover_DeadTreeLog", Offset=(0, 3000), Radius=9000.0)]
    for i, y in enumerate((-2400, 0, 2400)):
        events.append(spawn(-1.5, "GerInf", "Scavengers", (front + 600, y), 0))
        events.append(spawn(-1.5, "GerInf", "Scavengers", (front - 600, y), 0))
    for y in (-3300, -1100, 1100, 3300):
        events.append(spawn(-1.5, "GerPz", "PzIVG", (front - 2200, y), 0))
    for y in (-3200, 3200):
        events.append(spawn(-1.5, "GerJpz", "JagdPzIV", (front - 4200, y), 0))
    for y in (-1100, 1100):
        events.append(spawn(-1.5, "GerTiger", "Tiger", (front - 4200, y), 0))
    for y in (-3300, -1100, 1100, 3300):
        events.append(spawn(-1.5, "SovT34", "T3476", (back + 400, y), 180, player=2))
    for y in (-2200, 2200):
        events.append(spawn(-1.5, "SovT85", "T3485", (back + 1800, y), 180, player=2))
    for y in (-1100, 1100):
        events.append(spawn(-1.5, "SovIS", "IS1Heat", (back + 3200, y), 180, player=2))
    events += [
        ev(0.3, "AttackMove", "GerInf", TargetOffset=(gap * 0.05, 0), TargetYaw=0, Text="Move"),
        ev(0.5, "AttackMove", "GerPz", TargetOffset=(-gap * 0.05, 0), TargetYaw=0, Text="Attack"),
        ev(0.6, "AttackMove", "GerJpz", TargetOffset=(-gap * 0.25, 0), TargetYaw=0),
        ev(0.7, "AttackMove", "GerTiger", TargetOffset=(-gap * 0.2, 0), TargetYaw=0),
        ev(0.4, "AttackMove", "SovT34", TargetOffset=(gap * 0.05, 0), TargetYaw=180),
        ev(0.6, "AttackMove", "SovT85", TargetOffset=(gap * 0.15, 0), TargetYaw=180),
        ev(0.8, "AttackMove", "SovIS", TargetOffset=(gap * 0.25, 0), TargetYaw=180),
    ]
    for t, tag, line in ((7.0, "GerInf", "Attack"), (13.5, "GerTiger", "AttackExcited"), (19.0, "GerInf", "Confirm"),
                         (25.0, "GerPz", "Attack"), (31.0, "GerInf", "AttackExcited")):
        events.append(ev(t, "Voice", tag, Text=line, Settings={"ForceVoice": 1}))
    job["Events"] += events
    job["Shots"] = [
        # The German force advancing with the Reichstag behind it (take 1 was too far: units were specks; its 9:16
        # looked down the battle axis at the map edge).
        orbit("Establish", 0, 6, "", 120, 100, pitch0=22, pitch1=18, dist0=6000, dist1=5200, focal0=24,
              off=(-2000, 0, 0), look=(0, -1500, 400), vyaw=(125, 110), vpitch=(28, 24), vdist=(6000, 5400),
              vfocal=(20, 20)),
        orbit("SquadsAdvance", 6, 12, "tag:GerInf[1]", 40, 20, pitch0=14, pitch1=16, dist0=2600, dist1=2300, focal0=30,
              look=(0, 0, 120), vpitch=(18, 20), vdist=(2300, 2100), vfocal=(26, 26),
              vlook=((-919, -771, 150), (-1128, -410, 150))),
        orbit("Tigers", 12, 18, "tag:GerTiger[1]", 105, 80, pitch0=14, pitch1=16, dist0=2400, dist1=2200, focal0=34,
              aperture=4.5, look=(0, 0, 180), vpitch=(18, 20), vdist=(2400, 2300), vfocal=(26, 26),
              vlook=((311, -1159, 150), (-208, -1182, 150))),
        orbit("SovietLine", 18, 24, "tag:SovT34[2]", 110, 130, pitch0=18, pitch1=21, dist0=3200, dist1=2800, focal0=30,
              look=(0, 0, 150), vyaw=(140, 155), vpitch=(22, 24), vdist=(2600, 2400), vfocal=(26, 26),
              vlook=((919, -771, 150), (1088, -507, 150))),
        orbit("Clash", 24, 33, "", 100, 85, pitch0=24, pitch1=27, dist0=gap * 0.85, dist1=gap * 0.78, focal0=26,
              look=(0, -800, 200), vyaw=(168, 174), vpitch=(33, 35), vdist=(gap * 0.8, gap * 0.75), vfocal=(26, 26)),
        crane("Aftermath", 33, 40, "tag:GerPz", 150, 22, 34, 4500, 6500, 28, 26, yaw1=130,
              vpitch=(30, 40), vdist=(4800, 7000)),
    ]
    return job


# One of each German nomadic building, deployed one after another on a snake through a 4 x 3 grid (42 m apart, so no
# two footprints touch). Each truck appears just before its turn (the camera is still on the previous building) and
# unfolds at once; as it starts, the camera glides from the previous building to it and holds while it unfolds.
NOMADIC_SEQUENCE = (
    ("N_HQ", "HQTruck"), ("N_Ref", "RefTruck"), ("N_Vault", "VaultTruck"), ("N_Gamma", "GammaTruck"),
    ("N_Bar", "BarracksTruck"), ("N_Forge", "ForgeTruck"), ("N_Mech", "MechTruck"), ("N_Armory", "ArmoryTruck"),
    ("N_Med", "MedTruck"), ("N_Comms", "CommsTruck"), ("N_Air", "AirbaseTruck"), ("N_Exp", "ExperimentalTruck"),
)
NOMADIC_STEP = 6.0      # s between deploys
NOMADIC_PAN = 2.0       # s the camera takes to reach the new building
# A truck needs ~4.5 s from its deploy order until the unfold starts (first 16:9 take: bursts at order + 4-5 s), so
# each order goes out that much before its turn and the camera sets off as the unfold begins.
NOMADIC_UNFOLD_DELAY = 4.5
# Scouted: three rows of four on the open middle of the start area (the outer rows of a looser grid stood in ruins,
# against a walled compound and the fortress east of it); 32 m between spots still leaves wide gaps.
NOMADIC_ROWS_X = (4200, 1000, -2200)
NOMADIC_COLS_Y = (-4800, -1600, 1600, 4800)


def nomadic_spots():
    spots = []
    for r, x in enumerate(NOMADIC_ROWS_X):
        cols = NOMADIC_COLS_Y if r % 2 == 0 else tuple(reversed(NOMADIC_COLS_Y))
        spots += [(x, y) for y in cols]
    return spots


def nomadic_sequence():
    spots = nomadic_spots()
    first = 5.0  # the HQ truck is shown standing before it unfolds
    end = first + NOMADIC_STEP * len(NOMADIC_SEQUENCE) + 2.0
    # Buildings stay grey (construction look) for 9-29 s after they unfold; the last, the Experimental (29 s),
    # finishes ~100 s in. The closing orbit runs past that so the take ends on the finished base.
    duration = end + 25.0
    job = base_job("BaseExpansion_Berlin_OneByOne", "BaseExpansion", "Berlin", (13542, 45944, 0), -90, duration,
                   SkyMaterial=SKY["storm"], ClearUnitsRadius=12000.0)
    job["Events"].append(ev(-3, "Resources", Value=400000))
    keys = []
    for i, ((tag, truck), spot) in enumerate(zip(NOMADIC_SEQUENCE, spots)):
        t = first + NOMADIC_STEP * i
        # The first truck is in place from the start; later ones appear while the camera still frames the previous,
        # and get their order right away so their unfold starts as the camera sets off toward them.
        job["Events"].append(spawn(-1.0 if i == 0 else t - NOMADIC_UNFOLD_DELAY - 0.4, tag, truck, spot, 180))
        job["Events"].append(ev(t - NOMADIC_UNFOLD_DELAY, "Deploy", tag, Value=0, Settings={"Yaw": 180},
                                Text="Building" if i == 0 else ""))
        # Slow yaw drift (215 -> ~250 over the take) for parallax; the camera keeps a high three-quarter view so the
        # neighbouring buildings never block the subject.
        yaw = 215 + 3.0 * i
        if i == 0:
            keys.append(key(0, "", yaw, 26, 4600, 30, (spot[0], spot[1], 0), (0, 0, 400),
                            vpitch=32, vdist=5200, vfocal=24))
        else:
            prev = spots[i - 1]
            keys.append(key(t - first, "", yaw - 3.0, 26, 4600, 30, (prev[0], prev[1], 0), (0, 0, 400),
                            vpitch=32, vdist=5200, vfocal=24))
            keys.append(key(t - first + NOMADIC_PAN, "", yaw, 26, 4600, 30, (spot[0], spot[1], 0), (0, 0, 400),
                            vpitch=32, vdist=5200, vfocal=24))
    last = spots[len(NOMADIC_SEQUENCE) - 1]
    keys.append(key(end - first, "", 215 + 3.0 * len(NOMADIC_SEQUENCE), 26, 4600, 30, (last[0], last[1], 0), (0, 0, 400),
                    vpitch=32, vdist=5200, vfocal=24))
    sequence = shot("OneByOne", first, end, "Orbit", keys, ease="Linear")
    job["Shots"] = [
        crane("Arrival", 0, first, "", 210, 34, 30, 5200, 4800, 30, 30, off=(spots[0][0], spots[0][1], 0),
              look=(0, 0, 400), vpitch=(38, 34), vdist=(5800, 5400), vfocal=(24, 24)),
        sequence,
        crane("Reveal", end, duration, "", 250, 30, 42, 7000, 9500, 28, 24, yaw1=310, off=(1000, 0, 0),
              look=(0, 0, 300), vpitch=(36, 48), vdist=(8000, 11000), vfocal=(22, 20)),
    ]
    return job


# --- Berlin showcase: 8 more POIs (positions from MapDesigner/Berlin/berlin_01.layout.json, +X north, +Y east) ----

def hide_events(anchor, anchor_yaw, hide):
    """Runtime-only Hide events for actors given by world (x, y): (prefix, (x, y), radius) -> anchor-frame offsets."""
    a = math.radians(-anchor_yaw)
    events = []
    for prefix, (wx, wy), radius in hide:
        dx, dy = wx - anchor[0], wy - anchor[1]
        events.append(ev(-3.5, "Hide", Text=prefix, Offset=(dx * math.cos(a) - dy * math.sin(a),
                                                            dx * math.sin(a) + dy * math.cos(a)), Radius=float(radius)))
    return events


def map_flight(name, map_key, anchor, anchor_yaw, sky, keys_spec, duration=22.0, clear=8000.0, occlusion=True,
               hide=(), **extra):
    """One continuous camera flight (no cuts) through anchor-frame keys:
    keys_spec = ((t, (x, y), yaw, pitch, dist, focal, look), ...), look = z or (x, y, z) in the anchor frame. An
    S-curve over the whole flight eases it in and out; Catmull-Rom between the keys keeps it smooth.
    occlusion=False for long flights: a sideways occlusion swing at 100 m+ throws the camera tens of metres."""
    job = base_job(name, "POI", map_key, anchor, anchor_yaw, duration, SkyMaterial=SKY[sky], ClearUnitsRadius=clear,
                   **extra)
    job["Events"] += hide_events(anchor, anchor_yaw, hide)
    keys = [key(t, "", yaw, pitch, dist, focal, tuple(xy) + (0,) * (3 - len(xy)),
                look if isinstance(look, tuple) else (0, 0, look), vpitch=pitch + 6, vdist=dist * 1.08,
                vfocal=focal * 0.8)
            for t, xy, yaw, pitch, dist, focal, look in keys_spec]
    job["Shots"] = [shot("Flight", 0, duration, "Orbit", keys, occlusion=occlusion)]
    return job


def berlin_flight(name, anchor, anchor_yaw, sky, keys_spec, duration=22.0, clear=8000.0, **extra):
    return map_flight(name, "Berlin", anchor, anchor_yaw, sky, keys_spec, duration, clear, **extra)


def _lerp(a, b, f):
    if isinstance(a, tuple):
        return tuple(x + (y - x) * f for x, y in zip(a, b))
    return a + (b - a) * f


def camera_keys(anchor, keys, subdivide=3.0):
    """Flight keys from camera positions (anchor yaw 0, anchor = (x, y, its ground z)): (t, (cam x, cam y), cam z,
    facing yaw, pitch down, focal, ground z where the view lands) -> map_flight keys pivoting on that ground point."""
    # Unwrap the facing so each turn takes the short way, then subdivide every leg: the rig interpolates pivot,
    # bearing and distance, so a big turn between far-apart keys would swing the camera off the checked line.
    unwrapped, prev = [], None
    for k in keys:
        f = k[3]
        if prev is not None:
            f = prev + ((f - prev + 180.0) % 360.0 - 180.0)
        prev = f
        unwrapped.append(k[:3] + (f,) + k[4:])
    dense = []
    for a, b in zip(unwrapped, unwrapped[1:]):
        steps = max(1, round((b[0] - a[0]) / subdivide)) if subdivide else 1
        for i in range(steps):
            f = i / steps
            dense.append(tuple(_lerp(va, vb, f) for va, vb in zip(a, b)))
    dense.append(unwrapped[-1])
    out = []
    for t, (cx, cy), cz, facing, pitch, focal, ground in dense:
        h = cz - ground
        d = h / math.tan(math.radians(pitch))
        px, py = cx + d * math.cos(math.radians(facing)), cy + d * math.sin(math.radians(facing))
        out.append((t, (px - anchor[0], py - anchor[1], ground - anchor[2]), facing + 180.0, pitch, math.hypot(d, h),
                    focal, 0))
    return out


def batch_berlin_poi():
    return [
        # The nuclear crater by the map origin (on the editor's red X axis): low over the glass rim, up above the
        # bowl, then a wide circle with the blast zone around it.
        # Take 1 sat too close and steep (the glass floor filled the frame): low on the south rim looking across the
        # bowl toward the Gate ruin and the Forge camp, then up into a wide circle that shows the whole crater.
        berlin_flight("POI_Berlin_Crater", (-2167, -3000, 0), 0, "fire", (
            (0.0, (-6000, 1500), 200, 10, 2000, 24, (3000, 0, 300)),
            # Further back and higher than take 2 (its 8 s key sat over the bowl: green glass filled the frame).
            (8.0, (-3000, 0), 190, 30, 7500, 24, (2500, 0, 300)),
            (15.0, (0, 0), 235, 40, 9000, 24, -200),
            (22.0, (0, 0), 295, 34, 11000, 22, -200),
        )),
        # Down the central avenue from Alexanderplatz toward the crater at rooftop height, rising at the end.
        berlin_flight("POI_Berlin_CentralAvenue", (7583, 13333, 0), -119, "storm", (
            (0.0, (0, 0), 180, 18, 2600, 26, 300),
            (8.0, (4400, 0), 180, 18, 2600, 26, 300),
            (16.0, (8900, 0), 185, 22, 2800, 26, 300),
            (22.0, (10400, 0), 205, 42, 6500, 24, 300),
        )),
        # Along the contaminated Spree channel, over the half-collapsed Moltke bridge, between the quay walls.
        # Take 1 looked down at flat water: low along the channel looking ~30 m ahead, so the quays, the ruins and the
        # bridge come toward the camera; the channel bends toward +Y after the bridge (river points (16800,-16000),
        # (16500,-24000)).
        berlin_flight("POI_Berlin_SpreeMoltke", (19500, -10500, 0), -135, "storm", (
            (0.0, (-2500, 0), 180, 9, 2000, 26, (3000, 0, 250)),
            (9.0, (1800, 0), 180, 11, 2200, 26, (3000, 0, 250)),
            (16.0, (5800, 1980), 212, 15, 2600, 24, (2500, 1700, 250)),
            (22.0, (7265, 3341), 228, 32, 5500, 24, (0, 0, 0)),
        )),
        # The 9:16 Scale camera stood behind a ruin wall on the -Y side: film the patrol from the sinkhole side.
        poi("POI_Berlin_PotsdamerPlatz", "Berlin", (-11375, -11111, 0), 0, "storm", 2600, look_height=500,
            scale=pan("Scale", 9, 15, "tag:Patrol", "", 90, pitch=16, dist=3000, focal=24, off1=(0, 0, 500),
                      look=(0, 0, 150), vpitch=20, vfocal=18)),
        poi("POI_Berlin_Hauptbahnhof", "Berlin", (22400, -15500, 0), 0, "storm", 3800, look_height=700, yaw0=260,
            reveal_yaw=140),
        # Take 1 used the low hero angle: mostly sky with the tower small; a normal height, closer.
        poi("POI_Berlin_ZooTower", "Berlin", (-14625, -43889, 0), 0, "fire", 2800, look_height=1000),
        # Take 1's Scale camera stood behind a ruin wall on the -Y side (black frames): film from the yard side.
        # 2026-11-17 map update: the default route grazes a walled foundation (local X -900..300, Y -3900..-3000)
        # and half the squad lagged ~30 m behind, so the Scale shot aimed at empty ground; 9 m to +Y is open.
        poi("POI_Berlin_RailYard", "Berlin", (-3792, 13389, 0), 0, "storm", 2400, look_height=500,
            patrol=("Scavengers", (-1.5, -0.925), (1.5, -0.625)),
            scale=pan("Scale", 9, 15, "tag:Patrol", "", 90, pitch=16, dist=3000, focal=24, off1=(0, 0, 500),
                      look=(0, 0, 150), vpitch=20, vfocal=18)),
        # (Anhalter Bf has no set piece on the map, only empty lots: scouted 2026-11-17.)
        poi("POI_Berlin_Alexanderplatz", "Berlin", (11194, 26528, 0), -90, "storm", 2500, look_height=600),
    ]


# --- Other maps: showcase POIs + one fly-through per map (2026-11-17). Landmarks picked from headless actor dumps of
# the saved maps + the map docs; yaw0 = bearing landmark -> camera in the anchor frame, the orbit turns -45 deg.

def batch_maps_poi():
    return [
        # Ore Mountains (2026-11-17: the Ore landscape renders as the default checker material in game since the new
        # M_RTS_Landscape_OreMountains: Nanite base pass "too many SRVs: 66 (only 64)"; re-render Ore after a fix).
        # Base A Wismut yard (1200, 24000) yaw 90 R 2500, look 500, yaw0 -60, reveal_yaw -60, patrol (-1.5,-1.55) ->
        # (1.5,-1.45), ClearUnitsRadius 12000: taken out because its checker ground was the landscape failure above;
        # not evaluable until Ore renders, re-test it with the other Ore POIs.
        # Adit 1 "STROSSE 2" at the foot of the quarried crest face, mine railway + carts on the 18 m bench.
        poi("POI_Ore_Strosse2Adit", "Ore", (-21700, 2850, 0), 143, "dusk", 1500, look_height=800, low=True, yaw0=-98,
            reveal_yaw=-103, ClearUnitsRadius=9000.0),
        # North foothills over Base A, the radixite field and the checkpoint to the broken A4 viaduct, heading west.
        map_flight("POI_Ore_ValleyFlight", "Ore", (0, 0, 0), 0, "dusk", camera_keys((0, 0, 0), (
            (0.0, (7000, 26000), 2700, 195, 20, 24, 0),
            (9.8, (-1500, 15500), 2000, 245, 15, 24, 0),
            (16.5, (1500, 7000), 2200, 235, 18, 24, 0),
            (22.0, (2500, -500), 2000, 235, 20, 24, 0))), occlusion=False),
        # Oder-Warthe W4: the Marine-Peilstand tower on its granite knoll inside the dragon's-teeth ring. The anchor
        # drops onto the tower roof (~z 3570, knoll 1474): ground_z brings the pivot down to the knoll.
        poi("POI_OderWarthe_Werk4Peilturm", "OderWarthe", (19072, 15299, 0), 180, "storm", 3500, look_height=1050,
            low=True, yaw0=280, ground_z=2100.0, patrol=("Scavengers", (-1.3, -1.7), (1.0, -1.65))),
        # Rail yard metal_c on the Lubowsee: ramp, wagons, scrap tower, double track; W4 tower behind.
        poi("POI_OderWarthe_RailYard", "OderWarthe", (5300, 15300, 0), 135, "storm", 2500, look_height=900, yaw0=60,
            reveal_yaw=-20, patrol=("Scavengers", (-1.4, -1.3), (1.4, -1.0)),
            hide=(("BP_OW_DeadPine", (8681, 17662), 200), ("BP_OW_DeadPine", (7598, 18648), 200))),
        # Along the double track from the east wood over the crossovers and the Lubow dam head to the W4 tower.
        map_flight("POI_OderWarthe_RailFlight", "OderWarthe", (5300, 15300, 0), 0, "storm", (
            (0.0, (3288 - 5300, 24023 - 15300), 103, 14, 10024, 24, 0),
            (7.0, (6261 - 5300, 19501 - 15300), 112, 15, 8597, 24, 0),
            (14.0, (8998 - 5300, 14541 - 15300), 122, 15, 8983, 24, 0),
            (22.0, (19072 - 5300, 15299 - 15300), 178.6, 8.9, 20829, 22, (0, 0, 330))), occlusion=False),
        # Pripyat: the leaning 27 m radio mast on the bench edge with the signals house; patrol up the east-road ramp.
        poi("POI_Pripyat_SignalsMast", "Pripyat", (-6050, 16550, 0), 220, "storm", 3000, look_height=1300, low=True,
            yaw0=300, reveal_yaw=250, patrol=("Scavengers", (-0.75, -0.65), (0.75, -0.5))),
        # Khutor kill zone: six tank wrecks, log houses, stork nest, downed Ju 87 on the east-lane sand ridge.
        poi("POI_Pripyat_KhutorKillZone", "Pripyat", (1600, 24300, 0), 270, "dusk", 2400, look_height=250, yaw0=240,
            reveal_yaw=290, patrol=("Scavengers", (0.25, -0.8), (1.5, -0.8))),
        # Drowned forest past the sunken and Demeter pods, along the plank boardwalk to the start village and memorial.
        map_flight("POI_Pripyat_BogFlight", "Pripyat", (-17000, -29000, 0), 0, "storm", camera_keys(
            (-17000, -29000, 200), (
                (0.0, (-23200, -36500), 1800, 100, 15, 24, 0),
                (7.0, (-22600, -29000), 2000, 115, 22, 24, 50),
                (15.0, (-17500, -27400), 2600, 345, 15, 24, 400),
                (22.0, (-12500, -28500), 4500, 45, 15, 22, 400))), occlusion=False, clear=15000.0),
        # Kessel gate 304: water tower, two silos, pipes. Anchor moved 5 m off the tower (a top-down drop at the pad
        # centre lands on the tank). Sun travels to -Y: cameras on the +Y side looking -Y are front-lit.
        poi("POI_Kessel_Waterworks", "Kessel", (14400, -14128, 0), 180, "dusk", 2200, look_height=550, yaw0=270,
            reveal_yaw=290),
        # Gate 238: the overrun Soviet trench line (MG nest, dugout, wire, hedgehogs, knocked-out Pz III).
        poi("POI_Kessel_TrenchLine", "Kessel", (-11900, -12300, 0), 153, "dusk", 2500, look_height=250, yaw0=210,
            patrol=("Scavengers", (-1.5, -1.75), (1.5, -1.65))),
        # Down the gate 304 ramp, past the waterworks, turning back onto the yard (take 1 opened on 6 s of the empty
        # base plateau: start on the ramp instead and glide slower).
        map_flight("POI_Kessel_GateFlight", "Kessel", (13000, -11500, 0), 0, "dusk", camera_keys((13000, -11500, 520), (
            (0.0, (7600, -11000), 1650, 325, 8, 24, 520),
            (9.0, (11200, -12700), 1800, 330, 12, 24, 520),
            (22.0, (17800, -12200), 2300, 207, 16, 24, 520))), occlusion=False),
        # Desert lone butte (2500, -26700) dropped 2026-11-17: its top is a flat plate, and with the pivot on the floor
        # the look target sits inside the butte, so occlusion avoidance lifts every orbit to a top-down view. The butte
        # flight shows it from low instead.
        # Red Mesa north face with the cliff road climbing to the rim.
        poi("POI_Desert_RedMesaCliffRoad", "Desert", (25300, -23500, 0), 90, "haze", 2500, look_height=700, yaw0=270,
            reveal_yaw=300, pitch_add=-6, **DESERT_ARGS),
        # A sweep round the butte: silhouette first, then the sunlit east face, ending on a pull-back over the canyon.
        map_flight("POI_Desert_ButteFlight", "Desert", (-1140, -22500, 0), 0, "haze", camera_keys((-1140, -22500, 600), (
            (0.0, (-8500, -35000), 2200, 37, 4, 24, 600),
            (7.0, (-2500, -24000), 2000, 332, 7, 24, 600),
            (14.5, (1000, -20500), 2400, 279, 10, 24, 600),
            (22.0, (-1500, -17000), 3200, 292, 10, 22, 600))), occlusion=False, **DESERT_ARGS),
    ]


def batch_custom():
    return [reichstag_battle(), nomadic_sequence()]


BATCHES = {
    "base": batch_base,
    "vehicle": batch_vehicle_battles,
    "mixed": batch_mixed,
    "air": batch_air,
    "trench_supply": batch_trench_supply,
    "poi": batch_poi,
    "custom": batch_custom,
    "berlin_poi": batch_berlin_poi,
    "maps_poi": batch_maps_poi,
}


def write_jobs(selected):
    JOBS_DIR.mkdir(parents=True, exist_ok=True)
    written = []
    for batch_name, factory in BATCHES.items():
        if selected and batch_name not in selected:
            continue
        for job in factory():
            job["Batch"] = batch_name
            for shot_spec in job["Shots"]:
                length = shot_spec["End"] - shot_spec["Start"]
                if length < 4.0 or length > 9.5:
                    print(f"  note: {job['JobName']}/{shot_spec['Name']} is {length:.1f} s (house range 4-9 s)")
            path = JOBS_DIR / f"{job['JobName']}.json"
            path.write_text(json.dumps(job, indent=1), encoding="utf-8")
            written.append(path)
    return written


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--batch", action="append", help="only these batches (repeatable)")
    parser.add_argument("--list", action="store_true")
    args = parser.parse_args()
    if args.list:
        for batch_name, factory in BATCHES.items():
            print(batch_name, [j["JobName"] for j in factory()])
        return 0
    for path in write_jobs(args.batch):
        print("wrote", path.name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
