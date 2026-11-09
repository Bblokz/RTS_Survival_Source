# 50 — Soviet bases

Owner: Soviet-base agent. Code: `Source/RTS_Survival/SurvivalMode/SovietBases/`. Data:
`Content/RTS_Survival/SurvivalMode/Data/SovietBases/DA_SurvivalSovietBases` (`USurvivalSovietBaseSettings`).

Every start location the player did **not** pick gets a Soviet base, generated at runtime by a PCG graph, right after
`ConfirmStartLocation` (`ASurvivalModeDirector::GenerateStartLocationContent` → `GenerateSovietBaseAt`).
Bases are real enemy (player 2) buildings and units: they shoot, they unlock enemy tech, and the strategic AI uses their
reserve units. Every base has at least two production buildings, the least the AI's cluster search accepts. Breaking a base's defences and production pays out resources.

**Bases are a separate system.** By decision of the project owner they are not wired into the waves or into any other tech:
nothing outside `SovietBases/` calls `GetWavePressureMultiplier`, `OnSovietBaseDestroyed`, `GetAliveBaseLaunchPoints` or
`GetAliveSovietBaseCount`, so destroying a base does **not** lower wave pressure, and waves do not leave from bases. The API
stays for whoever wants it later. `DifficultyPercentage` and `bUseMutations` do not reach bases. Difficulty shows through what is
generated: layout, building selection, defences and garrison. The system depends on the director for start locations, the
chosen location and the difficulty, and on the enemy controller for the AI hookup; nothing depends on it except the director.

## Files

| File | What |
|---|---|
| `SurvivalSovietBaseTypes.h` | Designer structs: structure slot, threat step, site kit, reward, preset, difficulty row; role, zone, archetype, seed enums. |
| `SurvivalSovietBaseSettings.h/.cpp` | `USurvivalSovietBaseSettings` data asset and the code defaults `ResetToDefaults` writes into it. |
| `SurvivalSovietBaseSite.h/.cpp` | `FSurvivalSovietSiteMap`: terrain from real traces. Flat ground, slopes, drivable flood fill, open approaches, overlooks, drivable paths. |
| `SurvivalSovietBaseMasterPlan.h` | The plan an archetype draws in a local frame: streets, plots, fence runs, gates, emplacements, park rows. |
| `SurvivalSovietBaseArchetypes.cpp` | Where each archetype wants its gate, and the switch to its drawing. |
| `Archetypes/SovietBaseArchetype_<Type>.cpp` | One file per base type: its master plan and its preset. |
| `Archetypes/SovietBasePlanPen.h`, `SovietBasePresetLibrary.h/.cpp` | The pen the plans draw with; the price list, unit groups and slots the presets are built from. |
| `SurvivalSovietBasePad.h/.cpp` | Ground decals: the pad under a building, and the pieces of roads, junctions and gate aprons (`InitShape`). |
| `Tools/` | `regenerate_asset.py` writes the code tables into the data asset; `make_ground_decals.py` builds `M_SovietGroundDecal` and its pad, road and apron instances (`make_pad_materials.py` is the older pad material, only `MI_SovietPit_Earth` still comes from it). |
| `SurvivalSovietBaseLayout.h/.cpp` | `FSurvivalSovietBaseLayoutRun`: the layout as stages, sharing one occupancy; buys the threat path and executes the master plan through `FEnemyBaseSetupBuilder`. No UObjects except the resolver callback. `BuildPlan` runs all stages in one go (match table). |
| `PCG/SurvivalSovietBasePCGNodes.h/.cpp` | The custom PCG nodes (`Soviet Base Site`, `Soviet Base Stage`, `Soviet Base Spawn`) and `ASurvivalSovietBasePCGVolume`, the actor whose `UPCGComponent` runs the graph. |
| `SurvivalSovietBaseComponent.h/.cpp` | `USurvivalSovietBaseComponent`: strength, preset pick, async load, starts one PCG volume per base, serves the graph's nodes, batched spawning, garrison, AI hookup, destruction tracking, rewards, query API. |
| `SurvivalSovietBasePreview.h/.cpp` | Test builds only (`RTS_WITH_MAP_TESTS`): headless captures, spawn manifests, path tests, match table, match watch. |

Public contract unchanged: `InitSovietBases(ASurvivalModeDirector*)`, `GenerateSovietBaseAt(ASurvivalStartLocation*, int32)`.

## Generation is a PCG graph, run at runtime

`Content/RTS_Survival/SurvivalMode/PCG/SovietBases/PCG_SovietBase` is a real `UPCGGraph`. `GenerateSovietBaseAt` spawns one
`ASurvivalSovietBasePCGVolume` per base with a `UPCGComponent` (`GenerateOnDemand`, seed = base seed, graph =
`USurvivalSovietBaseSettings::BaseGraph`) and calls `GenerateLocal`. The PCG scheduler then runs:

```
Soviet Base Site ─ Stage MasterPlan ─ Stage Streets ─ Stage Buildings ─ Stage VehiclePark ─ Stage Guns ─ Stage Perimeter ─ Stage Dressing ─ Stage Garrison
                                          └───────────────┴─── every Stage's Placed points ───────────┴──────────────┘
                                                                                  └─► Soviet Base Spawn
```

- **Site** measures footprints and traces terrain over frames (the node sleeps with `bIsPaused`, the component wakes it) and
  outputs the terrain as points (`Density`, `Steepness`).
- **Stage** runs one step of the layout. Stages are chained through their `After` pins. Each outputs what it placed as points:
  transform plus attributes `ClassIndex`, `Role`; streets as point chains with `Road`, `RoadKind`, `RoadWidth`.
- **Spawn** reads the points it receives and builds exactly those, in the usual batches over frames, then sleeps until the base
  stands. What is built is what reaches this node: a stock PCG filter between a `Placed` pin and `Spawn` takes effect, and
  removing an edge removes that content.

Stages place through the **placement engine the PCG enemy-base nodes use**
(`Procedural/CustomNodes/EnemyBaseSetup/EnemyBaseSetupPlacement.h`: `FEnemyBaseSetupBuilder`, `FEnemyOccupancyGrid`), which gained, all
inert for the campaign nodes: a `Reserved` occupancy category (`ReserveCorridor`; streets cut gaps into walls, belts and wire
because those runs avoid it), a whole-footprint block test (`SetBlockedFootprintPredicate`), `PlaceCentered` (by footprint
centre, so off-centre Blueprint pivots line up) and `PlaceSegmentRun` (walls, fences, wire and sandbags laid end to end by real
length).

Limits of the graph today: stage order and the occupancy are shared state in `FSurvivalSovietBaseLayoutRun`, so stages cannot run
without each other's predecessors, and building selection stays in the data asset rather than in graph nodes. Node positions in
the graph asset were not set, so press Auto Arrange when opening it.

## How a base is built

1. **Distance rank**: 0 for the base nearest the player. The difficulty row reads everything by rank; its tables are stretched over
   the number of bases: `StrengthByDistanceRank` (which plans and content a base may use), `ThreatBudgetByDistanceRank` (how much
   defence it buys, so what the player meets rises in the order he meets it) and `ReservePointsByDistanceRank`.
2. **Preset**: seeded, weighted pick among presets whose strength range contains the base's strength and whose
   `MinProductionTier` the difficulty's tech cap reaches. `FarthestBasePresetWeights` shapes the farthest base (0 rules a preset out).
   While another archetype is free for that strength, no two bases share one.
3. **Site node**: footprints measured (`ClassesMeasuredPerStep`), terrain traced (`SiteTracesPerStep`, 400 cm grid to 5000 cm
   past the build area), spread over frames. Only colliding components count towards a footprint: most Rus Blueprints carry a
   large non-colliding sphere, and some leave a hidden component at the world origin.
4. **Threat budget**: first every step marked `bSignature` for exactly this difficulty is bought once (from its
   `SignatureMinStrength`), so a tier shows on every base. Then the budget is spent down the preset's `ThreatPath` in passes.
   A step is skipped when its gates say so (`MinStrength`, `MinDifficulty`, `RequiredProductionTier`, `FirstPass`, `MaxCopies`). A unit
   must cost at most the row's `MaxUnitPoints`; reserves also stay under the rank's reserve points. Structures that find no spot
   hand their points back; at the end the garrison buys with them, up to 2x `MaxCopies` and only units of at least the row's
   `MinTopUpUnitPoints`. What is left stays unspent and is reported (`unspent`).
5. **Stage order = priority for space**: streets and gates, production in zoned plots (a building that does not fit its plot
   moves to any plot, then to any free ground inside the base), support, revetments, guns, bunkers and fence runs, dressing,
   garrison posts. Guns that find no listed spot search rings inside the perimeter, artillery from the rear, others from the
   threatened side, so gun capacity grows with the plan. Plans grow with the budget (`SizeFill`). A slot that still fails goes to
   `DroppedSlots` in the plan and the manifest.
6. **Spawn node**: roads, then structures in batches (`StructuresPerSpawnStep` every `SpawnStepSeconds`), then garrison.
   All stages of a base together take 1 to 3 ms in DebugGame.

Everything is checked against real heights: `GetIsFootprintBuildable` rejects any corner on a slope or step, the gate goes
on the open approach nearest to what the archetype wants, and ridge guns go to `GetDefensePoints()` and detected overlooks.

## Rules no base type can break

The shared layout enforces these; an archetype only says which family and where.

- **One wall family per base.** `FenceLoop(Corners, Family)` sets it for the whole plan. Each span between corners and gates is fitted
  exactly: as many pieces as fit, stretched by what is left (`wall.max_stretch` in the manifest). Corners: rounded end pieces at any
  angle, or the family's corner piece at right angles. Thin families simply meet on the corner point. Gates are the family's gate
  piece, or an opening with its swing gate or barrier blocks. A slot on bad ground or a road stays open and both sides get a
  finished end. Walls go down before buildings: they are the outline, buildings fit inside.
- **Wall pieces are measured, not guessed.** Piece lengths and pivots in `SurvivalSovietBaseSettings.cpp` are the visible meshes,
  read from spawned actors (manifest `wall.piece_list[].vis`, `Saved/AgentNotes/base_types/wall_joints.py`). The concrete colliders run
  30 to 60 cm past the meshes, which once left sand at every joint. Environment Blueprints (sandbags, precast fence, wire) drop the spawn
  scale, so `EnforceSpawnScale` sets a wall piece's stretch again after it spawns.
- **Flags.** A family's `FlagRate` (Soviet concrete 0.07) swaps a piece for its flag variant, seeded, with at least `FlagMinGap` (4)
  plain pieces between two flags along the whole loop, corners and gates included (the count runs over spans and runs). Only
  straights carry flags. A single piece left between two open slots is dropped; a thin family (no end pieces) that stops at an open
  slot gets a concrete block (`Kit.GateBlocks`) as its finished end. Every open slot is in the manifest (`open_wall_slots`) and pictured.
- **Bunkers stand only outside.** `PerimeterLine` is the closed line of a plan; `FenceLoop` sets it, and a plan without a closed wall
  sets its own (the Hill Stronghold's is the circle of its trench arcs). Every bunker footprint is tested on placement and
  checked again in `ValidateBunkerRule`; a violation goes to `rule_violations` in the manifest and to the log as `SOVIET_RULE`.
  Light bunkers go forward as outposts, the row's bunkers stand against the wall, the heaviest form a second row further out.
  Watchtowers and gun pits may stand inside. The Admin Bunker is a production building, not a bunker. Wall bunkers stand flush:
  their back 12 cm off the wall's outer face, and never within one gate width of a gate (`GetIsClearOfGates`).
- **Every building stands on a pad.** Production, support, defences, huts and hangars get an `ASurvivalSovietBasePad` of their real
  footprint and yaw, plus a margin of 45% of the smaller half extent, between 70 cm and `Kit.PadMargin`. The surface comes from
  `FSovietMasterPlan::PadKinds` by zone (default gravel); defences get packed earth. Footprints under `Kit.PadMinHalfExtent` get none.
  Pads are opaque inside with a noise-broken soft edge, so neighbouring pads of one surface merge into one yard.

## Building footprints (round 5)

Each production or support building's pad is dressed for its class by the base's one `ASurvivalSovietBaseDressing` (one
instanced group per mesh per base, no collision or navigation effect, no shadows on slabs and planks, clutter and planks
culled as a group beyond `FoundationClutterCullDistance`). The layout picks the class by name token and carries it through
the PCG points (`FoundationStyle`, `FootprintHalf`, `DoorDirection`, `ApronLength`):
- factory / cooling towers / research: a chamfered slab (`SM_SovietSlab`, `M_SovietFoundationMesh`) 10 cm over the highest
  ground, the building lifted onto it, the pad decal in `MI_SovietPad_Foundation` (joints, oil stains, sand rim), a concrete
  apron to the road; slabs of one yaw within 150 cm merge;
- barracks: a gravel walk decal, duckboards to the road; tents: a trampled-earth oval and planks; shelters and storage:
  oily gravel and a rutted track decal; the command bunker: a concrete forecourt on the door side with sandbag corners;
- clutter in one or two work corners outside any slab, on the ground.
The manifest's `footprints[]` records slab heights, lifts, clutter clearances and aprons. `-SovietBaseNoDressing` (map tests)
keeps the plain pads; `-SovietBasePerf` with the watch measures the frame cost (README round 5: GPU +0.03 ms, +37 draws at the
RTS view, +18 at the map view). Materials: `Tools/make_ground_decals.py`; desert nets (owner choice): `Tools/make_desert_net.py`
and `-SovietBaseDesertNets`.

## Roads, junctions and gate aprons

Streets are painted as decals of `M_SovietGroundDecal` (kit `MainRoadSurface`, `LaneRoadSurface`, `TrackRoadSurface`, `StripRoadSurface`),
in pieces of at most 16 m that each sit on their own ground. Three widths: main (planned 1050, painted 1.05x), lane (850, painted 0.78x)
and track (700, painted 0.55x; Hill Stronghold crest tracks). Each road has soft verges, two wheel ruts along it, and ends that either run
into the street they meet or fade out. Where a street meets another, a rounded patch of the wider street fills the mouth; every gate gets a
concrete apron through it that fades out on both sides (`GateApronSurface`). A kind without a surface falls back to its ground path Blueprint.
Every surface is one of the landscape's own layer textures (rippled sand, western gravel, weedy cracked mud, scorched concrete)
times the landscape's brightness and a shade, so roads, pads and aprons share the desert palette: main road darkest, lane lighter,
tracks and yard paths worn cracked earth, aprons and concrete pads scorched concrete slabs with joints.
The decal material reads its frame from a dynamic instance (`ShapeCenter`, `ShapeAxis`, `ShapeHalf`, `ShapeEdge`, `ShapeFade`), so edges,
ruts and fades are in cm whatever the decal's size.

## Storage yards and lanterns

A plot dressed `Crates` or `Munitions` becomes an ammunition yard, one dressed `Barrels` a fuel yard. Yards are planned with the
master plan (`PlanYards`, `FitYard`): the plot plus 180 cm, pulled off the wall (450 cm), the streets and other plots, then grown into
free ground towards 42 x 34 m. From then on no other building, gun or prop may take yard ground (the builder's blocked-footprint
predicate). Up to three of the preset's ammunition stores or fuel tanks stand in the back row of a yard whose shorter half side is at
least 13 m; the rest stand elsewhere. A deep yard turns its rows towards the street. It is closed by one destructible
fence family (`Kit.AmmoYardFence`, precast concrete; `Kit.FuelYardFence`, chain link `BP_Industrial_Dest_FenceWire`) laid by the wall
code, with a 760 cm entrance in the street side. Stacks stand in straight rows with 340 cm aisles either side of a middle path;
worn-earth paths (`Track` streets, 480 planned) run from outside the entrance down the middle and along every aisle. Ammunition rows get
the large camouflage nets of `Kit.YardNets` over pairs of rows. Fuel and ammunition never share a yard; their distance comes from the
archetype's zoning. `ASurvivalSovietBaseLantern` (mesh `Kit.LanternMesh`, `SM_LampPost`) stands beside every gate, every street end
that meets another street and every yard entrance, never on a painted road. The manifest lists `yards` and `lanterns`.

## Archetypes

Each is a different street plan, perimeter and zoning, and each scales its own way, because its `ThreatPath` is its own.

| Preset | Archetype | Strength | Min tech | Plan | Scales by |
|---|---|---|---|---|---|
| Forward Firebase | Firebase | 0–1 | 0 | Hexagon or octagon wheel: sandbag rim, ring road, guns between ring and rim, hub with production, one rear gate | Guns, then heavier guns |
| Rifle Cantonment | Cantonment | 0–1 | 0 | Parade axis, parallel lanes, rows of barracks gable-on, service yards, wire fence | Infantry and barracks |
| Field Airstrip | Airstrip | 0–2 | 0 | Strip along a chord, apron with hangars, camp behind, fuel and bombs across the strip | Air defence |
| Tank Park | TankPark | 1–2 | 1 | Boulevard, cross lanes lined with U revetments, workshops at the end, fuel on a flank behind a berm | Armour |
| Supply Depot | FortifiedDepot | 1–2 | 1 | Walled square, fire-break cross, ammunition and fuel on opposite corners, refinery plant from Hard strength 2 | Wall bunkers and stores |
| Hill Stronghold | HillStronghold | 1+ | 0 | No wall: tight core, tracks to the crests, zig-zag trench arcs across open approaches, guns on ridges and ramp heads | Dug-in guns on crests |
| Command Fortress | CommandFortress | 2+ | 2 | Chamfered brick walls, two gates, avenues to a keep, one function per quadrant | Heavy bunkers and artillery |

A preset below its `MinProductionTier` is never rolled: no tank park without a factory, no fortress without its Admin Bunker.

## Difficulty: one row per difficulty

Every row is a set of tables in the data asset. Character is what the row can field; which of it stands on a given base depends on
that base's rank and archetype. Each archetype carries one signature step per tier from Hard up.

| Difficulty | Character | Strength by rank | Budget by rank | Reserve by rank | Max unit | Top-up floor | Tech cap | Wire s0/s1/s2 | Belts from | Forward posts s0/s1/s2 | Inner line s0/s1/s2 | Reward/threat |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| NewToRTS | Rear guard: infantry, MG towers, 45 mm guns | 0,0,1,1 | 50,70,125,190 | 0,0,4,5 | 10 | 3 | 0 | 0/0/0 | never | 0/0/0 | 0/0/0 | 3.5 |
| Normal | Line regiment: wire, mortars, light tanks, first tank factories | 0,0,1,2 | 65,110,170,310 | 0,0,7,20 | 15 | 3 | 1 | 0/1/1 | s2 | 0/0/0 | 0/0/0 | 3 |
| Hard | Guards division: T-34, BS-3, ZiS bunkers, Bofors, belts, research | 0,1,1,2 | 85,165,250,450 | 0,5,10,20 | 20 | 10 | 2 | 1/1/2 | s1 | 0/2/2 | 0/0/0 | 3 |
| Brutal | Shock army: T-34/85, KV-1, ML-20, turret bunkers | 0,1,2,2 | 120,250,400,640 | 0,7,12,20 | 28 | 12 | 2 | 1/2/2 | s1 | 0/2/3 | 0/3/4 | 2.75 |
| Ironman | Stavka reserve: KV-2, IS-1, B-4, mega bunkers, second belt | 1,1,2,2 | 170,370,640,940 | 0,10,20,26 | 50 | 20 | 2 | 2/2/3 | s0 | 1/3/4 | 0/3/4 | 2.5 |

Support buildings scale through the slot's `CountPerStrength` and `CountPerDifficulty`. These tables are the balance
surface: a row edit changes every base with no rebuild and no rounding step in between, since a budget is spent point
by point.

Threat points: units pay `EnemyTrainingHelpers` training prices. Structures pay `ThreatPoints` on the step: guard tower 6;
MG bunker, DShK and 53-K pits 8; AutoCn bunker, Ampulomet 10; Dome, Long Camo, mortar 12; ZiS, Bofors, weapon platform 15;
Bunker03 20; BS-3 22; Bunker05 28; Mega bunker 30; ML-20 38; B-4 45. Production, support and props cost nothing.

**Rewards**: a base's payout is its real threat × `RewardPerThreatPoint`, split by the preset's `DestroyRewards` shares and paid as it
is earned. `ProductionPayoutShare` (0.3) is paid when the last production building falls. The rest is paid in step with the base's
defences: every destroyed gun, tower and bunker earns its part, and `MinDefenseBrokenFraction` (0.6) of the defence points earns all of
it. Killing only production pays 30%. Payments go through `UPlayerResourceManager::AddResource`, clamped to storage. Storage is what the
player built: HQ 800 radixite, 350 metal, 250 parts; each refinery +1500 radixite; each metal vault +800 metal, +400 parts.

## Where the numbers live

All tables and the graph reference are serialized in `DA_SurvivalSovietBases` (about 190 KB), so soft references are followed by the
cooker and an edit in the asset is what the game uses. The constructor leaves them empty. `FillAllDefaults` (C++) is used by the `ResetToDefaults` button
and, as a safety net, by `PostLoad` when an asset has no presets (`GetIsUsingCodeDefaults`, reported in the manifests).
After changing the code defaults, press Reset To Defaults and save; the asset otherwise keeps its own values.
Saving this asset from an editor running an older DLL drops fields that DLL does not know.

## Seeds

`SeedMode = RandomPerMatch` (default) or `Fixed` (`FixedSeed`). Base seed = hash(match seed, rounded deploy centre, BaseIndex).
One seed reproduces axis, mirror, plan variant, unit picks and placements exactly.

## How bases work with the enemy AI

- **Trenches**: from the strategic AI's start (the grace period), the enemy controller's `UEnemyBaseTrenchComponent` digs Soviet
  field trenches out of the bases' pit anchors and on an arc in front of each base cluster, facing the player's HQ. They are free and capped per base. See
  `Source/Docs/EnemyAI/EnemyBaseTrenches.md`. Not yet watched in a real match.

- **Registration**: a spawned Rus building expansion registers itself in `URTSComponent::BeginPlay` →
  `UGameUnitManager::AddBxpForPlayer(OwningPlayer)`; units from the async spawner do the same. Ownership comes from the
  Blueprint defaults. Walls, fences and props are environment actors on purpose, so they never count as buildings.
- **Production floor**: every preset guarantees two production buildings at every difficulty; the matrix fails a base below that.
- **Tech** (`TrainingRequirements_ThinkStep`): BTX_RusBarracks → BasicInfantry, BTX_RusFactory / BTX_RusCoolingTowers →
  LightTanks, BTX_RusResearchCenter (Research Center, Admin Bunker) → Tier2. Difficulty caps it through `MaxProductionTier`;
  destroying the last building of a type removes the unlock. Strategic AI building tech tops out at Tier2.
- **Base clusters**: `ConfigureStrategicAIBaseDetection` merges core and satellite types into `FindEnemyBase_TimerRequest`,
  raises `MaxBasesToReturn` and `CoreClusterDistanceXY` (at least the plan's `RequiredCoreClusterDistance`, the smallest link
  that chains all production buildings) and caps the score thresholds.
- **Training location**: bases have no enemy trainer component. `GetValidTrainingSpawnTransform` falls back to
  `GetRandomEnemyBaseClusterSpawnTransform`, so by the code trained units appear at a random base cluster. A watched match has not shown this yet: in 380 s of the menu flow nothing spawned within 15 m of a cluster centre. The note
  in `40_WavesAndAI.md` that the AI trains nothing without a trainer component does not match this code. Until the first cluster
  search finishes the AI reports once that it has no spawn transform.
- **Reserves**: `RegisterAsBlackboardIdle(true)`, same path as freshly trained units.
- **Patrols** are infantry only: vehicles of a patrol group hold the post they spawned on, so they leave no tyre-trail loops.

## Testing

`-SovietBasePreview=<spec>` generates the listed bases through the graph (`label preset seed strength difficulty`), captures each, writes
`<label>_manifest.json` from the actors that really stand, counts spawned against planned, and paths from outside the base to
every post for all four nav agents. `-SovietBasePreviewDir=<dir>`, `-SovietBasePreviewTopOnly`, `-CaptureExposureBias=<stops>`.
`-SovietBaseMatchTable=<file>` rolls whole matches per difficulty, player start and match seed through the real strength and preset
code, planning without spawning. `-SovietBaseSurvey=<class list>` logs bounds and pictures Blueprints.
`-SovietBaseWatch=<seconds>` with `-SurvivalAutoStart` watches a real match and logs where enemy units appear and how many base
structures stand. It unpauses the world, like the player's start button; `-SovietBaseWatchKeepPause` does not.

**Bases only build while the world runs.** Preparation, the PCG graph and spawning tick with the world, so in a paused world (the setup
screen until the player presses start, or a bare `-SurvivalAutoStart` run that nobody starts) every start gets its PCG volume and nothing
else. Measured 2026-10-31: unpaused, four bases stood (663 structures) 16 s after start; kept paused, 0 after 90 s.
Run with `-ExecCmds="r.TextureStreaming 0"`.

## Risks / not verified

- **Packaged game**: not cooked or run, so runtime PCG in a cooked Shipping build is unproven. The graph sits in the cooked
  `SurvivalMode` folder and the data asset references everything it spawns. `Config/DefaultGame.ini` also gained the road, pit
  and heavy gun folders.
- **Navigation** is `DynamicModifiersOnly`: buildings block through nav modifiers. Path tests pass on the current terrain; rerun
  them when the map changes.
- The wall arch `BP_Military_Dest_WalkableGate`: the path test has to prove units pass under it on every plan that uses one.
- Road tiles are rebuilt by C++ from the tiles a `BP_GroundPathSpline_*` lays at spawn; each tile is one translucent draw.
- `CoreClusterDistanceXY` is raised globally on the enemy controller (bases are at least 12000 apart).
- The shared spawner has one class per training option; a player-faction variant could replace an entry. The component checks
  owner 2 on everything it spawns and reports once per class. `Squad_Rus_HazmatBazooka` is not in the spawner map; do not use it.
