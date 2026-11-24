# Compares UnitMaster vs InfantryMaster skeleton bones / reference pose, to see if the casual (bucket, wood) anims can
# be key-copied onto the infantry skeleton. Headless: -run=pythonscript -script=<this>
import unreal

OUT = unreal.Paths.project_saved_dir() + 'SovietAirSupply/probe_skeletons.txt'
lines = []
A = unreal.AnimPoseExtensions

um = unreal.load_asset('/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/CasualAnims/UnitMaster_Skeleton')
im = unreal.load_asset('/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Skeletons/InfantryMaster_Skeleton')
pu = A.get_reference_pose(um)
pi = A.get_reference_pose(im)
bu = [str(b) for b in A.get_bone_names(pu)]
bi = [str(b) for b in A.get_bone_names(pi)]
lines.append('unitmaster bones %d, infantry bones %d' % (len(bu), len(bi)))
lines.append('only in unitmaster: %s' % [b for b in bu if b not in bi])
lines.append('only in infantry: %s' % [b for b in bi if b not in bu])
for b in bu:
    if b not in bi:
        continue
    tu = A.get_bone_pose(pu, b, unreal.AnimPoseSpaces.LOCAL)
    ti = A.get_bone_pose(pi, b, unreal.AnimPoseSpaces.LOCAL)
    ru = tu.rotation.rotator()
    ri = ti.rotation.rotator()
    dl = (tu.translation - ti.translation).length()
    dr = max(abs(ru.pitch - ri.pitch), abs(ru.yaw - ri.yaw), abs(ru.roll - ri.roll))
    if dl > 1.0 or dr > 3.0:
        lines.append('DIFF %-16s dloc=%.1f drot=%.1f  um=%s/%s  inf=%s/%s' % (
            b, dl, dr, tu.translation, ru, ti.translation, ri))
for path in ['/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/CasualAnims/Anim_Bucket_Walk_Forward',
             '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/CasualAnims/Anim_Holding_Wood',
             '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/CasualAnims/Anim_Grabbing_Bucket',
             '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/CasualAnims/Anim_Dropping_Bucket',
             '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/CasualAnims/Anim_Bucket_Idle',
             '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/CasualAnims/Anim_Male_Walk',
             '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/Idle/IdleWalkAround/Anim_Idle_Walk1',
             '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/TeamWeaponAnims/Howitzer/AN_Infantry_Crew_AmmoSupply',
             '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/TeamWeaponAnims/Idle/Anim_TW_StandingIdle']:
    s = unreal.load_asset(path)
    try:
        lines.append('ANIM %s len=%.2f frames=%d rootmotion=%s' % (
            path.split('/')[-1], s.get_play_length(), s.get_editor_property('number_of_sampled_frames')
            if False else 0, s.get_editor_property('enable_root_motion')))
    except Exception as e:
        lines.append('ANIM %s err %s' % (path, e))
with open(OUT, 'w') as f:
    f.write('\n'.join(lines))
