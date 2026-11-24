# HQ unload crane (loop monorail)

Harvesters no longer just hand their cargo over at the Strike Division HQ. They reverse into one of two bays in front of
the truck nose. An overhead loop rail carries one trolley per bay, and each trolley lifts the cargo out of the hopper in
chunks and drops it into the radixite or metal bin. The bins' storage-level meshes and the harvester's cargo meshes work
as before. A "+ amount" pop-up shows over the bin on every landed load.

Design history: concept sheets and rough models in `_AssetSource/HQUnloadCrane/` (`build_monorail.py`, model B "loop" picked
2026-11-23).

## Pieces

| What | Where |
|---|---|
| Station actor | `RTS_Survival/Resources/UnloadStation/ResourceUnloadStation` (`AResourceUnloadStation`) |
| Types (bay/bin settings, trolley state, end reasons) | `Resources/UnloadStation/UnloadStationTypes.h` |
| Harvester side | `UHarvester`: states `MoveToUnloadBayApproach`, `ReverseIntoUnloadBay`, `CraneUnloading` |
| Drop-off side | `UResourceDropOff::DropOffResourcesFromCrane`, `SetUnloadStation` / `GetUnloadStation` |
| Spawn | `ANomadicVehicle::M_UnloadStationClass`, spawned in `CreateBuildingAttachments`, removed with the attachments |
| Meshes + Blueprint | `/Game/.../ChildBlueprints/SKDFZ-7/UnloadCrane/` (`SM_HQUnload_*`, `BP_HQUnloadStation`) |
| Mesh source | `_AssetSource/HQUnloadCrane/export_unload_assets.py` -> `export/*.fbx` |
| Import + hook-up | `Source/Tools/UnloadStation/ue_import_unload_station.py` (headless, DebugGame) |
| Test | `Resources/UnloadStation/Tests/UnloadStationTestSubsystem` (`-UnloadStationValidate`) |

## How it works

- The station is attached to the HQ building mesh, so its frame is the building mesh's frame (cm). Rail, bays and bin
  fallbacks are authored there. The C++ defaults hold the HQ's values; the Blueprint only sets the meshes.
- **Rail.** `RailControlPoints` are densified with Catmull-Rom exactly as `build_monorail.py` builds the rail mesh, so the
  spline sits on the mesh. Rail centre height 480 cm.
- **Bays.** Each bay has a pick point (where the harvester's cargo socket `Crystals` must stand), a dock yaw and a run
  direction. Each bay's trolley only uses its own stretch of the loop: bay L (Y -205) runs against the rail direction to the
  bins on the outer leg; bay R (Y 262) runs along it, back across the hood to the bins on the inner leg. The stretches
  never overlap, so both bays unload at the same time without traffic logic. Bin stops are found at BeginPlay: the first
  pass of the rail over each bin socket (`Radixite`, `Metal`) in the bay's run direction.
- **Harvester route.** At `MoveToDropOff`, if the drop-off has a station with a free bay, the harvester reserves the bay
  nearest to it, drives to a point 9 m ahead of the bay, then reverses in with the track path follower's force-reverse.
  A short guided slide (teleport-lerp, at most 13 m, about 2.6 m/s and 90 deg/s) lines it up exactly. A third harvester, or one
  whose route fails, uses the old drop-off point.
- **Unload.** The cargo is split into `TripsPerUnload` (3) chunks. Per trip the trolley runs to the bay, lowers, grabs (the
  pooled crystals (Pz I cargo mesh) or ingots (Pz IV cargo mesh) appear in the jaws), hoists, runs to the bin, lowers and opens. At that moment the chunk is
  credited through the drop-off and taken off the harvester in one step. The load in the air is only visual: if the
  harvester leaves (new order, dies) or the station goes (pack-up), nothing in flight is credited and the rest stays on
  board.
- **End.** Cargo empty: the harvester goes back to harvesting while the empty grab returns. Bin full: it looks for another
  drop-off with what is left. Station gone: same.

## Tuning (BP_HQUnloadStation / C++ defaults)

`TripsPerUnload` 3, `TrolleySpeed` 380 cm/s, `HoistSpeed` 190 cm/s, `GrabSeconds` 0.45, `ReleaseSeconds` 0.35. One trip on
bay L takes about 9 s, so a full unload adds about 30 s plus docking to a harvest cycle compared with the old instant drop-off.
Balance is unchanged otherwise; see `Docs/Balance` before touching harvest numbers.

## Headless test

```bash
MSYS_NO_PATHCONV=1 UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nosound -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate -UnloadStationValidate -abslog=<file>
```

Spawns an unpacked `Ger_HQ_Truck` at `-UnloadStationAt=X,Y` (default 16796,-15808) and two Pz IV harvesters with full cargo
(450 radixite, 225 metal). Checks: both dock (cargo within 1.2 m of the pick point, facing away from the HQ within 12 deg),
both bays docked and both trolleys working at the same time, exactly the cargo credited, radixite in at least 3 chunks,
bays released. Then a third trip: the HQ packs up while a load is in the air; cargo on board + credited must equal what
it carried. Logs `RTS_UNLOAD_TEST check=PASS|FAIL` and `RTS_UNLOAD_TEST RESULT`. Pictures (needs rendering, drop
`-nullrhi`, add `-RenderOffscreen`): `-UnloadStationCapture=<folder>`.

## Known limits

- During the guided slide the tracks do not animate (the harvester is moved, not driven).
- Rail and posts have no collision and no nav effect: units drive through the portal feet and the spider post.
- The portal feet stand in the sandbag row next to the truck nose; one sandbag group at X 690-780 / Y 310-400 stands in
  bay R.
- Only the Strike Division HQ and refinery have stations (the refinery's knuckle boom: `RefineryUnloadCrane.md`).
  Other drop-offs (harvester hub, other factions) are unchanged.
