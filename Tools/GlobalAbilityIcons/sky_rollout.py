"""Roll the picked sky plates out to the ability icons (2026-11-07, user picks from the sky plate study):
    aircraft strafes  -> Sky 03 (the RTS_IntroCinematic sky) with the in-engine dusk grade
    drops             -> Sky 06 night (cold)
    paratroopers      -> Sky 01 (grey storm); every icon that shows soldiers under canopies, whatever its name

Each icon keeps its own composition (round 1 / 3a / 3b / Mammut composers); only the sky changes. Within a group the
plate is cropped and mirrored per icon (stable per name) so a row of buttons does not repeat the same cloud.
The picked study cards keep the exact crop the user saw.

Usage: python sky_rollout.py [icon names]    writes Saved/GlobalAbilityIcons/out/T_GA_<Name>.png (backup first)
"""
import os
import shutil
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "GliderDelivery"))

import sky_plate_compose as spc  # noqa: E402  (patches compose_ability_icons in place)
import compose_ability_icons as cab  # noqa: E402
import round3a_compose_icons as r3a  # noqa: E402
import round3b_compose_icons as r3b  # noqa: E402
import compose_mammut_icon as mammut  # noqa: E402

OUT = cab.OUT
BACKUP = "D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityIcons/out_before_sky_2026-11-07/"

STRAFE = {"plate": "intro_i03_dusk_y090_p30", "size": 880, "pick": ("3xJu87Strafing", 72, False)}
DROP = {"plate": "intro_i06_night_y270_p30", "size": 880, "pick": ("GerFlakLuchsStugDrop", 0, False)}
PARA = {"plate": "intro_i01_y090_p08", "size": 800, "pick": ("GerParatroopers", 120, False)}

GROUPS = {
    "strafe": (STRAFE, {
        "3xJu87Strafing": cab.ICONS["3xJu87Strafing"],
        "2xHo229Strafing": cab.ICONS["2xHo229Strafing"],
        "Ju87Strafing": r3b.ICONS["Ju87Strafing"],
        "Bf109Strafing": r3b.ICONS["Bf109Strafing"],
        "2xBf110Strafing": r3b.ICONS["2xBf110Strafing"],
        "Me410Airstrike": r3b.ICONS["Me410Airstrike"],
        "GerJu87GTankBusters": r3b.ICONS["GerJu87GTankBusters"],
    }),
    "drop": (DROP, dict(
        [(n, cab.ICONS[n]) for n in ("SovietT34Drop", "SovietT34DropVariation", "GerPzIIIFlak41Drop", "GerSdkfz7FlakDrop",
                                     "GerRadixitePzIIMarderDrop", "GerFlakLuchsStugDrop", "GerSpotterPzIIIDrop")]
        + [(n, r3a.ICONS[n]) for n in ("GerE50Drop", "GerE75Drop", "GerMausDrop", "GerJagdTigerLaserDrop",
                                       "GerJagdPantherFlamerDrop", "GerSturmtigerDrop", "GerJagdTigerDrop",
                                       "GerKingTiger105Drop", "GerTigerRailDrop", "GerRadixitePzIIDrop",
                                       "GerRadixitePzIIIWespeDrop", "GerPzJagerLaserDrop", "GerPzIIIFlammDrop", "GerPumaDrop",
                                       "GerHarvesterDrop", "GerPzIHarvesterDrop", "GerPz38tRailgunDrop", "GerScavengersDrop",
                                       "GerFelsKriegerDrop", "GerRedSpineDrop", "GerRedSpinePairDrop", "GerRailTroopersDrop")]
        + [(n, r3b.ICONS[n]) for n in ("GerFlak88Drop", "GerSFH18Drop", "GerPanzerschreckPitDrop", "GerPakPitDrop",
                                       "GerFieldBunkerDrop", "GerFieldWorkshopDrop", "GerRadixiteMetalSupplyDrop",
                                       "GerConstructionSupplyDrop", "GerRadixiteCacheDrop", "GerVehiclePartsSupplyDrop")]
        + [("GerMammutE100Drop", mammut.mammut_icon)])),
    "para": (PARA, dict(
        [(n, r3a.ICONS[n]) for n in ("GerParatroopers", "GerEliteScavengersParadrop", "GerRailTroopersParadrop",
                                     "GerLightBringersParadrop", "GerFeuerSturmParadrop", "GerIronStormParadrop",
                                     "GerPzGrenIronStormParadrop", "GerMarderScavengerDrop", "GerEliteScavengersPakDrop",
                                     "GerSturmPionierenPakPitDrop")]
        + [("SovietParatroopers", cab.ICONS["SovietParatroopers"]), ("GerLuftlandeSturm", r3b.ICONS["GerLuftlandeSturm"])])),
}

# per-icon crop offset and mirror; the picked card keeps its exact crop
VARIANTS = [(0, False), (144, True), (72, True), (144, False), (0, True), (72, False)]


def look_for(name, spec, faction):
    pick_name, pick_x, pick_flip = spec["pick"]
    if name == pick_name:
        x, flip = pick_x, pick_flip
    else:
        x, flip = VARIANTS[zlib.crc32(name.encode()) % len(VARIANTS)]
    x = min(x, 1024 - spec["size"])
    return {"plate": spec["plate"], "crop": [x, 0, spec["size"]], "flip": flip, "treatment": "raw", "faction": faction}


def main():
    wanted = set(sys.argv[1:])
    os.makedirs(BACKUP, exist_ok=True)
    made = []
    for group, (spec, icons) in GROUPS.items():
        for name, fn in icons.items():
            if wanted and name not in wanted:
                continue
            path = OUT + "T_GA_%s.png" % name
            if os.path.exists(path) and not os.path.exists(BACKUP + os.path.basename(path)):
                shutil.copy2(path, BACKUP)
            spc.set_look(**look_for(name, spec, "sov" if name.startswith("Soviet") else "ger"))
            # the round-3a/3b composers copied the haze colour at import: keep their far-plane tint on the plate too
            r3a.SKY = r3b.SKY = cab.SKY_GER
            fn().save(path)
            made.append((group, name))
            print("ICON", group, name, flush=True)
    print("DONE", len(made))


if __name__ == "__main__":
    main()
