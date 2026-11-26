"""Shared list of the Soviet tanks that get a world map division piece.

name: suffix of SM_DivisionTank_<name>. vehicle: the game vehicle Blueprint. wrecks: Env_Dest_* Blueprints of that tank
(the first one that yields meshes is used).
"""
V = "/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/"
DEST = "/Game/RTS_Survival/WorldCampaign/Divisions/TankPieces"

TANKS = [
    {"name": "T34_76", "vehicle": V + "Rus_Medium/T34/Rus_Vehicle_T3476_Soviet",
     "wrecks": [V + "Rus_Medium/T34/Env_Dest_T34_76"]},
    {"name": "T34_85", "vehicle": V + "Rus_Medium/T34/Rus_Vehicle_T3485_Soviet",
     "wrecks": [V + "Rus_Medium/T34/Env_Dest_T34_85_NoFlame", V + "Rus_Medium/T34/Env_Dest_T34_85"]},
    {"name": "T26", "vehicle": V + "Rus_Light/T26/Rus_Vehicle_T26_Soviet",
     "wrecks": [V + "Rus_Light/T26/Env_Dest_T26"]},
    {"name": "BT7", "vehicle": V + "Rus_Light/BT-7/Rus_Vehicle_BT-7_Soviet",
     "wrecks": [V + "Rus_Light/BT-7/Env_Dest_Bt-7"]},
    {"name": "IS1", "vehicle": V + "Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-1_Soviet",
     "wrecks": [V + "Rus_Heavy/IS-1And-2/Env_Dest_IS-1_NoFlame", V + "Rus_Heavy/IS-1And-2/Env_Dest_IS-1"]},
    {"name": "IS2", "vehicle": V + "Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-2_Soviet",
     "wrecks": [V + "Rus_Heavy/IS-1And-2/Env_Dest_IS-2_NoFlame", V + "Rus_Heavy/IS-1And-2/Env_Dest_IS-2"]},
    {"name": "IS3", "vehicle": V + "Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-3_Soviet",
     "wrecks": [V + "Rus_Heavy/IS-1And-2/Env_Dest_IS-3_NoFlame", V + "Rus_Heavy/IS-1And-2/Env_Dest_IS-3"]},
    {"name": "KV1", "vehicle": V + "Rus_Heavy/KV1/Rus_Vehicle_KV-1_L11_Soviet",
     "wrecks": [V + "Rus_Heavy/KV1/Env_Dest_KV_1L11"]},
]


def selected(filter_text):
    """filter_text: comma separated names (empty = all)."""
    wanted = [entry.strip() for entry in (filter_text or "").split(",") if entry.strip()]
    return [tank for tank in TANKS if not wanted or tank["name"] in wanted]
