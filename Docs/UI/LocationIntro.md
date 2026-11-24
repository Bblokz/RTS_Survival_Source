# Mission location intro

One mission node, `Show Location Intro` (category Mission|Camera, on every `UMissionBase` Blueprint), plays a short
cinematic card over the normal game view:

- the HUD and the mission widgets hide, and camera input is locked, as in a cinematic takeover;
- black cinema bars slide in at the top and bottom;
- the location (shown in capitals) appears top centre: two brass rules grow outwards and the letters close in from
  wide spacing, with an optional smaller subtitle line under it;
- the date types out bottom left in a typewriter face beside a brass bar, with an optional time line under it;
- the player camera drifts along world X or Y the whole time, and can also turn (pan);
- everything fades, the bars slide away, the HUD comes back and `BP_OnLocationIntroFinished(ID, bWasShown)` fires.

## Pins

| Pin | Meaning |
| --- | --- |
| Location, Date | The two texts. |
| Drift Axis, Drift Distance | World axis and units (default X, 1500) the camera moves over the intro; negative goes the other way, 0 keeps it still. The move stops at a camera boundary. |
| Duration Seconds | Real seconds, default 7. Zero uses 7; a duration too short for the date and time to type out and stay readable is lengthened. |
| Pan Camera, Pan Settings | Turn the camera by Yaw/Pitch degrees (an orbit around the point it looks at). With Return To Start Rotation (default) it turns back in the last Return Seconds, so the player gets their usual view back. |
| ID (advanced) | Handed back in `BP_OnLocationIntroFinished`. |
| Location Subtitle, Time Of Day (advanced) | Optional extra lines. |
| Presentation (advanced) | Bars on/off and height, fade from black (for the very start of a map), when the texts start, typing speed, fade-out length, an intro sound and a per-letter typewriter sound. The defaults are the tuned look. |

## Behaviour

- Waits when the screen is busy (a cinematic, an info takeover, the tech tree or archive) or a `MoveCamera` move is
  still under way, and plays once that is over, starting from where the camera landed; after 45 unpaused seconds of
  waiting it is skipped and the event fires with `bWasShown` false.
- A mission may complete right after the node (e.g. a mission whose only job is the intro): the intro plays on and the
  next mission's widget shows when the HUD comes back.
- A second intro on the same mission while one runs is refused (event next frame, `bWasShown` false).
- A cinematic of the same mission (`OnCinematicTakeOverWithSkipping` or `OnCinematicTakeOverFromMission`) started
  over a showing intro ends the intro first (no event); a waiting intro plays after the cinematic. A cinematic from
  another mission is not guarded: the intro's end would bring the HUD back over it.
- Escape during the intro only shows "Pause is disabled during cinematic takeover", as in a cinematic.
- Mission failure and mission clean-up end it and hand the screen back (no event).
- Like a cinematic takeover it stops a playing portrait and mutes regular voice lines while it shows, so do not
  start a portrait right before it.

## Code

- `GameUI/LocationIntro/LocationIntroTypes.h`: pin structs.
- `GameUI/LocationIntro/W_LocationIntro`: pure C++ overlay (no Blueprint asset); `Style` holds fonts and colours.
- `Missions/MissionClasses/MissionBase/MissionLocationIntroSession`: waits for the screen, takes it over, drives camera
  and overlay from one real-time clock, hands the screen back.
- `Missions/MissionClasses/MissionBase/MissionLocationIntro.cpp`: the node and its event.
- `UPlayerCameraController::ApplyScriptedCameraPose`: moves the camera pawn inside the camera bounds and turns it.

## Test

`Source/Tools/LocationIntro/location_intro_test.py` in a throwaway DebugGame editor on RTS_ToCopySmall (command line in
the script). `LOCATION_INTRO_BUSY=1` locks the screen first to check the wait; `LOCATION_INTRO_FADE=1` turns on the fade
from black; `LOCATION_INTRO_CHAIN=1` runs an instant and a timed `MoveCamera` before the node and completes the mission
into a next mission right after it. It checks the screen is handed back, the camera moved by the drift and the rotation
returned.
