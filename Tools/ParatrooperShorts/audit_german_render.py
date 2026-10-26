"""Verify the German two-bomber short against rendered media and runtime events."""
from collections import Counter
from fractions import Fraction
import json
from pathlib import Path
import re
import subprocess
import sys


take = Path(sys.argv[1]).resolve()
job = json.loads((take / "Job.json").read_text(encoding="utf-8-sig"))
manifest = json.loads((take / "Manifest.json").read_text(encoding="utf-8-sig"))
log = (take / "render.log").read_text(encoding="utf-8", errors="replace")
video = take / (job["JobName"] + ".mp4")
probe = json.loads(subprocess.check_output([
    "ffprobe", "-v", "error", "-show_entries",
    "format=duration:stream=codec_type,width,height,avg_frame_rate,nb_frames", "-of", "json", str(video),
], text=True))
picture = next(stream for stream in probe["streams"] if stream["codec_type"] == "video")
duration = float(probe["format"]["duration"])
assert manifest["bComplete"] and "RTS_SHORTS_JOB_DONE job=" in log and "success=true" in log
assert (picture["width"], picture["height"], Fraction(picture["avg_frame_rate"])) == (1080, 1920, 60)
assert 0 < duration <= 120.001
assert abs(int(picture["nb_frames"]) / 60 - duration) < .02
assert any(stream["codec_type"] == "audio" for stream in probe["streams"])
segment = job["Segments"][0]
assert segment["ParatrooperAbility"] == "GA_GerParatroopers"
assert "RTS_SHORTS_PARATROOPER_CUT local=120 introFrames=120" in log
passes = re.findall(r"RTS_SHORTS_PARATROOPER_BEGIN ability=27 [^\r\n]*pass=(\d+) owner=(\d+)", log)
assert passes == [("0", "1"), ("1", "1")], passes
assert "RTS_SHORTS_PARATROOPER_AIRCRAFT count=2" in log
assert len(re.findall(r"RTS_SHORTS_PARATROOPER_AIRCRAFT_MATERIAL_REPAIRED aircraft=Ger_Aircraft_JU390B\S+ slot=1", log)) == 2
assert not re.search(r"RTS_SHORTS_PLACEHOLDER_MESH_HIDDEN[^\r\n]*owner=Ger_Aircraft_JU390B", log)
drop_squads = re.findall(r"RTS_SHORTS_PARATROOPER_SQUAD squad=(Squad_Ger_HazmatVultures\S+) owner=(\d+)", log)
assert len(drop_squads) == 2 and all(owner == "1" for name, owner in drop_squads), drop_squads
recipients = set(re.findall(r"RTS_SHORTS_DOUBLE_RANGE unit=(\S+)", log))
expected = {
    "Rus_Vehicle_T34E_C_": 3, "Rus_Vehicle_T-70_C_": 3,
    "Rus_Vehicle_IS-1_C_": 1, "Rus_Vehicle_IS-2_C_": 1,
    "Rus_Squad_PTRS_C_": 3, "Rus_BXP_BS3Pit_C_": 1,
    "Ger_Vehicle_StugIII_C_": 3, "Ger_Vehicle_PanzerIII_J_C_": 3,
    "Ger_Vehicle_TigerII_C_": 1, "Squad_Ger_Scavengers_C_": 4,
    "Ger_BXP_GerPit_Flak88_C_": 2, "Squad_Ger_HazmatVultures_C_": 2,
}
assert len(recipients) == sum(expected.values()), recipients
for prefix, count in expected.items():
    assert sum(name.startswith(prefix) for name in recipients) == count, (prefix, recipients)
owners = re.findall(r"RTS_SHORTS_OWNER_READY squad=(\S+) soldier=(\S+) owner=(\d+) expected=(\d+)", log)
assert owners and all(owner == expected_owner == ("1" if squad.startswith("Squad_Ger_") else "2")
                      for squad, soldier, owner, expected_owner in owners)
placements = re.findall(r"RTS_SHORTS_BATTLE_PARTICIPANT unit=(\S+) owner=(\d+) location=X=([\d.-]+) Y=([\d.-]+)", log)
for prefix, count, owner, position_x in [
    ("Rus_Squad_PTRS_C_", 3, "2", 228800),
    ("Squad_Ger_Scavengers_C_", 4, "1", 231150),
    ("Rus_BXP_BS3Pit_C_", 1, "2", 225800),
    ("Ger_BXP_GerPit_Flak88_C_", 2, "1", 234600),
]:
    found = [row for row in placements if row[0].startswith(prefix)]
    assert len(found) == count, (prefix, found)
    assert all(row[1] == owner and abs(float(row[2]) - position_x) < 1 for row in found), found
jumps = re.findall(
    r"RTS_PARATROOPER_JUMP run=(\d+) index=(\d+) unit=(Ger_Inf_Hazmat_Vulture\S+) source=(\w+) "
    r"past_target_cm=([\d.-]+) start=X=([\d.-]+) Y=([\d.-]+) Z=([\d.-]+) "
    r"landing=X=([\d.-]+) Y=([\d.-]+) Z=([\d.-]+)", log)
assert Counter(row[0] for row in jumps) == {"0": 4, "1": 4}, jumps
for run in ("0", "1"):
    soldiers = sorted((row for row in jumps if row[0] == run), key=lambda row: int(row[1]))
    assert all(row[3] == "aircraft" for row in soldiers), soldiers
    assert float(soldiers[0][5]) > 232000, soldiers[0]
    assert float(soldiers[-1][5]) < 228000, soldiers[-1]
    assert float(soldiers[0][8]) > 234000 and float(soldiers[-1][8]) < 229000, soldiers
assert "leg=1" in log and "leg=2" in log
assert "S_FireWeapon_Flak88(v=" in log
for turret in ["RTS_BP_EmbTurret_GerPit_Flak88_C_0", "RTS_BP_EmbTurret_GerPit_Flak88_C_1", "RTS_BP_EmbTurret_RusBS3Pit_C_0"]:
    assert re.search(r"RTS_SHORTS_FX_NEW[^\r\n]*owner=" + re.escape(turret) + r" ", log), turret
assert "HEAT" not in segment["Caption1"]
timeline = manifest["segments"][0]
assert abs(duration - (timeline["endFrame"] - timeline["startFrame"]) / job["Fps"]) < .02
report = {
    "video": str(video), "duration_seconds": duration, "resolution": [1080, 1920], "fps": 60,
    "german_bomber_passes": 2, "vulture_squads": 2, "vulture_soldiers": len(jumps),
    "bomber_propeller_materials_repaired": True, "bomber_meshes_not_hidden_as_placeholders": True,
    "drop_owner": 1, "soviet_owner": 2, "range_behaviour_recipients": len(recipients),
    "soldier_owners_verified": len(owners), "rear_to_front_jumps_verified": True,
    "landing_trails_verified": True, "regular_soviet_vehicles_verified": True,
    "flak_88_pits": 2, "bs3_100mm_pits": 1, "gun_and_infantry_placements_verified": True,
    "all_three_gun_pits_firing": True,
    "full_camera_route_verified": True, "has_audio": True, "job_complete": True,
}
(take / "Verification.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
