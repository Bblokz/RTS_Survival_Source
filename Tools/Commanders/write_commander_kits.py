"""Writes the commander kits into Config/DefaultGame.ini ([/Script/RTS_Survival.CommanderSettings]).

Plain python (no editor): python write_commander_kits.py [--check]. The kits table below is the round-3 design
(Docs/Abilities/Commanders.md, seven slots). DefaultGame.ini is shared with other sessions, so the script re-reads it
right before writing and replaces only the CommanderSettingsByType line of that section; it refuses to write when the
section is missing or has more than one such line. --check prints the line without writing.
"""
import os
import re
import sys

INI = os.path.join(os.path.dirname(__file__), "..", "..", "..", "Config", "DefaultGame.ini")
SECTION = "[/Script/RTS_Survival.CommanderSettings]"
PORTRAITS = "/Game/RTS_Survival/Textures/GameUI/Commanders/Portraits/"

# (ERTSCommander, portrait id, title, description, seven abilities in slot order).
# Slots: 1 intel (I), 2 small reinforcements (I), 3 economy (I), 4 reinforcements (II), 5 air strike (III),
# 6 signature (III or IV), 7 super-heavy (IV).
KITS = [
    ("BalancedCommander", "Balanced", "Field Commander",
     "A seasoned generalist: dependable support at every stage of a campaign, ending with a V2 strike and the E-50.",
     ["GA_GerSpiesInfiltration", "GA_GerMarderScavengerDrop", "GA_HarvesterDrop", "GA_GerParatroopers",
      "GA_3xJu87Strafing", "GA_GerV2Rocket", "GA_GerE50Drop"]),
    ("LuftwaffeLiaison", "AirSupport", "Luftwaffe Liaison",
     "Commands from the air: fighters, paratroopers, dive bombers and jet strikes, ending with a super-heavy airlift.",
     ["GA_GerReconFlight", "GA_Bf109Strafing", "GA_HarvesterDrop", "GA_GerParatroopers", "GA_GerLuftlandeSturm",
      "GA_2xHortentrafing", "GA_GerE100Drop"]),
    ("BarrageMarshal", "Artillery", "Barrage Marshal",
     "Breaks fortified lines with flares, air-dropped guns and massed artillery, then brings up the Sturmtiger.",
     ["GA_GerFlareCurtain", "GA_GerFlak88Drop", "GA_GerRadixiteMetalSupplyDrop", "GA_GerSFH18Drop",
      "GA_Ju87Strafing", "GA_LargeBarrage", "GA_GerSturmtigerDrop"]),
    ("RadixiteHerald", "Radixite", "Radixite Herald",
     "Weak in the middle of a campaign, unmatched at its end: radixite weapons and the radixite V-weapon.",
     ["GA_GerReconFlight", "GA_GerRadixitePzIIDrop", "GA_GerRadixiteCacheDrop", "GA_GerRadixitePzIIIWespeDrop",
      "GA_2xHortentrafing", "GA_GerRadixiteNuke", "GA_GerE100Drop"]),
    ("ScrapWarlord", "ScrapWarlord", "Scrap Warlord",
     "Cheap, fast and early: elite scavengers, salvaged harvesters and air-dropped mines, strongest at the start.",
     ["GA_GerSpiesInfiltration", "GA_GerEliteScavengersParadrop", "GA_GerPzIHarvesterDrop",
      "GA_GerEliteScavengersPakDrop", "GA_Me410Airstrike", "GA_GerAirMinefield", "GA_GerJagdTigerDrop"]),
    ("RailEngineer", "RailEngineer", "Rail Engineer",
     "Long-range kinetic warfare: rail troopers, railgun tank destroyers, rail bombardment and the Tiger Rail.",
     ["GA_GerReconFlight", "GA_GerRailTroopersParadrop", "GA_GerRadixiteMetalSupplyDrop", "GA_GerPz38tRailgunDrop",
      "GA_2xBf110Strafing", "GA_GerRailBombardment", "GA_GerTigerRailDrop"]),
    ("LightBringer", "LightBringer", "Light Bringer",
     "Precise and energy-hungry: laser infantry and tank destroyers, the Sonnengewehr beam and the laser JagdTiger.",
     ["GA_GerFlareCurtain", "GA_GerLightBringersParadrop", "GA_HarvesterDrop", "GA_GerPzJagerLaserDrop",
      "GA_2xHortentrafing", "GA_GerSonnengewehr", "GA_GerJagdTigerLaserDrop"]),
    ("Feuersturm", "Feuersturm", "Feuersturm",
     "Burns out infantry and bunkers at short range: flame squads, flame tanks, incendiary strikes and fire barrages.",
     ["GA_GerReconFlight", "GA_GerFeuerSturmParadrop", "GA_HarvesterDrop", "GA_GerPzIIIFlammDrop",
      "GA_GerIncendiaryJu87", "GA_GerFireBarrage", "GA_GerJagdPantherFlamerDrop"]),
    # Slot 6 is a placeholder (GA_Barrage) until the Fels Krieger walker exists.
    ("Stahllaeufer", "Stahllaeufer", "Stahlläufer",
     "Walkers instead of tanks: Red Spine walker squads land like infantry, and the Maus closes the campaign.",
     ["GA_GerReconFlight", "GA_GerRedSpineDrop", "GA_GerRadixiteMetalSupplyDrop", "GA_GerRedSpinePairDrop",
      "GA_Me410Airstrike", "GA_Barrage", "GA_GerMausDrop"]),
    ("Festungsbaumeister", "Festungsbaumeister", "Festungsbaumeister",
     "The defensive commander: air-dropped pits, guns and bunkers hold any line; the Maus holds the last one.",
     ["GA_GerFlareCurtain", "GA_GerPanzerschreckPitDrop", "GA_GerConstructionSupplyDrop",
      "GA_GerSturmPionierenPakPitDrop", "GA_HeavyCarpet_Ju390B_250Gr", "GA_GerFieldBunkerDrop", "GA_GerMausDrop"]),
    # Slots 2 and 4 are placeholders until the user picks the STG squad: Elite Scavengers (STG 44) and the
    # Vultures + Panzergrenadiere paradrop.
    ("Sturmkommandant", "Sturmkommandant", "Sturmkommandant",
     "All-infantry pressure: wave after wave of paratroopers and dive bombers, weak against armour until the E-75.",
     ["GA_GerReconFlight", "GA_GerEliteScavengersParadrop", "GA_HarvesterDrop", "GA_GerParatroopers",
      "GA_3xJu87Strafing", "GA_GerV2Rocket", "GA_GerE75Drop"]),
    # Round 3d (2026-11-05). Slots 5 and 6 (Ju 87 G, field workshop) are built by their own agents
    # (Tools/Ju87G, Tools/FieldWorkshop); slots 1-4 by Tools/SkyDrop/author_sky_drop_abilities.py.
    ("SkyDropQuartermaster", "ArmorDrop", "Sky-Drop Quartermaster",
     "Everything arrives by air: landed armour, vehicle parts, a parachuted field workshop and the E-50 by glider.",
     ["GA_GerMassiveFlare", "GA_GerPumaDrop", "GA_GerVehiclePartsSupplyDrop", "GA_GerFlakLuchsStugDrop",
      "GA_GerJu87GTankBusters", "GA_GerFieldWorkshopDrop", "GA_GerE50Drop"]),
]
SLOTS = 7


def texture(portrait_id, suffix=""):
    name = "T_Commander_%s%s" % (portrait_id, suffix)
    return '"%s%s.%s"' % (PORTRAITS, name, name)


def kit_entry(commander, portrait_id, title, description, abilities):
    if len(abilities) != SLOTS or len(set(abilities)) != SLOTS:
        raise ValueError("%s: needs %d different abilities, has %s" % (commander, SLOTS, abilities))
    if '"' in title or '"' in description:
        raise ValueError("%s: quotes in the text" % commander)
    return ('(%s, (GlobalAbilities=(%s),Title=NSLOCTEXT("RTSCommanders", "%s_Title", "%s"),'
            'Description=NSLOCTEXT("RTSCommanders", "%s_Desc", "%s"),Portrait=%s,PortraitCharacter=%s,'
            'PortraitBackground=%s))') % (
        commander, ",".join(abilities), commander, title, commander, description, texture(portrait_id),
        texture(portrait_id, "_Character"), texture(portrait_id, "_Background"))


def build_line():
    return "CommanderSettingsByType=(%s)" % ",".join(kit_entry(*kit) for kit in KITS)


def portraits_exist():
    content = os.path.join(os.path.dirname(INI), "..", "Content", "RTS_Survival", "Textures", "GameUI", "Commanders",
                           "Portraits")
    missing = []
    for kit in KITS:
        for suffix in ("", "_Character", "_Background"):
            path = os.path.join(content, "T_Commander_%s%s.uasset" % (kit[1], suffix))
            if not os.path.exists(path):
                missing.append(path)
    return missing


def write(line):
    with open(INI, "rb") as handle:
        raw = handle.read()
    text = raw.decode("utf-8-sig")
    newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(newline)
    section_index = lines.index(SECTION)
    end = section_index + 1
    while end < len(lines) and not lines[end].startswith("["):
        end += 1
    kit_lines = [i for i in range(section_index + 1, end) if lines[i].startswith("CommanderSettingsByType=")]
    if len(kit_lines) != 1:
        raise RuntimeError("expected one CommanderSettingsByType line in %s, found %d" % (SECTION, len(kit_lines)))
    lines[kit_lines[0]] = line
    out = newline.join(lines).encode("utf-8")
    if raw.startswith(b"\xef\xbb\xbf"):
        out = b"\xef\xbb\xbf" + out
    with open(INI, "wb") as handle:
        handle.write(out)


def main():
    missing = portraits_exist()
    if missing:
        raise RuntimeError("missing portraits: %s" % missing)
    line = build_line()
    if "--check" in sys.argv:
        print(line)
        return
    write(line)
    print("wrote %d kits (%d characters) to %s" % (len(KITS), len(line), os.path.abspath(INI)))


if __name__ == "__main__":
    main()
