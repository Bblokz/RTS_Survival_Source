"""Briefing panel texts of the story mission pieces, keyed by piece name.

Taken from the mission design docs in Source/Docs/Campaign (2026-11-25). Late reveals are left out on purpose:
the Berlin cases (Ore Mountains), the hidden island base (Pripyat), Starik (Oder-Warthe), the living vein (Lake).
The title is not set here: the piece takes its mission name from the map content settings.
"""


def _styled(text):
    return "<Text_Armor>" + text + "</>"


TEXTS = {
    "OreMountains": {
        "DescriptionText": _styled(
            "An intact ridge in the Ore Mountains on the Czech border, above a valley road the Soviets use for "
            "supply convoys. Build up a base on the ridge and raid the convoys passing below."),
        "PrimaryObjectiveText": _styled("Destroy Base B."),
        "SecondaryObjectiveText": _styled(
            "Raid the convoy yard, silence the artillery battery, take the radio outpost."),
    },
    "Berlin": {
        "DescriptionText": _styled(
            "The ruins of Berlin, destroyed in 1945 and patrolled by Soviets, with Shield holding the Reichstag. "
            "Set up a base in the Friedrichshain ruins and move west quietly to the Zoo Leitturm, where a Forge "
            "relay holds survey data."),
        "PrimaryObjectiveText": _styled("Hold the relay bunker at the Zoo Leitturm until the data is out."),
        "SecondaryObjectiveText": _styled("Destroy the Forge survey station beside the Brandenburg Gate."),
    },
    "OderWarthe": {
        "DescriptionText": _styled(
            "An old German bunker line across the lakes of western Poland. A Shield jammer has cut off the "
            "resistance cells; build a base and capture direction-finder stations to locate the hidden Shield "
            "command post."),
        "PrimaryObjectiveText": _styled("Find and destroy the Shield command post."),
        "SecondaryObjectiveText": _styled("Recover the signals archive from the command bunker."),
    },
    "Pripyat": {
        "DescriptionText": _styled(
            "The Pripyat marshes in southern Belarus: bog crossed only by causeways, with a Soviet base and a "
            "ruined airfield on the central ridge. Break the four Soviet AA batteries, take the base and repair "
            "its airstrip."),
        "PrimaryObjectiveText": _styled("Seize the central Soviet base and hold it against armored columns."),
        "SecondaryObjectiveText": _styled("Rescue downed pilots for bonus aircraft."),
    },
    "FrozenLake": {
        "DescriptionText": _styled(
            "A bay of Lake Vygozero in Karelia that the Forge keeps frozen with cryo pumps to mine the Radixite "
            "vein under it. Land on the shore, cross ice that will not hold heavy tanks, and take the Forge "
            "drill rig."),
        "PrimaryObjectiveText": _styled("Haul a Radixite core home from the drill rig, then destroy the Shield HQ."),
        "SecondaryObjectiveText": _styled("Destroy the four cryo pumps."),
    },
}
