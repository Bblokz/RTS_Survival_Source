import json
from pathlib import Path
root=Path(__file__).resolve().parents[3]/"ShortsCreator"
job_path=root/"Jobs/SovietParatrooperBattle.json"
job=json.loads(job_path.read_text(encoding="utf-8-sig"))
job["UnitOwningPlayer"]=2
job["EditorialOverlayRecipe"]="Source/Tools/ShortsEditing/paratrooper_callouts.json"
job["Segments"][0]["TargetOwningPlayer"]=1
camera={"BackDistance":3200.0,"SideOffset":-1000.0,"LateralShift":0.0,
        "Height":2200.0,"LookAtHeight":300.0,"FocalLength":11.5,"OrbitDegrees":0.0}
# Reach the low flank view sooner, then continue behind the German line for the reverse POV.
move={"DurationSeconds":30.0,"EndOffset":{"X":0.0,"Y":-3800.0,"Z":1100.0},
      "EndLookOffset":{"X":0.0,"Y":0.0,"Z":150.0},"EndFocalLength":8.0,
      "ContinuationPoints":[
          {"DurationSeconds":24.0,"Offset":{"X":4000.0,"Y":-3000.0,"Z":1600.0},
           "LookOffset":{"X":300.0,"Y":0.0,"Z":200.0},"FocalLength":8.5},
          {"DurationSeconds":24.0,"Offset":{"X":5200.0,"Y":1000.0,"Z":2200.0},
           "LookOffset":{"X":620.0,"Y":0.0,"Z":300.0},"FocalLength":11.5}]}
flak="/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_Flak37.Ger_BXP_GerPit_Flak37_C"
ml20="/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/Soviet/ML20_152MM/Pit/Rus_BXP_ML20Pit.Rus_BXP_ML20Pit_C"
lefh="/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Ger_BXP_GerPit_LeFH18.Ger_BXP_GerPit_LeFH18_C"
t34="/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3485.Rus_Vehicle_T3485_C"
ptrs="/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_PTRS.Rus_Squad_PTRS_C"
schreck="/Game/RTS_Survival/Blueprints/Squads/Ger/T2/PanzerGrenadiers/Squad_Ger_PanzerGrenadiers.Squad_Ger_PanzerGrenadiers_C"
vultures="/Game/RTS_Survival/Blueprints/Squads/Ger/ArmoryUnlocked/Squad_Ger_HazmatVultures.Squad_Ger_HazmatVultures_C"
panther="/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/PantherD/Ger_Vehicle_PantherD.Ger_Vehicle_PantherD_C"
is1="/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Heavy/IS-1And-2/Rus_Vehicle_IS-1_Soviet.Rus_Vehicle_IS-1_Soviet_C"
segment=job["Segments"][0]
old_t34=segment["ClassPath"]
segment["ClassPath"]=t34
hazmat=next(path for path in segment["BattleAllyClassPaths"] if 'Hazmat' in path)
panzer=next(path for path in segment["BattleEnemyClassPaths"] if 'PanzerIV_G' in path)
tiger=next(path for path in segment["BattleEnemyClassPaths"] if 'TigerH1' in path)
segment["BattleAllyClassPaths"]=[t34]*3+[hazmat]*2+[ml20]+[ptrs]*3+[is1]
segment["BattleEnemyClassPaths"]=[panzer]*2+[panther,tiger]+[flak]*2+[schreck]*3+[lefh]+[vultures]*6
def offset(x,y):
    return {"X":float(x),"Y":float(y),"Z":0.0}
segment["BattleAllyOffsets"]=[offset(0,y) for y in [700,-700,1400]]+[offset(-600,-1900),offset(0,2100),offset(-1800,0)]+[offset(800,y) for y in [-1200,0,1200]]+[offset(0,-1400)]
segment["BattleEnemyOffsets"]=[offset(0,y) for y in [-1200,1200,-400,400]]+[offset(1500,-1000),offset(1500,1000)]+[offset(-800,y) for y in [-1200,0,1200]]+[offset(3200,0)]+[offset(-1050,y) for y in [-1700,-1020,-340,340,1020,1700]]
segment["Caption1"]="SOVIETS | 4× T-34-85 | 1× IS-1 (HEAT) | 3× PTRS squads | 2× Hazmat squads | 1× ML-20 pit | 1× Mosin squad | 1× ROKS squad"
segment["Caption2"]="GERMANS | 2× Panzer IV G | 1× Panther D | 1× Tiger H1 | 6× Vulture squads | 3× Panzerschreck | 2× Flak 37 pits | 1× leFH 18 pit"
job['CaptionScale']=.62
job["UnitLabels"].pop(old_t34,None)
job["UnitLabels"][t34]="T-34-85"
job["UnitLabels"][ptrs]="PTRS squad"
job["UnitLabels"][schreck]="Panzerschreck squad"
job["UnitLabels"][lefh]="leFH 18 pit"
job["UnitLabels"][flak]="Flak 37 pit"
job["UnitLabels"][ml20]="ML-20 pit"
job["UnitLabels"][vultures]="Vulture squad"
job["UnitLabels"][panther]="Panther D"
job["UnitLabels"][is1]="IS-1 (HEAT)"
job["Segments"][0]["Camera"].update(camera)
job["Segments"][0]["ParatrooperCameraMove"]=move
job_path.write_text(json.dumps(job,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
setup_path=root/"Setups/ParatrooperBattle.json"
setup=json.loads(setup_path.read_text(encoding="utf-8-sig"))
setup["Job"]["UnitOwningPlayer"]=2
setup["Segment"]["TargetOwningPlayer"]=1
setup["Segment"]["Camera"].update(camera)
setup["Segment"]["ParatrooperCameraMove"]=move
setup_path.write_text(json.dumps(setup,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
job["JobName"]="SovietParatrooperBattle_Test"
job["Fps"]=30
job["ResolutionX"]=540
job["ResolutionY"]=960
(Path(__file__).parent/"SovietTest.json").write_text(json.dumps(job,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
print("Updated rear gun pits, slower left camera move and faction columns in the existing short.")
