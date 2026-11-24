# Mission camera tour, fog reveal and the "Secure the line" mission

Three pieces, built 2026-11-16 for `Ger_IntroMission` and usable on any mission map.

## `Play Camera Tour` (every `UMissionBase` Blueprint, category Mission|Camera)

A short in-map cinematic over the normal RTS camera, no Sequencer asset needed:

- the HUD and mission widgets hide and camera input locks (the mission cinematic takeover);
- black cinema bars slide in;
- the camera moves through the shots; each shot eases from its start pose to its end pose while its caption (title
  between two brass rules, smaller line under it) fades in and out bottom centre;
- after the last shot the camera eases back to where the player had it (position, turn and zoom), the bars slide
  away, the HUD returns and `BP_OnCameraTourFinished(ID, bWasShown)` fires.

| Shot field (`FMissionCameraTourShot`) | Meaning |
| --- | --- |
| Focus Start / Focus End | Ground point, with its height, in the middle of the screen at the start and end of the shot. |
| Yaw Start / Yaw End | World direction the camera looks in (0 = +X). It turns the short way round. |
| Pitch Start / Pitch End | Extra tilt on top of the camera's usual 70 degrees down: +20 gives a flatter 50 degree view. -40..30. |
| Arm Length Start / End | Distance from camera to focus (the zoom; gameplay range 100-5200); 0 keeps the player's zoom. |
| Seconds | Real seconds of the shot. |
| Blend In Seconds | The first part of the shot eases over from wherever the previous shot left the camera; 0 cuts. |
| Title / Subtitle | Caption; an empty title shows none. |

`FMissionCameraTourSettings`: bars on/off and height, `ReturnSeconds` (ease back to the player's view) and a start
sound.

Behaviour, the same as `Show Location Intro`: it waits while the screen is busy (another cinematic, an info takeover,
the tech tree, a `MoveCamera` move) and is skipped after 45 unpaused seconds (`bWasShown` false); one tour per mission
at a time; a takeover stops a playing portrait and mutes voice lines, so play lines after the event.

Portraits during a tour: the HUD (with its portrait) is hidden, so the tour overlay adds its own copy of the HUD
portrait widget (same class, W_BP_Portrait) at the HUD portrait's screen place, above the bars, and the portrait manager
shows portraits on it until the tour ends (`UPlayerPortraitManager::SetCinematicPortraitHost` /
`ClearCinematicPortraitHost`). A `PlayPortrait` during the tour therefore shows the speaker with the word-timed
subtitle box like in play; a line still playing when the tour ends moves back to the HUD portrait, its words still
following the audio. Mission failure
and clean-up end it and hand the screen back (no event). `GetIsCameraTourPlaying` and `GetCameraTourShotIndex`
(-1 when no shot shows) are there for Blueprints and tests.

How the camera is aimed: the camera hangs on the pawn's spring arm and looks through the pawn, and the pawn keeps
its own height. To put a ground point at another height in the middle of the screen, the session slides the pawn
along the view direction (`UMissionCameraTourSession::ApplyPose`). The pawn moves stay inside the camera bounds like
any scripted camera move.

Code: `Missions/MissionClasses/MissionBase/MissionCameraTour.cpp` (the nodes),
`MissionCameraTourSession.h/.cpp` (real-time clock, takeover, camera), `GameUI/CameraTour/` (types and the pure C++
overlay `UW_MissionCameraTour`), `UPlayerCameraController::GetScriptedCameraArmState` / `ApplyScriptedCameraArmLength`.

## `Reveal Area For Seconds` (Mission|FogOfWar)

Lifts the fog for the player over a circle for a while: a recon-flight vision bubble
(`AGlobalAbilityVisionBubble`) spawned for player 1. Returns the bubble actor; it removes itself after `Seconds`.

## `USecureBaseMission` (C++ mission class)

"Destroy these enemy buildings", with an intro that shows the player what and where they are. At its start (after its
trigger):

1. **Targets:** every living actor of `TargetClasses` (subclasses count) inside the target box (2D:
   `TargetAreaCentre` +- `TargetAreaHalfExtent`), sorted west to east. Each gets a `TargetMarkerClass` actor above it
   (its `SetText` event gets `TargetMarkerText`) and a minimap icon (`TargetMiniMapIcon`).
2. **Reveal:** a vision bubble of `RevealRadius` on every `RevealCentres` point for `RevealSeconds`.
3. **Base life:** `MarchingSquads` spawn (exact class) and march back and forth between their spawn point and their
   march-to point (`MarchToLocation` order, given again for 10 s while a fresh squad settles). `FuelDelivery`: the
   crane (`AMissionFuelCrane` child) spawns at once; the truck spawns `TruckSpawnDelaySeconds` after the tour starts
   (0 = with the squads), drives to `ParkLocation`, and once it stands still within `ParkedRadius` the crane gets
   `StartUnloading(Truck)`. The squads, truck and crane are ordinary enemy actors afterwards: the player fights them.
4. **Tour:** waits until the squads have spawned (at most `MaxSecondsWaitingForBaseLife`), then plays `TourShots`.
   Optional `TourVoiceLine` on `TourVoiceLinePortrait`: it starts `TourVoiceLineDelaySeconds` after the first shot
   shows, because the tour's takeover stops any portrait already playing. The portrait and its subtitle show on
   the tour overlay's portrait copy (see above). A tour that never showed plays the line once it hands the
   screen back, before `ObjectiveVoiceLine`. On `Ger_IntroMission` it is `IntroMission_SecureLineTour` (19.7 s,
   tested by `Source/Tools/IntroMission/tour_voice_line_headless_test.py`).
5. **Objective:** `ObjectiveTitle` / `ObjectiveDescription` (`{Destroyed}` and `{Total}` are filled in) on the mission
   panel, refreshed per kill; optional `ObjectiveVoiceLine` on `ObjectivePortrait` after the tour.
6. **Completion:** a target counts as destroyed when it is gone or no longer alive (`AActor::IsUnitAlive`, polled
   every 0.5 s, so a building that leaves a wreck counts too). Its marker and icon go, `BP_OnSecureBaseTargetDestroyed`
   fires, and once all are down (and the tour is over) the mission completes. With no living target at start it
   completes right after its tour.

Test getters: `GetLivingTargets`, `GetTargetCount`, `GetDestroyedTargetCount`, `GetBaseLifeActors`,
`GetMarchingSquadCount`, `GetFuelTruck`, `GetFuelCrane`, `GetHasFuelTruckParked`, `GetHasTourFinished`.

Code: `Missions/MissionClasses/SecureBaseMission/`. Its first use is `Mission_Intro_ClearSovietRig` on
`Ger_IntroMission` (see `Source/Docs/Campaign/GerIntroMission.md`), set up by
`Source/Tools/IntroMission/setup_secure_line_mission.py` and tested by `secure_line_headless_test.py`.
