# Unit portraits (training and selection images)

The square unit images in the training menu and selection panel start as an action shot of the vehicle on the
road in `RTS_GameStartTutorial`, cropped to the vehicle and scaled to 256x256. The tactical class icons are
engraved on top later (not covered here). The untouched images live in
`D:/UE_Assets/Icons/TrainingUI/GerTanks/<ArmoredCars|Light|Medium|Heavy>/NoIcon`.

This used to be done by hand in PIE (order the unit to fire just out of view, take a high-res screenshot, crop,
scale). `UUnitPortraitCaptureSubsystem` does it (vehicles and building expansions such as the gun pits) in an offscreen game run, so it runs while the editor stays open
and in use.

## How it works

- `BP_UnitImage_LandscapeAnchor` (`/Game/RTS_Survival/Blueprints/GameUI/ActionUI/`) marks the spot on
  `RTS_GameStartTutorial`. The vehicle is spawned there, facing the anchor's yaw plus 90 degrees (measured: the
  sun's cast shadow then falls under the vehicle). Hull-aimed vehicles are held on the spot while they turn.
- The subsystem (`RTS_Survival/GameUI/Tests/UnitPortraits/`) only exists when the command line has
  `-UnitPortraitCapture=<list file>`. It is compiled out with `RTS_WITH_MAP_TESTS` (shipping builds: no code).
- Per vehicle: spawn, wait for shaders and loads, take an `idle` picture, order attack-ground straight ahead at half
  the main gun's range (out of frame), and on each main-gun shot (magazine drops) save pictures 0, 2, 4, 9 and 18
  frames later (`shotN_t0` .. `t4`: flash, blast, tracer, smoke). After each shot the gun is reloaded instantly.
  Default 3 shots per unit.
- Three scene-capture cameras per moment: `FrontRight`, `Right` and `Front`, all from the unit's right so the gun
  points right as in the hand-made portraits (1536x1536, 30 degree FOV), framed on the vehicle's mesh bounds
  (meshes over 25 m or 25 m away are ignored). Each picture's projected vehicle box goes to `crops.csv` next to it.
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
| `-UnitPortraitShotDelays=a,b,..` | 0,0.06,0.14,0.3,0.6 | game seconds after a shot to picture; `0,0.034,0.067,0.1,0.134` takes every frame (short muzzle flashes, e.g. the leFH) |
| `-UnitPortraitSkyFill=<x>` | 3 (`SKY_FILL`) | movable sky light with the map's sky cubemap; lifts the shadows |
| `-UnitPortraitExposureBias=<stops>` | 1.0 (`EXPOSURE_BIAS`) | brighter or darker pictures |
| `-UnitPortraitUnitYaw=<deg>` | 90 | vehicle yaw relative to the anchor (90 keeps the cast shadow under the vehicle) |
| `-UnitPortraitIndirectLight=<x>` | off | post-process indirect lighting intensity |
| `-UnitPortraitTargetYaw=<deg>` | 0 | turn the attack-ground target off the nose |
| `-UnitPortraitFraming=<x>` | 1.25 | camera distance on the bounding sphere (crop does the tightening) |
| `-UnitPortraitDebugPostProcess` | off | extra camera that keeps post-process materials (shows the fog of war) |
| `-UnitPortraitDebugViewportShot` | off | tries `HighResShot` of the game view (writes nothing offscreen) |
| `-UnitPortraitShots=0` | - | only the idle picture; also spawns no AI controller, so the unit does not engage the map's enemies |
| `-UnitPortraitRtsView` | off | adds a steep `RtsCamera` view like the in-game camera |
| `-UnitPortraitFixedExposure` | off | manual exposure (the exposure bias still applies), so pictures of a comparison are exposed alike |

A list entry can also change the unit's look, for comparing test materials without editing its Blueprint:
`Name=/Game/.../BP_Unit.BP_Unit_C|Swap=/Game/.../MI_Old.MI_Old>/Game/.../MI_New.MI_New|TeamColor=r,g,b[,a]`. `Swap`
replaces every mesh slot (turret child actors included) showing the first material; `TeamColor` writes custom
primitive data 4-7. Used by `Tools/TeamColor` ([TeamColorShaders.md](../Rendering/TeamColorShaders.md)).

Unreal reads the first copy of a switch, so change the runner's lighting defaults through `SKY_FILL=...` and
`EXPOSURE_BIAS=...` rather than repeating the switch.

## Lighting findings (2026-10-15)

- Scene captures turn Lumen off by default (`SceneCaptureRendering.cpp`: GI and reflections set to None unless the
  capture's post process overrides them). The subsystem turns Lumen back on, but on this map it still adds no sky or
  bounce light in captures (raising Indirect Lighting Intensity to 6 changed nothing). The map's sky light is
  Stationary with the specified cubemap `SunsetAmbientCubemap`.
- Without help every shadow is pure black (about 16% of the pixels). `-UnitPortraitSkyFill=3` spawns a movable sky
  light with that same cubemap and switches the captures to the classic sky light path. The scene only renders the
  newest registered sky light and the map's own one re-registers a frame after start, which silently dropped the
  fill in some runs; the map's sky light is therefore hidden while the fill is used.
- Which flank faces the cameras matters more than the fill: from the unit's left the pictures were the shadowed
  side. Exposure bias 1.0 brings the crops close to the hand-made portraits' brightness.
- The fog of war is a post-process material: with post-process materials kept the picture is black, so the
  captures leave them out.
- Auto exposure adapts at 10 stops/s in the captures so the first vehicle is not over-exposed.
- `RTSFunctionLibrary::ReportError` dialogs (the tutorial map raises many) are answered automatically in unattended
  runs but stall a frame each; the subsystem pushes `PushModalDialogSuppression` so they are only logged.

## German vehicles generated (2026-10-15)

`ger_missing.txt` lists every German vehicle Blueprint without a portrait in the `NoIcon` folders; `ger_picks.txt`
records the frame picked for each. Published (new files, nothing replaced):

- ArmoredCars: Kubelwagen (idle; its Blueprint has no weapon), FlamePuma, Sdkfz250_37mm (idle; no weapon),
  Sdkfz251_Mortar.
- Light: Panzer38t_R, Pz38_Rail, PanzerII_F_Strike, PZII_Fire (Ger_Vehicle_PanzerII_HFire), PzJager_Laser,
  Sdkfz_140_Rockets.
- Medium: PZIV_F_Commander, PZIV_H, PZIV_Rockets.
- Heavy: Panther_G (the engine had an iconed texture but no `NoIcon` source), E25, JagdTiger, Tiger1_Rail.

Not made: `Ger_Vehicle_Sdkfz_9_37` (SdKfz 9 37mm). Its Blueprint has no weapon and a mesh that throws the framing
far out; it is also not in the async spawner's setup arrays. Kubelwagen and SdKfz 250 37mm are not in the spawner
either. Subtypes without any Blueprint (SdKfz 251 Transport, Panzerwerfer, Pz IV/70, Keugel T38, Pz I Scout) have
nothing to picture.

## German gun pits (2026-10-15)

`ger_pits.txt` captures the Pak 40, leFH 18 and sFH 18 pits (`Ger_BXP_GerPit_*`) the same way: spawned on the anchor
as a level designer places them, gun found on the child turret actor. Published to
`D:/UE_Assets/Icons/TrainingUI/GermanPits` as `T_Ger_Pit_{Pak40,LeFH18,SFH18}.png` (picks in
`ger_pits_picks_pak.txt`, `ger_pits_picks_howitzers.txt` and `ger_pits_picks_lefh.txt`).

- The pits are larger, so crop looser: `--zoom 1.0` for the Pak pit, `--zoom 0.85` for the howitzer pits (their
  crates then fit).
- Use the howitzers' `t0` frame (muzzle flash). From `t1` on the blast cloud covers the pit, and in scene captures
  it shows a hard horizontal clipped edge.
- The leFH's muzzle flash lasts about one frame and peaks between the default moments; it was retaken with
  `-UnitPortraitShots=5 -UnitPortraitShotDelays=0,0.034,0.067,0.1,0.134` (every frame after the shot).
- The first try hung at frame 1 with no CPU before the game started (not in the capture code); a rerun worked.
- Later German pits (`ger_pits_new.txt`, picks in `ger_pits_new_picks.txt`): the captured B-4 pit
  (`Ger_BXP_CapturedB4Pit`, under the Soviet B4_203MM folder) and the Flak 37 pit (`Ger_BXP_GerPit_Flak37`), both
  run with every-frame shot delays and cropped at `--zoom 1.0`. The B-4 pit's 20 m footprint is why the mesh-size
  cutoff for framing is 25 m. From the Right view the Flak pit's interior is in shadow but the flash frame still
  reads best there.
