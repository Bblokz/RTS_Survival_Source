# Ore Mountains lookout towers (Schachtwarte)

The two fire lookout towers of the Ore Mountains mission (campaign mission 2). The look is concept D "Schachtwarte",
approved by the owner on 2026-11-18. It is a small pre-war tin-shaft headframe with two sheaves and a grating deck at
12 m, plus a roofless stone hoist house to the east.

Owner decisions (2026-11-22/23) that this follows:
- there are 2 towers;
- infantry capture them like a capture point;
- once taken they stay the player's, with no garrison;
- the AI never retakes them.

Design and lore are in `Docs/Campaign/OreMountainsMission.md`, section "Fire lookout towers".

Status 2026-11-23: imported and Blueprint built. Not placed in a map. Not play-tested.

## Assets

All under `/Game/RTS_Survival/Blueprints/Environment/OreMountains/LookoutTowers/`.

| Asset | What |
|---|---|
| `BP_OreLookoutTower` | the tower; child of `ALookoutTowerCaptureActor` (C++, child of `ACaptureActor`); **no graph** |
| `Meshes/SM_OreLookout_Schachtwarte` | the whole site, 9,372 tris, LODs 9,372 / 4,686 / 2,342, 13 UCX hulls, 13 sockets, Nanite off |
| `Meshes/SM_OreLookout_Sheave` | one sheave wheel, pivot on the hub, turns about local X, no collision (2 LODs) |
| `Meshes/SM_OreLookout_HeldPanel` | the orange tarpaulin hung from the deck rail while the tower is held; pivot on the top rail; no collision |

No new materials. The slots use existing instances:

| Slot | Material |
|---|---|
| Steel | `AditPortals/Materials/MI_OreAdit_Steel` (rusty riveted steel) |
| Rope | `A4Viaduct/MI_A4_RebarRust` |
| Stone | `A4Viaduct/MI_A4_Granite` (ashlar courses) |
| StoneTrim | `A4Viaduct/MI_A4_Rubble` (wall caps, lintels) |
| Concrete | `A4Viaduct/MI_A4_Concrete` (collar cap, footings, floor slab) |
| Panel | `AditPortals/Materials/MI_OreAdit_Tarp` (orange tarp) |

Source model: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreLookoutTowers/Model/` (README
there; the FBX files were used as exported, nothing in Blender was changed).

## Blueprint components

Axes: X north, Y east. The pivot is the collar centre on the ground.

| Component | Setup |
|---|---|
| `DefaultSceneRoot` | static |
| `TowerMesh` | the site mesh, `BlockAll`, affects navigation (the UCX hulls cut the navmesh and stop units) |
| `SheaveAxle` | scene component on the axle (0, 0, 1340), movable, tag `LookoutSpin` |
| `Sheave_N` / `Sheave_S` | children of the axle at (±70, 0, 0), unrotated, no collision |
| `HeldPanel` | at the `HeldPanel` socket (-243, -60, 1310), unrotated, no collision, tag `LookoutHeldPanel` |

The sheave and panel meshes are built in the main mesh's axes, so they sit at their sockets with zero rotation (the
sockets carry yaw 90).

The cursor takes the tower by its simple collision:
- the footprint hull over the collar and leg feet, up to 3.6 m;
- the hoist house walls, up to 4.2 m.

The upper steel and the deck are not clickable. A click volume up there was tried and dropped: ground finding for
drops, abilities and decals also traces `Visibility`, and a 15 m box would have made a roof over the headframe.

At BeginPlay, C++ also sets the tower mesh to block shells (`Projectile`) and every player's weapon traces, so the walls
are cover. `BlockAll` alone ignores those custom channels.

## What C++ does (`ALookoutTowerCaptureActor`)

The code is in `Source/RTS_Survival/Environment/LookoutTower/LookoutTowerCaptureActor.h/.cpp`.

**Capture**
- Infantry capture it as an `ACaptureActor`: the default is 2 units and 20 s, with the text "Manning the lookout...".
- Capturing units walk to the sockets containing `entrance` (`entrance_01`, outside the hoist house door).
- When a capture starts, the tower locks: `GetCaptureUnitAmountNeeded` returns 999 from then on. Further capture
  orders are refused (the order checks the squad size first), so no units are lost on a tower that is already taken.
- The tower cannot be destroyed: it cannot be damaged and has 10,000,000 health.

**On capture** (`OnLookoutCaptured(Player)`; fires once)
- shows the components tagged `LookoutHeldPanel`;
- starts the spin of the components tagged `LookoutSpin` (12 deg/s about X; the actor ticks only from then on);
- for a human player only:
  - reveals the fog of war for good. It uses a permanent `AGlobalAbilityVisionBubble` (a `UFowComp` with
    `Fow_Active`) at the `Reveal` socket (15 m up), radius `RevealRadius`;
  - turns on the convoy warnings;
- broadcasts `OnLookoutTowerCaptured(Tower, Player)`.

`ACaptureActor` reports a completed capture only to Blueprint (`BP_OnCapturedByPlayer`). The tower therefore runs its
own timer of the same `CaptureDuration` in `OnCaptureByPlayer`. The base has no cancel path today, so the two cannot
drift. If a cancel is ever added to `ACaptureActor`, give it a native completion hook and call `OnLookoutCaptured` from
there instead. `BP_OnCapturedByPlayer` still fires and is free for extra Blueprint logic.

**Convoy warnings**
- At BeginPlay every tower binds to `AEnemyConvoyRoute::OnConvoyStarted` of its watched routes: the routes in
  `WatchedRouteIds`, or every placed route when that list is empty.
- It counts every convoy those routes start, whether the tower is held or not.
- While a human holds the tower, each start broadcasts
  `OnConvoyWarning(Tower, ConvoyIndex, CargoType, Route, Convoy)`:
  - `CargoType` is `EConvoyCargoType` (Fuel, VehicleParts, Ammo, Concrete). It is the convoy definition's main
    cargo (`Cargo.CargoType`). Truck specs may override it per truck, and at the start event the trucks are still
    spawning, so for a mixed column (convoy 3's ammo trucks) ask `Convoy` later (`GetTruckCargoType`,
    `GetCargoOutcome`);
  - `ConvoyIndex` is the number of convoys the watched routes started since the level began, this one included (1 = the
    level's first convoy). The mission uses one route per convoy (`ORE_Convoy1..6`), so with an empty
    `WatchedRouteIds` it matches the mission's convoy number as long as every convoy is started in order. `Route`
    (`GetRouteId`) names the convoy without relying on the order.
- With both towers held, `bOnlyFirstHeldTowerWarns` (on by default) lets only the tower taken first broadcast. The
  mission can bind both towers and still gets one warning per convoy. Turned off, each held tower warns.

**Statics for missions**
- `GetAllLookoutTowers(WorldContext)`;
- `GetIsAnyLookoutTowerHeld(WorldContext, Player)`, for example for the "sealed cargo" mark on convoy 5.

**BlueprintPure**
- `GetIsHeld`;
- `GetHoldingPlayer` (INDEX_NONE until taken).

## Settings (`LookoutSettings`, class defaults or per placed tower)

| Setting | Default | Use |
|---|---|---|
| `RevealRadius` | 9000 cm | fog revealed around the tower for the holder; 0 = none |
| `RevealSocketName` | `Reveal` | reveal centre (falls back to the actor location) |
| `CaptureLocationSocketFilter` | `entrance` | sockets the capturing units walk to |
| `WatchedRouteIds` | empty (= all routes) | convoy routes the tower warns about |
| `bOnlyFirstHeldTowerWarns` | true | one warning per convoy with both towers held |
| `HeldPanelTag` / `SpinTag` | `LookoutHeldPanel` / `LookoutSpin` | component tags the C++ looks for |
| `SpinDegreesPerSecond` | 12 | sheave turn rate while held; 0 = still |

`CaptureSettings` (from `ACaptureActor`): `AmountCaptureUnitsNeeded` 2, `CaptureDuration` 20 s,
`CaptureProgressBarScale` 1.

## How the mission Blueprint uses it

1. On the stage that starts the tower content (or at mission start), call `GetAllLookoutTowers`.
2. For each tower, `Bind Event to OnConvoyWarning`. Optionally also bind `OnLookoutTowerCaptured`, for a "lookout
   manned" Advisor line or an objective tick.
3. In the warning event, switch on `CargoType` and play the Advisor line naming the cargo ("Lookout reports a fuel
   column on the road"). Use `ConvoyIndex` or `Route → GetRouteId` to add the special case for convoy 5 ("sealed
   cargo").
4. Convoy stages that want the tower mark before the convoy starts can call `GetIsAnyLookoutTowerHeld(self, 1)` in their
   own pre-start warning. The tower event itself fires when the column spawns (`OnConvoyStarted`). For an earlier
   warning, the mission plays its line before `StartRoute`, as the 240 s stages already do.

## How to place

- Place `BP_OreLookoutTower` with **yaw 0**: the door, ladder, panel and deck lookouts face south, towards the default
  camera.
- Pick a fairly broad summit. The site is about 21.5 x 13.8 m: X -360..1020 cm, Y -240..1907 cm, with the hoist house
  to the east (+Y).
- Walls and footings run 50 cm below the pivot, so seat the pivot on the ground at the collar. Don't seat it by actor
  bounds.
- Keep the area south of the door clear and reachable: `entrance_01` is at (-420, 1350, 0).
- Rebuild navigation after placing.
- For two towers on different route sets, fill `WatchedRouteIds` per placed tower.

## Rebuilding

The scripts are in `Source/Tools/OreLookoutTowers/`. All run headless; `run_ue_py.sh <script> <log> [DebugGame]`
writes its logs to `Saved/OreMountains/lookout/`.

1. `ue_lookout_import.py` (Development is fine):
   - uses the legacy FBX importer, so the UCX hulls survive;
   - deletes and re-imports the three meshes;
   - sets LODs, assigns the existing materials, fixes the socket yaws;
   - checks bounds, hulls and sockets against `OreLookout_Schachtwarte_sockets.json`.

   The FBX holds centimetre data in a metre-unit file, so the import uses uniform scale 0.01. Delete the old mesh before
   reimporting; a reimport over it ignores the scale.
2. `ue_lookout_bp.py` (`DebugGame` until the Development DLL has `ALookoutTowerCaptureActor`):
   - deletes and rebuilds `BP_OreLookoutTower`;
   - places the components from the mesh sockets.
3. `ue_lookout_check.py` (`DebugGame`): fresh-load check in its own process; it saves nothing. It checks:
   - the class defaults;
   - the component templates: collision, navigation, tags, places;
   - that the hull boxes lie inside the mesh;
   - a spawned tower: `OnLookoutCaptured(1)` twice gives held, holder 1 and one captured event;
   - that the hull boxes are at the model's scale (top 420 cm).

Last run 2026-11-23: RESULT OK.

## Not verified / known limits

**Not exercised headless.** BeginPlay never runs headless, and line traces get no hits in the commandlet world. The
check called `OnLookoutCaptured` directly, so the real capture path (`OnCaptureByPlayer` and its timer) never ran. None
of these has been checked:
- runtime collision: units blocked, shells stopped, the cursor click;
- the navmesh cut;
- the capture walk to the door;
- the panel hide/show and the spin;
- the fog reveal radius;
- the convoy warning;
- the look: the materials were picked by name and never rendered on this mesh.

**Needs a rebuild.** The Development DLL (2026-11-22) has no `ALookoutTowerCaptureActor`. A Development editor shows the
Blueprint with a missing parent until Development is rebuilt.

**Held-tower capture feedback.** On a held tower:
- the hover text reads "Capturable: 999 units required";
- a capture order plays the "not enough squad units" announcer line.

A generic "can be captured" hook on `ACaptureActor` / `ICaptureInterface` would fix both; it is shared code, so that is
the owner's call.

**Socket yaws.** The legacy importer read the socket yaws wrongly (0 / -26.6). The import script sets them from the
Blender report (180 for the seats and the door, 90 for the sheaves, panel and reveal). Only a later `UCargo` garrison
would use the `cargo_*` seats; the owner chose no garrison.
