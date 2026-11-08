"""Colour coding for the instrument-placard unit icons: the candidate enamel palette and which colour each unit icon
(Set08_FlatStencil) gets. Shared by placard_colour_matrix.py (review sheet) and the portrait engraver.

Scheme (approved by the user 2026-11-07):
  - class icons (left corner, the chassis) use the three German camouflage colours, light -> heavy getting darker:
    sand = light / armoured car, olive = medium, Panzergrau = heavy; super heavy gets a dark blue of its own
  - role icons (right corner, what it fights) use signal colours:
    oxide red = anti-armour, amber = rapid fire / anti-infantry, sky blue = anti-air, plum = indirect / explosive,
    hazard yellow = flame and radiation, teal = support, cream = elite / precision
"""

# Muted period enamels (RGB).
PALETTE = {
    "sand": (166, 142, 96),
    "olive": (92, 104, 60),
    "panzergrau": (104, 112, 120),
    "oxide red": (150, 52, 40),
    "amber": (176, 124, 48),
    "sky blue": (80, 128, 160),
    "dark blue": (40, 60, 104),
    "plum": (118, 70, 108),
    "hazard yellow": (192, 170, 54),
    "teal": (60, 116, 104),
    "cream": (196, 190, 170),
}

# (group, [(icon stem in Set08_FlatStencil, recommended colour, short reason)])
GROUPS = [
    ("Vehicle class (left corner)", [
        ("T_LightTankClass_Icon_V2", "sand", "light chassis; Dunkelgelb"),
        ("T_ArmoredCar_Icon", "sand", "light, wheeled"),
        ("T_MediumTankClass_Icon_V2", "olive", "medium chassis"),
        ("T_HeavyTankClass_Icon_V2", "panzergrau", "heavy chassis"),
        ("T_SuperHeavyTankClass_Icon", "dark blue", "super heavy; user's pick 2026-11-07"),
        ("T_TankDestroyerClass_Icon", "oxide red", "tank hunter = anti-armour"),
        ("T_ArtilleryClass_Icon_V3", "plum", "indirect fire"),
    ]),
    ("Vehicle / weapon role (right corner)", [
        ("T_AutoCannon_Class_V2", "amber", "rapid fire, belt fed"),
        ("T_MGClass_Icon_V2", "amber", "rapid fire, anti-infantry"),
        ("T_AntiAirClass_Icon_V3_NoStar", "sky blue", "anti-air"),
        ("T_RocketWeaponClass_Icon_V2", "plum", "explosive salvo"),
        ("T_RocketAbiltyClass_ICon_V2", "plum", "explosive, targeted"),
        ("T_RailgunClass_Icon_V2", "oxide red", "armour piercing"),
        ("T_LaserClass_Icon", "oxide red", "armour piercing beam"),
        ("T_FlameClass_Icon", "hazard yellow", "fire hazard"),
        ("T_FlameClass_Icon_TankGun", "hazard yellow", "fire hazard (vehicle)"),
        ("T_RadiationClass_Icon", "hazard yellow", "radiation hazard"),
    ]),
    ("Infantry role", [
        ("T_AssaultClass_Icon_V3", "amber", "close-range anti-infantry"),
        ("T_ATInfantryClass_Icon_V2", "oxide red", "anti-armour"),
        ("T_CommandoClass_Icon_V3", "cream", "elite"),
        ("T_SniperClass_Icon_V2", "cream", "precision"),
        ("T_EngineerClass_Icon_V2", "teal", "builds / repairs"),
    ]),
    ("Support", [
        ("T_RepairAuraClass_V2", "teal", "repair"),
        ("T_RangeProviderClass_ICon_V2", "teal", "spotting / vision"),
        ("T_ExtraIcon_V2", "teal", "medic / heal"),
        ("T_CargoGarrisionClass_Icon", "teal", "transport / garrison"),
    ]),
    ("Aircraft", [
        ("T_CASAircraftClass_Icon_V3_Clean", "amber", "strafing ground attack"),
        ("T_BombAircraftClass_Icon_V2", "plum", "bombs, explosive"),
    ]),
]


def recommended_colour(icon_stem):
    for _, entries in GROUPS:
        for stem, colour, _ in entries:
            if stem == icon_stem:
                return PALETTE[colour]
    raise KeyError(icon_stem)
