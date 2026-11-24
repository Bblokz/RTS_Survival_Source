# Metal depot unload elevator (bucket elevator)

The Strike Division metal depot (Opel Blitz metal vault) unloads harvesters with an inclined bucket elevator over its
north sandbag wall. A harvester reverses up to the elevator's boot, nose away from the depot. A feed apron swings down
onto its rear and ingots pour over the hopper's rear rim, slide down the apron into the boot, ride up the 4.6 m boom in
the buckets, and are thrown off the head pulley into the bin on the truck bed, where the storage-level mesh fills as
before. A "+ amount" pop-up shows over the bin each time a chunk lands.

It reuses the HQ crane's station logic (`Docs/Units/HQUnloadCrane.md`): bay reservation, reverse-in, chunked crediting,
and what happens when the harvester leaves or the depot packs up are all the same. The refinery's overflow point
(`Docs/Units/RefineryUnloadCrane.md`) is reused too.

Design history: `_AssetSource/MetalDepotUnload/`. Five non-crane ideas (2026-11-24), option 2 (bucket elevator) picked;
three rough models in `build_elevator.py` (A enclosed tower, B inclined open elevator, C open paternoster frame); B
picked and built the same day.

## Pieces

| What | Where |
|---|---|
| Station actor | `RTS_Survival/Resources/UnloadStation/BucketElevatorUnloadStation` (`ABucketElevatorUnloadStation`, child of `AResourceUnloadStation`) |
| Base hooks it uses | `GetUsesTrolleys`, `OnUnloadStationReady`, `TickUnloadStation`, `GetHarvesterDockPoint`, `GetBayState`, `DeliverBayChunk` on `AResourceUnloadStation` |
| Meshes + Blueprint | `/Game/.../ChildBlueprints/GerEco/Opel_MetalVault/UnloadElevator/` (`SM_DepotElevator_*`, `BP_DepotElevatorStation`); the ingots are the HQ's `SM_HQUnload_Ingot` |
| Model, clearance sweep, preview, FBX export | `_AssetSource/MetalDepotUnload/build_bucket_elevator.py` (`-- export render`, `pz1` checks the Pz I) |
| Import + hook-up | `Source/Tools/UnloadStation/ue_import_depot_elevator.py` (headless, DebugGame) |
| Test | `UnloadStationTestSubsystem` with `-UnloadStationValidate -UnloadStationTruck=MetalDepot` |

## How it works

- **No trolleys.** The station returns false from `GetUsesTrolleys`, so the base class makes no trolleys and the
  elevator drives its one bay from `TickUnloadStation`. The base still gets a "rail": the boom's ground line from the
  boot to the head, which only tells it that the station reaches the metal bin.
- **Docking by the rear bumper.** `GetHarvesterDockPoint` returns the lowest X of the harvester's hull (the mesh with
  the cargo socket), so every harvester backs up to the same spot in front of the boot: rear bumper on (-22, -470),
  facing -Y.
- **Apron.** When the harvester docks, the apron (hinged on the boot at (-22, -446, 152)) swings down in 1.4 s until its
  tip rests 40 cm ahead of the rear bumper and 75 cm above the cargo socket (just under the Pz IV's hopper rim). It is
  scaled along its length to reach. It swings back up once every bar is off it.
- **Belt.** 25 buckets on one loop: up strand, half wrap round the head pulley (-22, -45, 410), return strand, half
  wrap round the boot pulley (-22, -368, 80), 30 cm off the boom axis. The belt runs at 55 cm/s while anything is in the
  system and ramps over 0.8 s. The pulleys spin with it.
- **Bars.** One ingot is fed per bucket pitch (0.81 s). Each pours over the rim onto the apron tip (0.5 s), slides down
  to the hinge and drops into the boot. A bucket passing the bottom of the boot wrap picks one up; 70 degrees round the
  head pulley, the bucket's mouth faces down and the bar is thrown into the bin (it stops 45 cm above the bin socket).
- **Credit.** 32 bars per unload, four chunks (`TripsPerUnload`): every eighth bar that lands credits one chunk through
  the drop-off, with the pop-up. The last bar credits whatever is left. Bars in the air are only visual: when the
  harvester leaves or the bin fills, the ones still in flight vanish and the apron goes up.
- **Overflow.** The old drop-off point (-92, -352) lies under the boot, so bay-less harvesters go to the overflow point
  (1050, -750), east of the depot and about 11 m from the docked hull (see the refinery doc for why it is that far).
- **Cargo.** Only metal has a bin. Vehicle-parts harvesters never reserve the bay and go to the overflow point.

The rig numbers live in both the C++ defaults and `build_bucket_elevator.py`; change them in both.

## Tuning (BP_DepotElevatorStation / C++ defaults)

`BeltSpeed` 55 cm/s, `BarsPerUnload` 32, `TripsPerUnload` 4, `ApronSeconds` 1.4, `PourSeconds` 0.5. A full unload takes
38.5 s from docking (225 metal on a Pz IV, measured headless 2026-11-24: docked at t=11.3 s, empty at t=49.8 s),
against about 37 s at the HQ and the refinery. It splits into 1.4 s apron swing, about 26 s of feeding (32 bars, one
per 0.81 s bucket pitch) and about 11 s for the last bar to pour, slide, ride the 5.5 m from the boot to the throw
point and fall into the bin. To shorten it, raise `BeltSpeed` (the feed rate follows it) or lower `BarsPerUnload`. No balance numbers changed; see
`Docs/Balance` before touching harvest numbers.

## Clearances

`build_bucket_elevator.py` sweeps every bucket over one pitch (6 samples each, so the whole loop) and the apron through
its swing, against the depot mesh, the docked harvester (Pz IV, or the Pz I with `pz1`) and the frame. Nothing touches.
Two steel stakes stand in the north sandbags at X -79..-70 and 25..32 (up to Z 239); the boom passes between them, which
is why its centre line is X -22 and the chords are only 68 cm apart. The masts stand just south of the boot casing.

## Headless test

```bash
MSYS_NO_PATHCONV=1 UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nosound -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate -UnloadStationValidate -UnloadStationTruck=MetalDepot -abslog=<file>
```

The refinery's single-bay run with metal: an unpacked `Ger_MetalVault_Truck` and two Pz IV harvesters carrying metal
(each half the depot's free metal storage, at most 225). Exactly one docks (within 1.2 m and 12 deg of the station's dock
transform) and is unloaded by the elevator, the other unloads at the overflow point, exactly the cargo is credited in
at least 4 store changes, and the bay is released. Then the pack-up check. Pictures: drop `-nullrhi`, add
`-RenderOffscreen -UnloadStationCapture=<folder>`. Single-bay runs capture `rig_lowering` (apron swing), `station_busy`,
`mid_unload_06s/14s/24s` (bars on the belt and in the air), `after_unload` and `pack_up_moment`.

## Known limits

- The hopper does not tip: the bars come over the rear rim on their own. A real tip needs the hopper split out of
  `SM_PZ4Harvester_Hull` (shared with every Pz IV harvester) as its own pivoting part, with the cargo mesh on it.
- Same as the HQ: tracks do not animate during the guided slide; the elevator has no collision and no nav effect; it
  pops in after the deploy and vanishes on pack-up.
- The apron and bar path adapt to each harvester's rear and cargo height, but only the Pz IV was checked in UE; the Pz I
  was checked in Blender with an assumed cargo height.
