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
  harvester leaves (dies, is towed or stunned) or the station goes (pack-up), nothing in flight is credited and the rest
  stays on board.
- **Unload lock.** From the moment the harvester starts reversing into its bay until its unload ends
  (`UHarvester::GetIsLockedForUnload`), orders do not interrupt it. Any order given without shift (move, stop, attack,
  another harvest target, ...) goes through `ICommands::SetUnitToIdleForNewOrder`, which asks
  `GetMustFinishActiveCommandBeforeNewOrder` (`ATankMaster` -> harvester). While locked, the running harvest or
  return-cargo command keeps running. The commands queued behind it are dropped and refunded, and the new order is
  queued right behind it, so a second order replaces the first one that was waiting. A stop only drops the waiting
  orders, so the harvester goes idle after the unload. The harvester is marked (`HoldNewOrderUntilUnloaded`). When the
  unload ends for any reason, or docking fails, its next harvest step is replaced by ending the running command
  (`EndCommandForHeldOrder`), and the queue moves on to the order that waited. Shift-queued orders keep their old
  meaning (after the harvest command). System resets (`SetUnitToIdle`: towing, stuns, loading) still interrupt at once.
  Bay-less harvesters at the overflow point are never locked.
- **Progress bar.** From docking until the unload ends, the harvester fills the `ActionProgressBar` in its health bar
  (`UW_HealthBar`, optional widget, collapsed by default; the harvesters use `W_BpHealthBar_Harvester`). The fill is the
  larger of the cargo already credited and the time since docking against `GetExpectedUnloadSeconds` (the station's last
  measured unload, `ExpectedUnloadSeconds` 37 s before the first). Time alone stops at 97 %, so the bar is full only when
  the last load lands; it never goes down during one unload. Any end (empty, bin full, station gone, bay released) hides it.
- **End.** Cargo empty: the harvester goes back to harvesting while the empty grab returns. Bin full: it looks for another
  drop-off with what is left. Station gone: same.

## Tuning (BP_HQUnloadStation / C++ defaults)

`TripsPerUnload` 3, `TrolleySpeed` 380 cm/s, `HoistSpeed` 190 cm/s, `GrabSeconds` 0.45, `ReleaseSeconds` 0.35. One trip on
bay L takes about 9 s, so a full unload adds about 30 s plus docking to a harvest cycle compared with the old instant drop-off.
To make up for that, harvesters carry and cut three times as much since 2026-11-24 (`HarvesterYield` in `Harvester.h`):
cargo capacity x3 (Blueprint `SetupHarvester` values and tech capacity upgrades) and every harvest operation takes
`AmountPerHarvest` x3 from the resource (so fields run dry three times as fast in cuts). An unload still takes 3 trips,
each carrying a third of the larger cargo. See `Docs/Balance` before touching harvest numbers again.

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

Unload lock (all three stations): every harvester that docks in the first unload gets a move order right away; on the
HQ the second one gets a move then a stop. Checks: it is locked, the order is held, it keeps unloading, it stays on the
dock (drift <= 30 cm), and after the unload the move runs (after the stop, nothing does). `-UnloadStationCaptureEvery=2`
adds the first two views as `seq_NNN` frames every 2 s while a bay is docked, to review the rig frame by frame.

Progress bar (all three stations): every docked harvester must show the bar while unloading, its fill never drops and
reaches at least 0.6, and it is hidden once the harvester is empty (`unload_progress_*` checks).

## Known limits

- During the guided slide the tracks do not animate (the harvester is moved, not driven).
- Rail and posts have no collision and no nav effect: units drive through the portal feet and the spider post.
- The portal feet stand in the sandbag row next to the truck nose. Bay R used to hold sandbags (the nose pile and the
  arc of a sandbag emplacement, up to 75 cm high). They were cut out of `HQ_BuildingMesh` on 2026-11-24
  (`Tools/UnloadStation/ue_cut_hq_bayr_sandbags.py`): the emplacement now opens toward the bay, only its flat floor
  boards (<= 14 cm) remain in the bay, and bay R docks in about 10 s like bay L. The reverse stall check (moves < 25 cm
  and turns < 10 deg for 1.5 s -> guided slide) stays as the fallback for anything else in a bay.
- Only the Strike Division HQ and refinery have stations (the refinery's knuckle boom: `RefineryUnloadCrane.md`).
  Other drop-offs (harvester hub, other factions) are unchanged.
