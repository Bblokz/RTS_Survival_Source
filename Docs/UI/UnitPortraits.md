# Unit portraits (training and selection images)

The square unit images in the training menu and selection panel start as an action shot of the vehicle on the
road in `RTS_GameStartTutorial`, cropped to the vehicle and scaled to 256x256. The tactical class icons are
engraved on top later (not covered here). The untouched images live in
`D:/UE_Assets/Icons/TrainingUI/GerTanks/<ArmoredCars|Light|Medium|Heavy>/NoIcon`.

This used to be done by hand in PIE (order the unit to fire just out of view, take a high-res screenshot, crop,
scale). `UUnitPortraitCaptureSubsystem` does it in an offscreen game run, so it runs while the editor stays open
and in use.

## How it works

- `BP_UnitImage_LandscapeAnchor` (`/Game/RTS_Survival/Blueprints/GameUI/ActionUI/`) marks the spot on
  `RTS_GameStartTutorial`. The vehicle is spawned there, facing the anchor's yaw.
- The subsystem (`RTS_Survival/GameUI/Tests/UnitPortraits/`) only exists when the command line has
  `-UnitPortraitCapture=<list file>`. It is compiled out with `RTS_WITH_MAP_TESTS` (shipping builds: no code).
- Per vehicle: spawn, wait for shaders and loads, take an `idle` picture, order attack-ground straight ahead at half
  the main gun's range (out of frame), and on each main-gun shot (magazine drops) save pictures 0, 2, 4, 9 and 18
  frames later (`shotN_t0` .. `t4`: flash, blast, tracer, smoke). After each shot the gun is reloaded instantly.
  Default 3 shots per unit.
- Three scene-capture cameras per moment: `FrontRight`, `Right` and `FrontLeft` (1536x1536, 30 degree FOV), framed
  on the vehicle's mesh bounds. Each picture's projected vehicle box goes to `crops.csv` next to it.
- The game view itself is never drawn offscreen, so `HighResShot` does not work there; scene captures do.

## Running it

```bash
cd Source/Tools/UnitPortraits
./run_capture.sh ger_missing.txt D:/UE5Projects/RTS_Survival/Saved/UnitPortraits/ger
python crop_portraits.py D:/UE5Projects/RTS_Survival/Saved/UnitPortraits/ger D:/UE5Projects/RTS_Survival/Saved/UnitPortraits/ger_crop --sheet --zoom 1.1
python publish_portraits.py ger_picks.txt D:/UE5Projects/RTS_Survival/Saved/UnitPortraits/ger_crop
```

1. `run_capture.sh <list> <out folder>` runs `UnrealEditor-Cmd -game -RenderOffscreen` on the Development editor
   target (build it first; the user's editor runs DebugGame, so there is no DLL lock). About 30 s per vehicle.
   Log: `Saved/Logs/UnitPortraits.log`, grep `RTS_PORTRAIT`.
2. `crop_portraits.py` squares each picture on its `crops.csv` box and scales it to 256. `--sheet` writes a contact
   sheet per unit for picking; `--zoom` > 1 crops tighter.
3. `publish_portraits.py <picks> <crop folder>` copies the picked crop of each unit into its `NoIcon` folder, named
   after the list entry. It never replaces an existing portrait without `--overwrite`.

List files are `Name=/Game/.../BP_Unit.BP_Unit_C`, one per line; the name is the portrait file stem
(`T_Ger_HeavyTank_E25`) and its prefix picks the `NoIcon` folder. `trial.txt` holds the Panzer IV G, which already
has a hand-made portrait, for comparing looks after changes.

### Switches

| Switch | Default (runner) | Use |
|---|---|---|
| `-UnitPortraitCapture=<file>` | required | unit list |
| `-UnitPortraitDir=<folder>` | `Saved/UnitPortraits` | output root |
| `-UnitPortraitShots=N` | 3 | main-gun shots pictured per unit |
| `-UnitPortraitSkyFill=<x>` | 3 (`SKY_FILL`) | movable sky light with the map's sky cubemap; lifts the shadows |
| `-UnitPortraitExposureBias=<stops>` | 0.3 (`EXPOSURE_BIAS`) | brighter or darker pictures |
| `-UnitPortraitIndirectLight=<x>` | off | post-process indirect lighting intensity |
| `-UnitPortraitTargetYaw=<deg>` | 0 | turn the attack-ground target off the nose |
| `-UnitPortraitFraming=<x>` | 1.25 | camera distance on the bounding sphere (crop does the tightening) |
| `-UnitPortraitDebugPostProcess` | off | extra camera that keeps post-process materials (shows the fog of war) |
| `-UnitPortraitDebugViewportShot` | off | tries `HighResShot` of the game view (writes nothing offscreen) |

Unreal reads the first copy of a switch, so change the runner's lighting defaults through `SKY_FILL=...` and
`EXPOSURE_BIAS=...` rather than repeating the switch.

## Lighting findings (2026-10-15)

- Scene captures turn Lumen off by default (`SceneCaptureRendering.cpp`: GI and reflections set to None unless the
  capture's post process overrides them). The subsystem turns Lumen back on, but on this map it still adds no sky or
  bounce light in captures (raising Indirect Lighting Intensity to 6 changed nothing). The map's sky light is
  Stationary with the specified cubemap `SunsetAmbientCubemap`.
- Without help every shadow is pure black (about 16% of the pixels). `-UnitPortraitSkyFill=3` spawns a movable sky
  light with that same cubemap and switches the captures to the classic sky light path; shadows then read like the
  hand-made portraits. In one run with fill 2 the sun's cast shadow went missing; fill 3 gave the same result in two
  runs.
- The fog of war is a post-process material: with post-process materials kept the picture is black, so the
  captures leave them out.
- Auto exposure adapts at 10 stops/s in the captures so the first vehicle is not over-exposed.
- `RTSFunctionLibrary::ReportError` dialogs (the tutorial map raises many) are answered automatically in unattended
  runs but stall a frame each; the subsystem pushes `PushModalDialogSuppression` so they are only logged.
