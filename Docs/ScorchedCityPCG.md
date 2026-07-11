# Scorched City PCG Node

Custom PCG node (`Scorched City`) that generates a scorched city around a single pivot point.
Source: `Source/RTS_Survival/Procedural/CustomNodes/ScorchedCity/`.

## 1. High-level architecture

Three layers, deliberately decoupled:

| Layer | Files | Responsibility |
|---|---|---|
| PCG settings | `PCGScorchedCity.h` | `UPCGScorchedCitySettings` — all designer-exposed UPROPERTYs, pins. |
| PCG element | `PCGScorchedCity.cpp` | `FPCGScorchedCityElement` — resolves assets & real bounds, runs the generator, spawns managed actors, emits point outputs. Main-thread, non-cacheable. |
| Layout generator | `ScorchedCityGenerator.h/.cpp`, `PCGScorchedCityTypes.h/.cpp` | Pure, deterministic C++ (no UObjects): zoning, roads, lots, bounds-aware placement. Could later run off the game thread. |

Pipeline inside `Generate()` (fixed order = deterministic):

`BuildZones → BuildGridRoads → BuildCurvedRoads → AddIntersectionFootprints → BuildLots → PlaceBuildings → PlacePowerLines → BuildScatter → ExportRoads`

Pins:
- **In** (Spatial, required): the city area, typically a closed spline loop. Its bounds define the city rectangle (`CityLengthX/Y` are derived, not settings) and its shape trims the layout: closed splines are sampled into a 2D polygon for fast point-in-polygon tests; other spatial data falls back to density sampling. The fill is a good-enough approximation, not an exact packing of the shape.
- **Exclusion** (Spatial, optional): buildings and scatter are never placed where these sample density > 0.
- **Scatter** (Point): debris points with a `Mesh` (SoftObjectPath) attribute → wire into a Static Mesh Spawner, mesh selection "By Attribute".
- **OccupiedBounds** (Point): every reserved oriented footprint (roads, intersections, buildings, poles) with an `Occupancy` int attribute — use with Difference/filters to exclude other PCG content.
- **Lots** (Point): generated lots with a `Used` attribute (debugging / extensions).
- **OuterOrphanRoads** (Point): rim road endings that could not be connected with a natural curve; each point sits on the road's final outer position (ground-projected) and its rotation yaws outward, so other PCG logic can continue those roads beyond the city.

## 2. Data structures

- `FScorchedFootprint` — oriented 2D rectangle (center, half extents, yaw). The atomic unit of *all* placement; never center points.
- `FScorchedSpatialHashGrid` — hash map of cells → footprint indices, each entry typed (`Road`, `Intersection`, `Building`, `PowerPole`). Queries take a type bitmask, so "scatter may overlap roads but not buildings" is one mask change.
- `FScorchedRoadNode` / `FScorchedRoadEdge` — graph; edges store polylines (2 points for grid streets, many for curved). Node degree drives intersection handling.
- `FScorchedLot` — oriented rect beside a road with facing yaw, corner/dense/grid/major flags.
- Results: `FScorchedBuildingSpawn`, `FScorchedPoleSpawn`, `FScorchedRoadSplineResult`, `FScorchedIntersectionSpawn`, `FScorchedScatterCandidate`.

## 3. Bounds-aware placement

- Building footprints come from **measured real bounds**: the element spawns a transient instance of each Blueprint class once, reads `GetComponentsBoundingBox` (exact, incl. construction-script meshes), destroys it, and caches half extents + pivot-to-center offset. `bUseFootprintOverride` bypasses this.
- Every candidate is an oriented footprint **after rotation** (scale for scatter), tested with a SAT overlap against the hash grid.
- Inflation: candidate is inflated by `BuildingSpacingExtra + rand(MinSpacing..MaxSpacing)` before testing; the stored footprint is uninflated. (Spacing therefore protects the placement being made; it is not retroactively enforced by neighbours.)

## 4. Road generation

- **Zoning**: macro cells of `GridBlockSize` are labelled grid/curved by hashed noise with probability `GridLayoutAmount : CurvedRoadAmount`, and dense/sparse from `OverallDensity`. Both layouts coexist in one city; zone borders are where curved roads sprout.
- **Grid**: lattice crossings touching grid cells become nodes; neighbouring crossings connect where the street borders a grid cell → blocks, straight streets, 90° 4-way intersections. Every `MajorRoadInterval`-th line is a major road.
- **Curved**: random-walk walkers seeded at grid/curved zone borders (or radially for pure-curved cities). A walker is only seeded when the stretch after the junction is clear (space for a natural curve). Per segment: heading += rand ± maxTurn (from `CurvedRoadCurvature`), length jittered by `CurvedRoadVariation`, branch chance `CurvedRoadBranchChance`. Walkers stop at city bounds (clamped), on self-proximity, on road collision (snap-to-node junctions only when the turn into the junction is ≤ ~70°), or entering grid zones. Walks that neither connect to another road nor reach a minimum curve length are discarded entirely.
- **Corner fillets**: degree-2 corners of two perpendicular straight streets (typically at the city rim) are replaced by a curved quarter-arc edge — both streets are pulled back by the fillet radius and bridged, so rim streets bend into their flanking roads instead of butting at 90°.
- **Orphan pass**: remaining degree-1 endings that roughly face each other within ~1.75 blocks are joined with cubic-bezier curves (only when the path is clear and inside the area). Unconnectable rim endings are exported on the OuterOrphanRoads pin.
- Committed edges are **rasterized** into road-width oriented boxes (chunk = road mesh length) in the hash grid — this is what keeps everything else off the roads. (Fillet-shortened streets keep their original raster; slightly conservative near smoothed corners.)
- Export: intersection mesh instances at degree≥3 nodes. The 4-way asset may be **non-square** (`IntersectionSizeX/Y`, auto-derived from mesh bounds or overridden per axis): each road end is trimmed back by the footprint's support radius along its own exit direction, so splines end flush with the piece on both axes. Splines are linear for grid streets, curved otherwise; the road mesh is chained along each spline with `USplineMeshComponent`s; leftover slivers shorter than half a road width are dropped.

## 5. Lots & buildings

Lots are marched along both sides of every edge (width/depth scaled up on major roads), offset by `RoadWidth/2 + RoadSetback + MinRoadBuildingDistance`, rejected if they clip other roads or leave the city. Buildings: lots are filled majors/corners first; a density gate (dense vs sparse zone × `OverallDensity`) decides if a lot is used; a weighted pick combines designer weight, zone preference and size-vs-lot fit (large → major/corner, small → side streets); placement faces the road (`FacingYaw` + allowed rotation offsets), pushes the building to the lot front, jitters laterally, and SAT-tests the inflated footprint against everything.

Two safeguards keep blocks from staying empty: short streets between large 4-ways that cannot fit a full-width lot get one **narrower centered lot** (down to ~55% width), and at `OverallDensity ≥ 0.35` a **guarantee pass** places at least one building on every street that has lots, so the random density gate can no longer leave whole squares empty.

### 5.3 Road-side objects

- **Road lanterns/signs** (`RoadLanterns`): a weighted Blueprint list marched along every road at a global `Spacing`, alternating sides, offset `OffsetFromRoadEdge` from the road edge and always yawed so local +X faces the road. Bounds-aware; spots blocked by buildings, poles, props or the roadways themselves are skipped — but intersection footprints are allowed, so lanterns also appear on the corner areas of 4-ways.
- **Road blocks** (`RoadBlocks`): candidate spots every `Interval` along each road roll a seeded `Chance`; winners get an arc of items of **one** Blueprint type. The arc radius derives from a random yaw span in `[MinYawSpanDegrees, MaxYawSpanDegrees]` so its lateral reach always equals the width to block (`2·R·sin(span/2) = Width`; 180° = U shape). On plain road that width is `RoadWidth`; when the spot lands on an intersection the block is instead centered on the node and sized to the intersection mesh's full lateral extent — enough blockers to seal the crossing — with at most one block per intersection. Item count and offsets come from the type's real bounds, items face outward, and solid obstacles just leave gaps in the block.

### 5.4 Auxiliary blueprints

`AuxiliaryBlueprints` entries (props, wrecks, barricades...) are placed around the squares (random points inside generated lots, `CountPerLot`) and in rings around placed buildings (`CountPerBuilding`, `Min/MaxDistanceFromBuilding` from the footprint edge) — never on roads or intersections. Per entry: `bCheckBuildingCollision` / `bCheckPoleCollision` toggle those occupancy masks; failed placements retry a few jittered positions nearby; `bOverrideScale` applies a uniform random scale in `[MinScale, MaxScale]` (footprint scaled accordingly). Placements reserve `Auxiliary` occupancy so they never stack on each other, and spawn as managed Blueprint actors with ground-projected pivots.

### 5.5 Building categories & HISM batching

Each building entry has a `Category`:
- **ScorchBuilding** (default): the Blueprint's static meshes (plain components and ISM/HISM instances, extracted once from a transient inspection instance) are batched into **one city actor** holding one Hierarchical Instanced Static Mesh component per unique mesh — the same manual "merge selected actors into HISMs" workflow, automated. Placement, footprints and ground-projected pivots are identical to actor spawning; non-mesh components/logic of the Blueprint are discarded. Instances are bulk-added per mesh so each HISM cluster tree builds once.
- **BlueprintBuilding**: spawned as its Blueprint actor, unchanged (keeps components and logic).

## 6. Scatter

Per building × per profile: chance roll, count from density × zone, positions sampled in a ring measured from the **footprint edge** (support function, so distance respects building orientation), optional clustering. Checks: exclusion input, city bounds, avoid-roads / avoid-buildings masks using the mesh's scaled radius. `bAlignToGround` line-traces down (ignoring all actors this node spawned) and aligns to the ground normal.

## 6.5 Decals

Each decal category has a `CountMultiplier` (clamped 1–20) that scales how many decals it generates. The systems share `FScorchedDecalEntry` (material, weight, min/max world size) and spawn as `UDecalComponent`s on one managed anchor actor, projected straight down (ground-snapped without ignoring spawned actors, so they land on road meshes too):
- **RoadDecals**: `SplineRoadDensity` (per 10 m of trimmed road spline, so never under intersections) with road-aligned yaw, plus `IntersectionDensity` (per 4-way).
- **LotDecals**: `LotDensity` per generated lot, kept inside the lot, skipped under buildings; each placement is recorded in a local decal grid.
- **AroundBuildingDecals**: `DensityPerBuilding`, ringed off the building footprint edge; checked against the decal grid so they **never overlap lot decals** (or each other).

Road splines are trimmed per exit direction against the real (unclamped) intersection mesh half-extent — diagonal exits trim up to √2 further — with a small tuck-in so they end flush at the 4-way's edge instead of running underneath it; slivers between adjacent large intersections are dropped entirely. Curved walkers leave junctions exactly along the cardinal exit (curvature starts at segment 2) so their splines connect flush to grid roads.

## 7. Power lines

Per edge: one side chosen, poles marched at `PowerLineSpacing` with `OffsetFromRoadEdge` from the road edge; every pole spot is occupancy-tested individually (intersections included), so no arc clearance eats the street — set `PowerLineSpacing` to the two-pole asset's pole distance. Each step places the **two-pole asset** toward the next valid pole position (yawed along the road), or the **single-pole asset** on broken rolls (`BrokenSectionChance`), endpoints, or when the next position is blocked. Pole clearance squares go into the hash grid; blocked positions produce natural gaps. Author the two-pole asset to span ~`PowerLineSpacing`; after a two-pole section the march skips one spacing so chained assets never double a pole.

## 8. Editor-exposed settings

All on the node: city (`CityLengthX/Y`, `RandomSeed`, `OverallDensity`, `GridLayoutAmount`, `CurvedRoadAmount`), roads (`RoadWidth`, `RoadSetback`, `MinRoadBuildingDistance`, `GridBlockSize`, `MajorRoadInterval`, curvature/variation/branch/segment, mesh refs + length/size overrides), buildings (array of `FScorchedBuildingAssetSettings`: class, weight, footprint override, min/max spacing, rotation mode, size category, zone preference; plus `BuildingSpacingExtra`, `LotWidth/Depth`), scatter (array of `FScorchedScatterProfile`: meshes+weights, per-building density, min/max distance from bounds, scale, yaw, chance, avoid flags, align-to-ground, clustering) and `FScorchedPowerLineSettings`.

Determinism: everything derives from `RandomSeed` via one `FRandomStream` plus positional hashes (zones). The PCG component seed is intentionally ignored (`UseSeed() = false`).

Terrain: with `bSnapToGround` (default on) road spline points (subdivided every ~12 m), intersections, buildings and poles are line-traced onto the landscape; `RoadZOffset` lifts roads/intersections to avoid z-fighting with the ground. Building footprints are measured from **mesh components only** (FX/trigger components would inflate them); measured sizes are printed to the log, and the node warns when a building is larger than any lot or when the intersection footprint had to be clamped.

## 9. Performance notes for large cities

- All overlap tests go through the spatial hash grid (cell ≈ half a block): queries touch only nearby footprints, so cost scales ~linearly with placed objects, not quadratically.
- Intersections render as a single ISM component; scatter should be spawned as ISMs via Static Mesh Spawner (that is why it's a point output). Buildings/poles are individual actors — the expensive part; keep prefab component counts sane.
- Costs are budgeted per attempt: 4 building picks × 3 jitter attempts per lot, capped curved-road walkers, capped branch depth.
- The generator is UObject-free; if very large districts hitch, the natural next step is running `Generate()` in a task and spawning across frames via a custom PCG context (same pattern as the SpawnLevelInstance node's wait loop).
- Sync `LoadSynchronous` of assets happens once per execution; at runtime prefer preloading the building classes.
- Bounds measurement spawns each building class once per execution; with many classes consider footprint overrides for the cheap ones.
- Ground-align traces are per scatter point; disable `bAlignToGround` on flat cities.

## Usage

1. Add a `Scorched City` node, feed it the city area (e.g. Get Spline Data from a closed spline actor).
2. Assign RoadMesh, IntersectionMesh, building Blueprint entries, scatter profiles, pole assets.
3. Wire **Scatter** → Static Mesh Spawner (By Attribute, attribute `Mesh`).
4. Use **OccupiedBounds** with Difference nodes to keep other PCG content out of the city.
