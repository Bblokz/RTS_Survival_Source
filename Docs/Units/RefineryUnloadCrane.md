# Refinery unload crane (knuckle boom)

The Strike Division refinery (Opel Blitz) unloads harvesters with a knuckle-boom loader on a portal over the truck nose.
A harvester reverses into the single bay in front of the nose. The boom picks the radixite out of its hopper, slews
round the open +Y side (folding up on the way), and drops it onto the truck bed, where the bed's storage-level mesh fills
as before. A "+ amount" pop-up shows over the bed on every landed load.

It reuses the HQ crane's station logic (`Docs/Units/HQUnloadCrane.md`): bay reservation, reverse-in, chunked crediting,
and what happens when the harvester leaves or the refinery packs up are all the same.

Design history: `_AssetSource/RefineryUnloadCrane/`. A rail round (`build_refinery_monorail.py`) was rejected, then six
nose cranes (`build_nose_cranes.py`); option 4, the knuckle loader, was picked on 2026-11-24.

## Pieces

| What | Where |
|---|---|
| Station actor | `RTS_Survival/Resources/UnloadStation/KnuckleCraneUnloadStation` (`AKnuckleCraneUnloadStation`, child of `AResourceUnloadStation`) |
| Overflow point | `AResourceUnloadStation::bUseOverflowDropOffPoint` / `OverflowDropOffPoint`, read by `UResourceDropOff::GetDropOffLocationNotThreadSafe` |
| Meshes + Blueprint | `/Game/.../ChildBlueprints/GerEco/OpelBlitz_Ref/UnloadCrane/` (`SM_RefUnload_*`, `BP_RefineryUnloadStation`); rope, grab, jaws, crystal and ingot are the HQ's `SM_HQUnload_*` |
| Model, clash sweep, renders, FBX export | `_AssetSource/RefineryUnloadCrane/build_knuckle_crane.py` (`-- render export`) |
| Import + hook-up | `Source/Tools/UnloadStation/ue_import_refinery_crane.py` (headless, DebugGame) |
| Test | `UnloadStationTestSubsystem` with `-UnloadStationValidate -UnloadStationTruck=Refinery` |

## How it works

- **The boom tip is the trolley.** The station's rail is an open spline, the path the tip follows: from the hopper
  (611, 13) round the slew pivot (220, 13) through the +Y side to the bed (-176, 13), at tip height 500. The path pulls in
  to 2.55 m reach halfway, so the boom folds up while it slews. Rope, grab, jaws and payload hang from the tip exactly as
  from the HQ trolley.
- **Rig.** Every tick the turret yaws towards the tip, then a two-link solve (knuckle up) places boom 1 (2.2 m, shoulder
  pin 4.3 m up) and boom 2 (3.0 m). Each ram's cylinder sits on its eye and points at the rod's eye, and the rod points
  back, so the pair telescopes. The rig numbers live in both the C++ defaults and `build_knuckle_crane.py`; change them
  in both.
- **Bay and overflow.** The one bay sits on the old drop-off point, so a harvester that finds the bay taken would drive
  into the docked one. The station therefore has an overflow point at (1000, -1050), on the side the boom never swings
  over, and the drop-off hands that out as its drop-off location. That harvester unloads instantly the old way, as a
  third harvester does at the HQ. The path follower stops up to about 4.6 m short of the point (acceptance plus agent
  radius), which is why the point is 11 m from the docked hull instead of right beside the bay, and why
  `UHarvester::OnMoveToDropOffFinished` counts a successful path to an overflow point as arrived (its 3D 2.5 m check
  would fail and send the harvester to the backup spot, which can lie in the bay).
- **Cargo.** The refinery only has a radixite bin (socket `Radixite` on the bed). Metal harvesters never reserve the bay
  and go to the overflow point.
- **Idle.** The boom waits halfway along the path, folded up over the open side.

## Tuning (BP_RefineryUnloadStation / C++ defaults)

`TrolleySpeed` 260 cm/s along the 10.7 m tip path (a slew of about 4.5 s each way), `HoistSpeed` 190 cm/s, three trips per
unload, `DropHeadAboveBin` 150. In the headless test a full unload takes about 50 s from docking to the last load (the
HQ crane takes about 37 s); raise `TrolleySpeed` to shorten it. No balance numbers changed; see `Docs/Balance` before touching harvest numbers.

## Clearances

`build_knuckle_crane.py` checks the whole slew path every 1/24 of its length: booms, rams, rope, grab and a hanging load
against the refinery mesh, the docked Pz IV harvester and the portal. Nothing touches. The turret only sits on its own
slew ring. The portal legs stand at (228, -142) and (228, 165), beside the hood, in front of the sandbag wall's gap.

## Headless test

```bash
MSYS_NO_PATHCONV=1 UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -game -unattended -nosound -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate -UnloadStationValidate -UnloadStationTruck=Refinery -abslog=<file>
```

Spawns an unpacked `Ger_Refinery_Truck` and two Pz IV harvesters carrying radixite (each half of the refinery's free
storage, at most 450). Checks: exactly one docks (within 1.2 m and 12 deg) and is unloaded by the crane, the other
unloads within 5.5 m of the overflow point and at least 4.5 m from the docked one, exactly the cargo is credited, in at least 4 store
changes, and the bay is released. Then the pack-up check from the HQ test: the refinery packs up while a load is in the
air, and cargo on board + credited must equal what was carried. Pictures: drop `-nullrhi`, add `-RenderOffscreen
-UnloadStationCapture=<folder>`.

## Known limits

- Same as the HQ: tracks do not animate during the guided slide; the crane has no collision and no nav effect.
- The station pops in once the deploy finishes and vanishes on pack-up; there is no folded transport pose on the
  driving truck.
- If the path to the overflow point fails or is blocked, the harvester still falls back to the drop-off's backup spot
  on the building side facing it, which can lie in the bay.
- The nearest-drop-off search (`UGameResourceManager`) measures to the overflow point, 11 m from the truck nose, so it
  slightly favours other drop-offs for harvesters on the far side.
- Only the Strike Division refinery and HQ have cranes.
