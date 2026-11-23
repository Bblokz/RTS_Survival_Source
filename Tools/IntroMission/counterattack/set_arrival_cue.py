"""Puts the air-drop announcer cue on LS_Intro_FirstWaveArrival's audio track (2026-11-22).

    UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript
      -script="D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack/set_arrival_cue.py"
      -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate

The cinematic's camera is driven by the mission, but its clock is this sequence: with a train the drop shot starts at
TrainShotSeconds (2 s), so the cue starts there and plays over the cut to the landing field. A sound on the sequence's
own audio track also gets its subtitle from W_CinematicOverlay (DT_CinematicTranscripts row), and the cinematic
takeover does not mute it the way it mutes portrait voice lines. Re-running replaces the cue. Saves only the sequence.
"""
import json

import unreal

SEQUENCE = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/LS_Intro_FirstWaveArrival'
CUE = '/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/IntroMission/IntroMission_ArrivalCue'
CUE_START_SECONDS = 2.0
REPORT = 'D:/UE5Projects/RTS_Survival/Saved/CounterattackPresentationTest/set_arrival_cue.json'

out = {}
seq = unreal.load_asset(SEQUENCE)
cue = unreal.load_asset(CUE)
for track in list(seq.get_tracks()):
    if isinstance(track, unreal.MovieSceneAudioTrack):
        seq.remove_track(track)
        out['removed_old_track'] = True
track = seq.add_track(unreal.MovieSceneAudioTrack)
section = track.add_section()
section.set_sound(cue)
duration = cue.get_editor_property('duration')
section.set_range_seconds(CUE_START_SECONDS, CUE_START_SECONDS + duration)
end = max(seq.get_playback_end_seconds(), CUE_START_SECONDS + duration + 1.0)
seq.set_playback_end_seconds(end)
out.update({'cue': cue.get_path_name(), 'start': CUE_START_SECONDS, 'duration': duration, 'playback_end': end,
            'saved': bool(unreal.EditorAssetLibrary.save_loaded_asset(seq, False))})
with open(REPORT, 'w') as fh:
    json.dump(out, fh, indent=1)
unreal.log_warning('ARRIVAL_CUE ' + json.dumps(out))
