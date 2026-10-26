"""Verify the completed preview against actual runtime events and delivered media."""
import json
import re
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

take=Path(sys.argv[1]).resolve()
manifest=json.loads((take/"Manifest.json").read_text(encoding="utf-8-sig"))
job=json.loads((take/"Job.json").read_text(encoding="utf-8-sig"))
log=(take/"render.log").read_text(encoding="utf-8",errors="replace")
video=take/(job["JobName"]+".mp4")
probe=json.loads(subprocess.check_output(["ffprobe","-v","error","-show_entries","format=duration:stream=codec_type,width,height,avg_frame_rate","-of","json",str(video)],text=True))

video_stream=next(stream for stream in probe["streams"] if stream["codec_type"]=="video")
assert (video_stream["width"],video_stream["height"])==(job["ResolutionX"],job["ResolutionY"])
assert Fraction(video_stream["avg_frame_rate"])==job["Fps"]
duration=float(probe["format"]["duration"])
assert 0<duration<=120.001,duration
assert manifest["bComplete"]
assert "RTS_SHORTS_JOB_DONE job=" in log and "success=true" in log
assert any(stream["codec_type"]=="audio" for stream in probe["streams"])
recipients=set(re.findall(r"RTS_SHORTS_DOUBLE_RANGE unit=(\S+)",log))
assert len(recipients)==29,recipients
assert 'Rus_Vehicle_T3485_Soviet' not in ' '.join(recipients)
assert sum(name.startswith('Rus_Vehicle_T3485_C_') for name in recipients)==4
assert sum(name.startswith('Ger_Vehicle_PanzerIV_G_C_') for name in recipients)==2
assert sum(name.startswith('Ger_Vehicle_PantherD_C_') for name in recipients)==1
assert sum(name.startswith('Rus_Vehicle_IS-1_Soviet_C_') for name in recipients)==1
for unit,owner,x,y in [('Rus_Vehicle_IS-1_Soviet_C_0',2,228000,8600),('Ger_Vehicle_PantherD_C_0',1,232000,9600)]:
    sample=re.search(r'RTS_SHORTS_BATTLE_PARTICIPANT unit='+re.escape(unit)+r' owner=(\d+) location=X=([\d.-]+) Y=([\d.-]+)',log)
    assert sample and int(sample[1])==owner and abs(float(sample[2])-x)<1 and abs(float(sample[3])-y)<1,(unit,sample)
for turret in ['Rus_Turret_IS-1_Soviet_C_CAT_0','Ger_Turret_PantherD_C_CAT_0']:
    assert re.search(r'RTS_SHORTS_FX_NEW [^\r\n]*asset=NS_TankCasingBase owner=\S*'+re.escape(turret)+r' ',log),turret
forge_graph=Path(__file__).with_name('ForgeIS1TankEvents.json').read_text(encoding='utf-8-sig')
assert 'BP_Beh_Permanent_SetHEATShells' in forge_graph
vulture_positions=re.findall(r"RTS_SHORTS_BATTLE_PARTICIPANT unit=(Squad_Ger_HazmatVultures\S*) owner=(\d+) location=X=([\d.-]+) Y=([\d.-]+) Z=([\d.-]+)",log)
assert len(vulture_positions)==6,vulture_positions
assert len({float(row[3]) for row in vulture_positions})==6
for name,owner,x,y,z in vulture_positions:
    assert owner=='1' and abs(float(x)-230950)<1,(name,owner,x)
    assert name in recipients,name
pit_positions=re.findall(r"RTS_SHORTS_BATTLE_PARTICIPANT unit=((?:Ger_BXP_GerPit_Flak37|Ger_BXP_GerPit_LeFH18|Rus_BXP_ML20Pit)\S*) owner=(\d+) location=X=([\d.-]+) Y=([\d.-]+) Z=([\d.-]+)",log)
assert len(pit_positions)==4,pit_positions
for name,owner,x,y,z in pit_positions:
    soviet=name.startswith("Rus_")
    assert int(owner)==(2 if soviet else 1),(name,owner)
    howitzer='LeFH18' in name
    assert abs(float(x)-(226200 if soviet else (235200 if howitzer else 233500)))<1,(name,x)
    assert abs(float(z)-144)<2,(name,z)
    assert abs(float(y)-(10000 if soviet or howitzer else (9000 if float(y)<10000 else 11000)))<1,(name,y)
    assert name in recipients,name
    assert re.search(r"RTS_SHORTS_RANGE_READY unit="+re.escape(name)+r" owner="+owner+r" weapon=\S+ range=[\d.]+",log),name
for turret in ["RTS_BP_EmbTurret_ML20Pit_C_0","RTS_BP_EmbTurret_GerPit_Flak37_C_0","RTS_BP_EmbTurret_GerPit_Flak37_C_1","RTS_BP_EmbTurret_GerPit_LeFH18_C_0"]:
    assert re.search(r"RTS_SHORTS_FX_NEW [^\r\n]*owner="+re.escape(turret)+r" ",log),turret
owners=re.findall(r"RTS_SHORTS_OWNER_READY squad=(\S+) soldier=(\S+) owner=(\d+) expected=(\d+)",log)
assert len(owners)>20,len(owners)
assert all(owner==expected==('1' if squad.startswith('Squad_Ger_') else '2') for squad,soldier,owner,expected in owners)
infantry_positions=re.findall(r"RTS_SHORTS_BATTLE_PARTICIPANT unit=((?:Rus_Squad_PTRS|Squad_Ger_PanzerGrenadiers)\S*) owner=(\d+) location=X=([\d.-]+) Y=([\d.-]+) Z=([\d.-]+)",log)
assert len(infantry_positions)==6,infantry_positions
for name,owner,x,y,z in infantry_positions:
    soviet=name.startswith('Rus_')
    assert int(owner)==(2 if soviet else 1),(name,owner)
    assert abs(float(x)-(228800 if soviet else 231200))<1,(name,x)
    assert name in recipients,name
assert all(name in log for name in ["Rus_Squad_Mosin_C_0","Rus_Squad_FlameROKS_C_0"])
cut=int(job["Fps"]*2)
assert f"RTS_SHORTS_PARATROOPER_CUT local={cut} introFrames={cut}" in log
camera_samples=re.findall(r"RTS_SHORTS_PARATROOPER_CAMERA_MOVE local=(\d+) alpha=([\d.]+) location=X=([\d.-]+) Y=([\d.-]+) Z=([\d.-]+) pitch=([\d.-]+) focal=([\d.-]+)",log)
assert len(camera_samples)>=3,camera_samples
first_camera=camera_samples[0]
last_camera=camera_samples[-1]
assert float(first_camera[1])==0,first_camera
move=job["Segments"][0]["ParatrooperCameraMove"]
assert move["DurationSeconds"]==30,move
assert move["EndOffset"]=={"X":0.0,"Y":-3800.0,"Z":1100.0},move
assert job["Segments"][0]["Camera"]["Height"]==2200
start_position=[float(value) for value in first_camera[2:5]]
route=[{"DurationSeconds":move["DurationSeconds"],"Offset":move["EndOffset"],"FocalLength":move["EndFocalLength"]}]+move["ContinuationPoints"]
assert len(route)==3,route
assert route[-1]["Offset"]=={"X":5200.0,"Y":1000.0,"Z":2200.0},route
route_seconds=sum(point["DurationSeconds"] for point in route)
for sample in camera_samples:
    remaining=int(sample[0])/job["Fps"]
    previous=start_position
    previous_focal=job["Segments"][0]["Camera"]["FocalLength"]
    for point in route:
        end_position=[230000+point["Offset"]["X"],10000+point["Offset"]["Y"],144+point["Offset"]["Z"]]
        if remaining<=point["DurationSeconds"] or point is route[-1]:
            break
        remaining-=point["DurationSeconds"]
        previous=end_position
        previous_focal=point["FocalLength"]
    alpha=min(1,remaining/point["DurationSeconds"])
    eased=alpha*alpha*(3-2*alpha)
    expected=[start+(end-start)*eased for start,end in zip(previous,end_position)]
    assert all(abs(float(actual)-wanted)<.1 for actual,wanted in zip(sample[2:5],expected)),sample
    assert abs(float(sample[6])-(previous_focal+(point["FocalLength"]-previous_focal)*eased))<.02,sample
assert abs(float(last_camera[4])-float(first_camera[4]))<20,(first_camera,last_camera)
assert abs(float(last_camera[2])-235200)<1,last_camera
assert abs(float(last_camera[3])-11000)<1,last_camera
assert abs(float(last_camera[5])-float(first_camera[5]))<.1,(first_camera,last_camera)
assert 'leg=1' in log and 'leg=2' in log
assert len(job["Segments"][0]["Caption1"].split("|"))==8
assert len(job["Segments"][0]["Caption2"].split("|"))==8
assert '1× IS-1 (HEAT)' in job['Segments'][0]['Caption1']
assert "S_FIreWeapon_Rus85mm(v=" in log
assert "S_FireWeapon_Flak88(v=" in log
assert "S_FireWeapon_MediumGeneral(v=" in log
assert "Cue_SmallRocketLaunch(v=" in log
segment=manifest["segments"][0]
expected_duration=(segment["endFrame"]-segment["startFrame"])/job["Fps"]
assert abs(duration-expected_duration)<.05,(duration,expected_duration)
final_camera_progress=min(1,max(0,(duration-2)/route_seconds))
report={"video":str(video),"duration_seconds":duration,"aircraft_intro_seconds":2,
        "resolution":[video_stream["width"],video_stream["height"]],"fps":job["Fps"],
        "range_behaviour_recipients":len(recipients),"soldier_owners_verified":len(owners),
        "rear_pits_verified":len(pit_positions),"all_four_pits_firing":True,"faction_header_columns":True,
        "added_infantry_squads_verified":len(infantry_positions),"regular_t34_85_count":4,
        "vulture_squads_verified":len(vulture_positions),"panzer_iv_g_count":2,"panther_d_count":1,
        "forge_is1_count":1,"forge_is1_heat_blueprint_verified":True,
        "both_armies_firing":True,"has_audio":True,"job_complete":True,
        "camera_move_verified":True,"camera_move_reached_fraction":final_camera_progress,
        "camera_route_seconds":route_seconds,"german_reverse_view_verified":True,
        "last_logged_camera_height_above_ground_cm":float(last_camera[4])-144,
        "audio_cache_assertion_after_export":"NumberOfLoadsInFlight.GetValue() == 0" in log,
        "battle_outcome":re.findall(r"RTS_SHORTS_BATTLE_DECIDED[^\r\n]+",log)[-1:]}
(take/"Verification.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps(report,indent=2))
