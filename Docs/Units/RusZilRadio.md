# Soviet ZIL radio truck (Oder-Warthe decoy transmitter)

This is the enemy radio truck that drives the roads behind the Oder-Warthe bunker line on Hard and above. It sends
fake signals and gives the player decoy bearings. Destroying it is the bonus objective "Destroy the radio truck".
For the mission design, see `Docs/Campaign/OderWartheMissionFlow.md`, sections 9 and 11.

The truck is the ZIL cargo truck with these additions:
- A closed radio shelter box on the bed.
- An 11.5 m four-leg lattice mast on the rear platform, with a T dipole on top and guy lines.
- A tied-back whip on the front roof corner.

The mast and dipole are what read from the RTS camera. On 2026-11-22 the user picked concept C of six (A-F), on the
ZIL hull.

## Assets

Folder: `/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/ZilRadio/`

| Asset | What it is |
|---|---|
| `Rus_ArmoredCar_ZilRadio` | Vehicle Blueprint. It is a duplicate of `Rus_ArmoredCar_ZilFuel` without the six barrel meshes, and its parent is `RTS_BP_TrackedTankMaster`, like every Zil truck. The hull is `SM_Hull_ZilRadio`. On death it spawns `Env_Dest_ZilRadio`. |
| `Env_Dest_ZilRadio` | Wreck Blueprint. It is a duplicate of `Env_Dest_Zil_Fuel` without the barrels, and its hull is `SM_Destroyed_ZilRadio`. |
| `Meshes/SM_Hull_ZilRadio` | Live hull. Its material is `M_Zil_Body`. Complex collision is `SM_Collision_Hull_ZilRadio`, used as simple collision. It keeps the Zil sockets (Exhaust, EngineFire, Exp1, Turret). |
| `Meshes/SM_Destroyed_ZilRadio` | Wreck. Its material is `MI_Zil_Destroyed`. The mast lies about 10 m out to the truck's left, as visual geometry only. |
| `Meshes/SM_Collision_Hull_ZilRadio` | The Zil hull collision plus the shelter box. The mast has no collision. |

The FBX reimport added a convex hull to the hull and the wreck. It was removed (`zr_04_strip_simple.py`), so both
meshes match the Zil originals:
- The hull has no simple collision and uses complex-as-simple through the collision mesh. Weapons aim at the
  inherited `AimOffsetPoints` (about 1 m up on the body), not the mesh bounds, so the tall mast does not pull shots
  off target.
- The wreck has no simple collision either. Its NavModifier (`RTS_NavArea_BlockCharLight`) therefore keeps the Zil's
  346 x 150 x 150 failsafe box around the truck, and the fallen mast blocks nothing.
| `DA_Convoy_RadioTruck` | Convoy definition for `AEnemyConvoyRoute`: one radio truck, three guard tiers with no escorts. It is a duplicate of `DA_Convoy_Fuel`. |

The chassis, wheels, wheel animations, driver, engine sound and armour are the Zil truck's, unchanged.

All new faces use the ZIL atlas, so no new textures or materials were made. The source and build scripts are in
`D:/Blender/BlenderProjects/BlenderTanks/Trucks/ZilRadio/` (see `README.md` there).

## Unit data

It runs as `Tank_ZilFuel`. There is no new `ETankSubtype`, so no C++ changed. The cost of this:
- Health, speed, armour and cost are the Zil fuel truck's: 0.8x armoured car health (215 HP in the test), 55 km/h.
- Logs and the convoy's member names call it "Zil Fuel".

A dedicated subtype (`Tank_ZilRadio`) would fix the name and allow its own health. It is a follow-up.

Neither Zil truck has a row in `DT_TrainingOptions` or `DT_Items`. Soviet units are enemy-only, so no icon or
portrait was made.

## Driving it in the mission

- Place an `AEnemyConvoyRoute`.
- Set `Definition = DA_Convoy_RadioTruck` and set the route points.
- To make it loop the roads, use `Repeat` set to `Infinite` (the design asks for an `AEnemyConvoyRoute` loop).

Two things for the scripter:

- When the convoy reaches its exit point, the truck escapes and despawns. Repeat convoys bring a new truck. For
  "Destroy the radio truck", listen to `OnConvoyFinished` and check the outcome `TrucksDestroyed`, or count with
  `GetDestroyedTruckCount`.
- `DA_Convoy_RadioTruck` keeps the fuel convoy's cargo cache class (`BP_ScavObj_RadixiteDump`). The test convoy
  logged `open_caches=1` after the kill, so the kill appears to drop that cache (its cargo total was 0). If a radio
  truck should drop nothing, change the definition's `Cargo` settings.

## Test

The test ran on 2026-11-22: `RESULT PASS (8/8)`.
- Script: `Saved/AgentScripts/ZilRadioUE/zr_02_pie_test.py`.
- Setup: a throwaway Development editor, run headless with `-nullrhi`, with UECP disabled. The map was
  `UT_EnemyAI`.
- The script spawns an `AEnemyConvoyRoute` with `DA_Convoy_RadioTruck` in PIE through
  `MissionMapTestLibrary.MapTest_SpawnActor`.

Checks:
- The convoy spawned `Rus_ArmoredCar_ZilRadio_C` for player 2 (enemy).
- The hull is `SM_Hull_ZilRadio`.
- The truck drove along the route (+X, 10 m in 9 s, including forming).
- 1e6 damage removed the truck.
- `Env_Dest_ZilRadio_C` with `SM_Destroyed_ZilRadio` stood at the death spot.
- The route counted 1 destroyed and 0 escaped.

An earlier run let the truck reach the exit: it formed, cruised 30 m and despawned as escaped.

The collision strip was done after the PIE run. It does not touch the tested path: the live hull ignores simple
collision, and the death spawn is unchanged.

Not tested:
- In-game rendering. The GPU was full of other editors, so there are no pictures from Unreal. The previews are
  Blender Workbench renders.
- Combat against player units.
