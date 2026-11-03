# German Flakvierling bunker (breakthrough doctrine)

An open concrete gun emplacement: a 2 cm Flakvierling on a raised dais behind a chest-high parapet and laid sandbag walls, with a Ringstand beside it where a scavenger gunner works an MG34. A camo net tents over the ready ammo lockers.

| | |
|---|---|
| Blueprint | `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/FlakvierlingBunker/Ger_BXP_FlakvierlingBunker` |
| Expansion type | `BXT_GerFLakBunker` for now, so it has stats. See [Not done](#not-done). |
| Turrets | `FlakvierlingBunker/RTS_BP_EmbTurret_BF_Flakvierling` (child actor: the 4x20mm turret with a crew), `Ger_EIT_MG34_Scavenger` (runtime mount) |
| Size | 8.9 x 9.5 m above ground, parapet 1.25 m, sandbags to about 1.7 m |
| Triangles | 4.2k main mesh, 10.0k sandbags, 6.8k ammunition |

Blender source, design history and the turret fit numbers: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerBreakthroughBuildings/FlakvierlingBunker` (`FlakvierlingBunker_Brief.md`, `Model/README.md`).

Import: `Tools/GerPits/ue_flakvierling_bunker_import.py`. It is safe to rerun after a Blender rebuild.

## Assets

| Asset | |
|---|---|
| `FlakvierlingBunker/Mesh/SM_BF_FlakvierlingBunker` | Main mesh: 12 UCX hulls, sockets `Turret`, `MG`, `Exp1-3` |
| `FlakvierlingBunker/Mesh/SM_BF_FlakvierlingBunker_Sandbags` | All sandbags, on `M_GerSandBagScaffold_Atlas`, no collision |
| `FlakvierlingBunker/Mesh/SM_BF_FlakvierlingBunker_Ammo` | MG can stack in the chamber's back corner, no collision; shared by both health states |
| `FlakvierlingBunker/Mesh/SM_BF_FlakvierlingBunker_Damaged` | 50 % health mesh: same pivot, sockets, UCX and slots; breaches with rebar, chunks, rubble, torn net |
| `FlakvierlingBunker/Mesh/SM_BF_FlakvierlingBunker_Sandbags_Damaged` | 50 % health sandbags: blown gaps and bags knocked onto the ground |
| `BreakthroughShared/MI_BF_SootedFortress_UVTile` | The faction concrete at `BF_Tiling` 1 |
| `BreakthroughShared/M_BF_Trim`, `M_BF_CamoNet`, `M_BF_FieldSupply` | Trim, two-sided masked net, field supply atlas |

`MI_BF_SootedFortress_Casting` has `BF_Tiling` 4. That is for meshes with 0-1 UVs. Breakthrough production meshes hold the 4 m tile in their UVs and need `MI_BF_SootedFortress_UVTile`, or the concrete comes out four times too small.

## Blueprint

A copy of `Ger_BXP_FlakBunker`. What changed:
- `Flak37` became `Flakvierling`; `Flak88`, `Cargo` and `GeometryCollection` are gone, with their nodes. The mesh has no `cargo` sockets, and `SetupCargo` reports an error without them.
- `BuildingMeshComponent` and both `BP_InitBuildingExp` meshes use the new mesh.
- The damage-level `Set Material` nodes all set the concrete instance, so damage changes nothing visually yet.
- Death goes straight from `ExplosionsAtDeath` to the decals and `VerticalDestruction`: there is no destruction mesh.
- `HideSandBags` also hides `AmmoStack`, its child.
- `Exp1-3` on the new mesh keep the three explosion functions unchanged.
- `ShrapnellRounds`, the flak bunker's ability, is kept on purpose.

Child actor at (-130, -100, 115), the `Turret` socket. Bunker front is +X.

### The 50 % mesh swap (2026-11-03)

As on the Flak 88 bunker ([GerFlak88Bunker.md](GerFlak88Bunker.md#the-50--mesh-swap)):
- `Health.HealthLevelsToNotifyOn` gained `Level_50Percent`, so it is now 100, 66, 50 and 33 %. Only 50 was added: a 75 % level would be crossed first and delay the 66 % explosions by a hit.
- `Bp_OnHealthChanged` calls `ApplyHealthMesh` before its existing switch. At or below `DamagedAtOrBelow` (0.5) health, `BuildingMeshComponent` gets `DamagedMesh` and `Sandbags` gets `DamagedSandbags`; above it, the intact pair. `AmmoStack` rides on `Sandbags` unchanged.
- `SetBunkerBusy(true)` on start of construction or pack-up holds the swap. `SetBunkerBusy(false)` on finished construction or cancelled pack-up releases it and applies the meshes for the current health.
- The switch's `SetMaterial` on slot 0 still sets the concrete instance, on whichever mesh is shown.

Verified in a Play session on StartGame (spawned by hand):
- both components swap at 40 % and 20 %;
- they stay intact at 60 %;
- they go back to intact on repair to 55 % and 90 %;
- in-game shots `Model/Renders/ue_intact.png` and `ue_damaged.png` in the Blender folder.

## The Flakvierling crew (2026-11-03)

The turret is a copy of `RTS_BP_EmbTurret_4x20mmFlak` with a `UGunPitCrewComponent` (`PitCrew`), the gun pits' crew system. The original turret is untouched: both HQ trucks and several test maps use it. Every animation is reused, nothing was baked:

| Crew | Spot in `PitOrigin` space (+X muzzles) | Animation |
|---|---|---|
| Crew1 loader | (-80, 80, -58), beside the right-hand breeches | the Flak 37 pit's loader: idle `Anim_GerPitFlak_Loader_Idle`, reload `Montage_GerPit_LoadFlak` |
| Crew2 gunner | (-95, -15, 0), behind the mount | the Flak team weapon's `AN_Infantry_Flak_ControlWheels` (handwheels) |

- **Loader height:** the loader's animation stands him 58 cm up, on the Flak 37's platform. His spot is 58 cm lower, so his feet land on the dais and his hands reach the upper breeches.
- **Who was left out:** the Flak 37 pit's handler, spotter and commander work 2.4–3.2 m from the gun. The chamber walls here are 1.6 m from the axis, so anything turning with the mount must stay on the 1.25 m dais. No rounds change hands.
- **Wiring:** BeginPlay calls `InitPitCrew`, `PlaySingleFireAnimation` calls `OnGunFired` (the crew brace, at half strength) before each gun's own fire montage, and `ReloadWeapon` calls `OnReloadStarted`. Each 20 mm gun reloads its 20-round magazine in 10 s on its own, so the loader is busy most of the fight. `MinReloadTimeForLoadingSequence` is 2 s.
- **Sockets:** on the shared skeleton `Flak4_20mm/Skeleton/SK_Flak4_20mm_Skeleton`:
  - `PitOrigin` on `B_Root`, the bone the aim offset turns;
  - `PitStatic` on the static `Flak4` root;
  - both cancel the bones' ×44.2 import scale and their rotation, so the crew hang at 1:1 in plain gun space.
  - Their rotations were set as pitch, yaw, roll: (90, 90, 0) and (0, 90, 0). `set_skeleton_socket_transform` takes that order, not roll, pitch, yaw.

### Fixed on the way: only one gun recoiled

The four guns' fire montages play on slots `FireGun1`-`FireGun4`, and all four slots were in `DefaultGroup`. A montage cuts off any other montage in its group, so in a burst each shot cancelled the previous gun's recoil: only the last gun to fire visibly kicked. The bunker test caught it (the first gun never moved). Each slot now has its own group, `FireGun1Group`-`FireGun4Group`, on the shared `SK_Flak4_20mm_Skeleton`. That also fixes the HQ trucks' Flakvierlings, which use the same skeleton and montages.

### Testing

`-FlakvierlingBunkerValidate` on TestCover (`GunPitTestScenario`, run like the Flak 88 bunker's test in [GerFlak88Bunker.md](GerFlak88Bunker.md#testing)) checks:
- the crew sit on `PitOrigin` at 1:1;
- the mount stands 115 cm up on the dais;
- the mount and crew turn to a ground target;
- after a shot, gun 1 kicks (`FallbackKickBone` `B_Gun1`, since the mount has no gun-pit anim instance) and the gunner braces;
- the loader's `Montage_GerPit_LoadFlak` plays while a magazine is changed.

No rounds change hands, so the harness's montage drill (`LoaderReloadMontage`) replaces the round drill.

## The MG gunner

`MGGunnerMount` (`UEmbeddedTurretMountComponent`) mounts `Ger_EIT_MG34_Scavenger` on socket `MG` with offset (0, 0, -140), which puts his feet on the concrete firing step in the ring at z 35.

`Ger_EmbTurret_MG34` does not work here. It is a hull gun: ±30° yaw, ±20° pitch, assault turret. In the ring it would cover a 60° sector.

## Verified

In a Play session on `UnitTests/RTS_ToCopySmall`: the actor spawned with the right mesh and materials, the Flakvierling child actor was present, and the gunner mounted at (280, 250, 35). No errors from the bunker in the log. The Blueprint compiled with no errors or warnings.

The game starts paused on that map, so timers do not run until `SetGamePaused(false)`. The mount waits for a timer.

**Not verified:** firing at a target, auto-engage, the damage explosions and death, navigation blocking (`LetBuildingMeshAffectNavMesh` is false and `NavModifier` is inherited from a 19 m bunker), placement through the build menu, and the look of the materials in Unreal.

## Not done

It is not in the HQ's build menu. The rail Pak bunker is the pattern for that:
- its own `EBuildingExpansionType` (`BXT_QuadFlak` exists and has no data);
- data in `BxpDataHelpers.h` and footprint rules in `BxpConstructionRulesHelpers.h`;
- the mapping in `RTS_BP_RTSAsyncSpawner`;
- a slot in `TrainingLayoutDataAsset.cpp` and a row in `DT_TrainingOptions`.

Those need a C++ rebuild. Until then the bunker shares the old flak bunker's stats and cost through `BXT_GerFLakBunker`, and must be placed in a level by hand.
