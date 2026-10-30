# 30 — Start locations and player start content

Owner: start-location agent. Code: `Source/RTS_Survival/SurvivalMode/StartLocations/`.
Content: none yet (`Content/RTS_Survival/SurvivalMode/PCG/StartLocations/` is intentionally empty, see "Why no PCG").
Data asset instance: `Content/RTS_Survival/SurvivalMode/Data/DA_SurvivalStartContent` (create after the C++ compiles).

## What a start location is

`ASurvivalStartLocation` is placed on the flat ground where the HQ lands.

| Concept | Where | Notes |
|---|---|---|
| Deploy centre | `GetDeployCenter()` = actor location | HQ lands here; vehicle rings (1100 cm spacing) and infantry (+900) land around it. |
| Build area | `BuildAreaRadius` (default 6000 cm) | Flat disc for buildings. Nothing from the start content is placed inside it except pits on defense points. |
| Defense points | `GetDefensePoints()` (world positions, priority order); `GetDefensePointDetails()` adds the origin | Resolved in `BeginPlay` for **every** start location (the Soviet-base agent may use them on the unpicked ones) and in the editor. |
| Designer points | `DesignerDefensePoints` (`MakeEditWidget`, actor-relative) | Drag the diamond widgets onto hill tops / ramp heads. Snapped to the landscape height. |
| Hill-top scan | `DefensePointSearch` | Polar grid of landscape heights from `0.5 × BuildAreaRadius` to `BuildAreaRadius + 4500`, every 400 cm × 6°. A sample is a hill top when it is ≥ 300 cm above the deploy centre, no 8-neighbour is higher (plateaus count) and the top is flat (≤ 25° over 300 cm). Picked highest first (on a plateau: the edge nearest the base), ≥ 2500 cm apart, up to 8 points in total. |
| Source | `DefensePointSource` | `DesignerThenAuto` (default), `DesignerOnly`, `AutoOnly`. |
| Fallback | `FallbackBuildEdgePointCount` (4) | Only when no designer point and no hill top exists: evenly spaced points on the build edge, so pits and Soviet defenses always have somewhere to go. |

`PCGFindEnemyDefensePoints` is **not** reused: it takes candidate points and an "EnemyBase" spatial input and
selects a rectangular symmetric formation; it never looks at terrain height, which is what "hill tops / ramp
heads" needs.

### Editor visualisation (select the actor, or tick `bAlwaysDrawDebug`)

- Green ground-following ring: build area edge; green vertical line: deploy centre.
- Yellow / red / grey spheres on poles: designer / hill-top / fallback defense points.
- With `PreviewContentSettings` set: cyan rings = resource band (inner edge, outer edge), orange ring = outermost
  reach of wrecks / clutter.
- Button **Preview Start Content**: runs the real planner with `PreviewContentSettings` + `PreviewDifficulty`
  and draws every planned field (circles), actor footprint and facing arrow (radixite green, metal grey,
  wreck orange, pit red, crater brown). Nothing is spawned. Re-plans with the same seed on every move/edit;
  **Clear Start Content Preview** removes it.
- Button **Refresh Editor Preview**: re-scan after landscape sculpting (the actor does not see landscape edits).
- Button **Bake Defense Points Into Designer Points**: copies the resolved points (hill tops included) into the
  designer widgets for hand tweaking.
- `bDrawDebugInGame`: keeps drawing in PIE, including the layout that was actually generated.

The drawing uses an editor-only tick (`ShouldTickIfViewportsOnly`) with `DrawDebug*` (compiled out in Shipping);
the landscape is only sampled when something changed (throttled to 0.3 s while dragging).

## What happens when the player picks a location

`ASurvivalModeDirector::GenerateStartLocationContent` (Deploying phase) calls
`GeneratePlayerStartContent(Settings->StartContent)` on the chosen location only; the others get Soviet bases.

1. Defense points are re-resolved.
2. `FSurvivalStartContentPlanner` builds a plan synchronously (landscape height queries/traces only):
   1. **Pits** on defense points in priority order (designer → hill top → fallback), skipping points steeper than
      30° or closer than 2000 cm to another pit; facing away from the deploy centre.
   2. **Protected areas** are reserved: build disc, a 900 cm disc around every defense point, and the build area
      of every other start location + 3000 cm (Soviet bases go there; no player content).
   3. **Resource fields**, radixite and metal interleaved on evenly spaced angular slots (random start angle) so
      both resources are on every side. Each field searches around its slot (±12° steps, then further out) for
      a free disc whose centre and 8 ring samples are on landscape, not steeper than the field's max slope and
      without a cliff inside. Field = one centre actor + N field actors spread in the disc.
   4. **Wrecks** per wreck group, random in their band, ≥ `MinSpacing` apart, outside fields/protected areas.
   5. **Craters** (decal actors) and **foliage** (instanced meshes, no collision, no navigation) in the clutter
      band, outside everything else.
3. All soft classes/meshes of the plan are loaded in one `UAssetManager::GetStreamableManager().RequestAsyncLoad`
   (high priority); on completion every actor is spawned with `AlwaysSpawn` and tracked in
   `GetSpawnedStartContent()` (weak). Foliage becomes one `UInstancedStaticMeshComponent` per mesh on the
   start location.
4. Shortfalls are reported: no content at all → `ReportError`; fewer fields than requested → `ReportWarning`;
   classes that failed to load → `ReportError` with the paths.

Difficulty comes from `ASurvivalModeDirector::GetDifficulty()`; seed 0 in the data asset = new layout every
match (time ticks hashed with the location name); any other seed = the same layout every time (testing).

## Why direct C++ spawning and not a runtime PCG graph

Decision (b). The PCG field nodes would have been the natural reuse, but at runtime in a packaged build:

1. `UPCGCreateRadixiteFieldSettings` / `UPCGCreateMetalFieldSettings` have no `PCG_Overridable` properties, so
   neither graph user parameters nor attribute overrides can drive counts, radii or actor lists from a data
   asset without editing those nodes (outside this folder). Their logic lives in anonymous namespaces, so it
   cannot be called from C++ either.
2. The only runtime-parameter precedent (`ARTSLandscapeDivider::SetupPcgGraphParameters`) writes parameters
   into the shared graph **asset**; doing that from several start locations / PIE sessions is unsafe.
3. `UPCGComponent::CreateGenerateTask` aborts silently when the owner has no primitive bounds — a bare
   scene-root actor generates nothing.
4. The project documents a runtime PCG hang when a branch receives empty point data
   (`ARTSLandscapeDivider::EnsureResourceRegionsNonZero`); a start location on bad terrain can produce exactly that.
5. The campaign's scavenge proxies (`BP_Prx_*`, `ARTSActorProxy`) require an `ARTSLandscapeDivider` and destroy
   themselves without one; survival maps have none. The `BP_Async_*_ScavObj` loaders add a 2 s delay.
6. The radixite/metal nodes are only used by the editor-generated world campaign graph (`PCG_WorldCampaign`);
   the runtime-proven campaign path (`PCG_CreateSmall*` subgraphs) is plain `Spawn Actor` of the same Blueprints.

Direct spawning keeps generation synchronous to plan, async only for loading, debuggable in the editor preview,
and independent of PCG scheduling. What is reused: the exact Blueprints the campaign PCG nodes and subgraphs
spawn, the landscape-only ground projection of the radixite/metal nodes (`GetHeightAtLocation` +
`ActorLineTraceSingle` on the proxy), the async-load-then-spawn pattern of `ARTSActorProxy`, and the
capture/scavenge Blueprints as they are. No custom PCG node or graph was needed.

## Data asset `USurvivalStartContentSettings` — defaults (Normal difficulty)

Distances are measured from the **build area edge** (add `BuildAreaRadius` for distance to the deploy centre).
The constructor fills all lists, so a new asset works immediately.

| Setting | Radixite | Metal |
|---|---|---|
| `FieldCount` | 2 | 2 |
| `GapFromBuildEdge` (build edge → field edge) | 600 | 600 |
| `MaxExtraOutwardOffset` (random push out) | 1200 | 800 |
| `FieldRadius` | 1800 | 2600 |
| Field centre distance from build edge | 2400 – 3600 | 3200 – 4000 |
| Outer field edge from build edge (max) | 5400 | 6600 |
| `CentreActors` | growth crystal | metal tower (w 1) / ruin (w 0.6) |
| `FieldActors` per field (`Min`–`Max`) | 5 – 8 crystals | 2 – 4 objects |
| `ActorClearance` | 120 | 150 |
| `MaxGroundSlopeDegrees` | 30 | 25 |

| Wreck group | Count | Band from build edge | `MinSpacing` | Max slope |
|---|---|---|---|---|
| `TankHulks` | 3 | 5500 – 9500 | 2000 | 35 |
| `VehicleParts` | 4 | 5000 – 9000 | 1200 | 35 |

| Other | Default |
|---|---|
| `Pits.PitCount` / `MinPitSpacing` / max slope / face outward | 3 / 2000 / 30° / yes |
| `Clutter` band | 0 – 9000 from build edge |
| `Clutter.CraterCount` / `FoliageInstanceCount` / foliage max slope / cull | 12 / 80 / 40° / 20000 |
| `DefensePointReserveRadius` | 900 |
| `OtherStartLocationClearance` | 3000 (added to the other location's build radius) |
| `FieldClearance` (between fields) | 400 |
| `PlacementAttemptsPerItem` | 48 |
| `GroundTraceUp` / `GroundTraceDown` | 20000 / 50000 |
| `Seed` | 0 (random) |

`DifficultyScaling` (multipliers; counts rounded; resource field count never below 1, field actors never below 1):

| Difficulty | Field count | Field actors | Wrecks | Pits |
|---|---|---|---|---|
| NewToRTS | 1.5 | 1.25 | 1.5 | 1.5 |
| Normal | 1.0 | 1.0 | 1.0 | 1.0 |
| Hard | 1.0 | 0.85 | 0.75 | 0.75 |
| Brutal | 0.75 | 0.75 | 0.6 | 0.5 |
| Ironman | 0.75 | 0.7 | 0.5 | 0.35 |

"Amount" is the number of crystals / metal objects; the yield per crystal is the Blueprint's
`UResourceComponent::TotalAmount` (see the optional integration request).

## Spawned Blueprints (verified to exist in the editor, 2026-10-29)

Footprints were measured from the Blueprints' static-mesh bounds where possible.

| Use | Class (package path) | Footprint |
|---|---|---|
| Radixite centre (grows nodes over time, `URadixiteGrowthComponent`) | `/Game/RTS_Survival/Blueprints/Resources/ChildBps/Radixite/Resource_Radixite_Crystal_LargeWithGrowth` | 600 |
| Radixite crystal | `.../Radixite/Resource_Radixite_Crystal`, `Resource_Radixite_Crystal_SmallDecal`, `Resource_Radixite_Crystal_NoDecal` | 200 |
| Metal centre | `/Game/RTS_Survival/Blueprints/Resources/ChildBps/Metal/Resource_MetalTower` (1400), `Resource_Metal_Ruin1` (650) | |
| Metal field objects | `.../Metal/Resource_Metal_Barrels` (280), `Resource_Metal_BrokenWaterTank` (970), `Resource_Metal_Rails` (620) | |
| Tank hulks (metal, radixite, weapon/vehicle blueprints) | `/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/DestroyedTanks/German/Light/PZ38T/ScavObj_Ger_Pz38T`, `.../Medium/Stug/ScavObj_Ger_Stug`, `.../Medium/PZIV/ScavObj_Ger_PZIV`, `.../Heavy/Tiger1/ScavObj_Ger_Tiger1` (weight 0.25) | 550 / 650 (estimate) |
| Vehicle parts (RESOURCE_VEHICLE_PARTS 10–25 + metal) | `/Game/RTS_Survival/Blueprints/Environment/ScavengeObjects/VehicleParts/ScavObj_VehicleParts1..4`, `.../VehicleParts/Cars/ScavObj_PostApo_Car_1..5` (weight 0.5) | 350 (estimate) |
| Capturable pits (`ACaptureActor` children) | `/Game/RTS_Survival/Blueprints/Environment/CaptureObjects/GerPits/BP_CaptureGerPit_{Flak37,Pak40,TwinMG42,Mortar,Puppchen,Panzerschreck}` (w 1), `{Flak88,LeFH18,SFH18}` (w 0.35) | 600 (estimate) |
| Capturable bunkers | `/Game/RTS_Survival/Blueprints/Environment/CaptureObjects/Bunkers/BP_CaptureBunker_37MM`, `BP_CaptureBunker_Turret` (w 0.2) | 750 |
| Craters | `/Game/RTS_Survival/Materials/Decals/CraterDirt/BP_RTS_RandomDirtCraterDecal_Sandy` (350), `BP_RTS_RandomDirtCraterDecal_Small_Sandy` (180) | |
| Foliage meshes | `/Game/RTS_Survival/Blueprints/Environment/GroundFoliage/Meshes/SM_GF_DryTuft_A`, `SM_GF_DryTuft_B`, `SM_GF_DryLitter` | – |

`ACaptureActor` is the only `ICaptureInterface` implementer; all 13 capture Blueprints in the project derive
from it (`BP_CaptureBrummbar` and the base classes are excluded on purpose).

## How a designer sets up start locations in a new map

1. Sculpt 4–6 flat plateaus (radius ≥ `BuildAreaRadius`, default 60 m) with ramps up to hills. Keep
   hill tops flat (≤ 25°) and ≥ 3 m above the plateau so the scan finds them.
2. Place one `ASurvivalStartLocation` in the middle of each plateau, on the ground. Set `DisplayName`
   (picker label) and `BuildAreaRadius`.
3. Assign `PreviewContentSettings` = `DA_SurvivalStartContent`, select the actor, check the rings and
   defense points. Drag designer points onto ramp heads the scan cannot find; switch to `DesignerOnly` if
   the scan picks bad spots.
4. Press **Preview Start Content** for each difficulty you care about; count the field circles (green radixite,
   grey metal) against the requested numbers and fix terrain or distances where fields are missing.
5. Spacing between start locations: the default content reaches 15 500 cm from the deploy centre
   (6000 + 9500), and the other locations reserve their build radius + 3000. Keep deploy centres
   **≥ 25 000 cm apart** for a full layout; below ~21 600 cm metal fields start losing room.
6. Navigation: the project navmesh is `DynamicModifiersOnly`; spawned resources/wrecks/pits must carry their
   own nav modifiers (the campaign Blueprints already work this way at runtime).

## Integration requests (outside this folder)

None required — `ASurvivalModeDirector` already passes `Settings->StartContent`, and the director headers are
only read.

Optional (later, for per-difficulty resource **yield** instead of crystal count): a scale hook on the resource
component, e.g. in `Source/RTS_Survival/Resources/ResourceComponent/ResourceComponent.h` (public section):

```cpp
	/** @brief Scales the stored amount before BeginPlay registers it (survival start content difficulty). */
	void ScaleTotalAmountBeforeBeginPlay(const float Multiplier)
	{
		TotalAmount = FMath::Max(1, FMath::RoundToInt(TotalAmount * Multiplier));
	}
```

It would be called from `SpawnPlannedActor` via a deferred spawn. Not referenced by the current code.

## Files

| File | Purpose |
|---|---|
| `StartLocations/SurvivalStartLocation.h/.cpp` | The actor: contract API, defense points, generation, async load + spawn, editor debug view and preview. |
| `StartLocations/SurvivalStartContentSettings.h/.cpp` | The data asset and its structs, with verified defaults. |
| `StartLocations/SurvivalStartContentPlanner.h/.cpp` | Plan building (pits → protected areas → fields → wrecks → craters → foliage). |
| `StartLocations/SurvivalDefensePointFinder.h/.cpp` | Designer points, hill-top scan, build-edge fallback. |
| `StartLocations/SurvivalStartPlacementHelpers.h/.cpp` | Landscape-only projection, occupancy circles, weighted picks, annulus sampling, ground rotation. |

## Risks / not verified

- Not compiled yet (integrator builds). Not run in PIE.
- Wreck and pit footprints are estimates (geometry collections / child actors have no measurable static mesh);
  adjust in the data asset if they overlap.
- Pit forward axis is assumed +X; if pits face the base, set `YawOffsetDegrees = 180` on the entries.
- German pit Blueprints are used for every faction; add faction-specific lists if a Soviet player faction needs
  other pits.
- Tank hulks give metal/radixite/blueprints, not `RESOURCE_VEHICLE_PARTS`; the vehicle-part piles and cars
  provide vehicle parts.
- Runtime spawning of ~60 actors happens in one frame after loading (during the drop-in); a hitch is possible
  on slow machines.
