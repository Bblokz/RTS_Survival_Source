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
- **Curved**: random-walk walkers seeded at grid/curved zone borders (or radially for pure-curved cities). Per segment: heading += rand ± maxTurn (from `CurvedRoadCurvature`), length jittered by `CurvedRoadVariation`, branch chance `CurvedRoadBranchChance`. Walkers stop at city bounds (clamped), on self-proximity, on road collision (with snap-to-node junctions), or entering grid zones.
- Committed edges are **rasterized** into road-width oriented boxes (chunk = road mesh length) in the hash grid — this is what keeps everything else off the roads.
- Export: 4-way mesh instances at degree≥4 nodes; road polylines trimmed back by half the intersection size around them. Splines are linear for grid streets, curved otherwise; the road mesh is chained along each spline with `USplineMeshComponent`s.

## 5. Lots & buildings

Lots are marched along both sides of every edge (width/depth scaled up on major roads), offset by `RoadWidth/2 + RoadSetback + MinRoadBuildingDistance`, rejected if they clip other roads or leave the city. Buildings: lots are filled majors/corners first; a density gate (dense vs sparse zone × `OverallDensity`) decides if a lot is used; a weighted pick combines designer weight, zone preference and size-vs-lot fit (large → major/corner, small → side streets); placement faces the road (`FacingYaw` + allowed rotation offsets), pushes the building to the lot front, jitters laterally, and SAT-tests the inflated footprint against everything.

## 6. Scatter

Per building × per profile: chance roll, count from density × zone, positions sampled in a ring measured from the **footprint edge** (support function, so distance respects building orientation), optional clustering. Checks: exclusion input, city bounds, avoid-roads / avoid-buildings masks using the mesh's scaled radius. `bAlignToGround` line-traces down (ignoring all actors this node spawned) and aligns to the ground normal.

## 6.5 Decals

Three systems share `FScorchedDecalEntry` (material, weight, min/max world size) and spawn as `UDecalComponent`s on one managed anchor actor, projected straight down (ground-snapped without ignoring spawned actors, so they land on road meshes too):
- **RoadDecals**: `SplineRoadDensity` (per 10 m of trimmed road spline, so never under intersections) with road-aligned yaw, plus `IntersectionDensity` (per 4-way).
- **LotDecals**: `LotDensity` per generated lot, kept inside the lot, skipped under buildings; each placement is recorded in a local decal grid.
- **AroundBuildingDecals**: `DensityPerBuilding`, ringed off the building footprint edge; checked against the decal grid so they **never overlap lot decals** (or each other).

Road splines are trimmed per exit direction against the real (unclamped) intersection mesh half-extent — diagonal exits trim up to √2 further — with a small tuck-in so they end flush at the 4-way's edge instead of running underneath it; slivers between adjacent large intersections are dropped entirely. Curved walkers leave junctions exactly along the cardinal exit (curvature starts at segment 2) so their splines connect flush to grid roads.

## 7. Power lines

Per edge: one side chosen, poles marched at `PowerLineSpacing` with `OffsetFromRoadEdge` from the road edge, skipping near intersections. Each step places the **two-pole asset** toward the next valid pole position (yawed along the road), or the **single-pole asset** on broken rolls (`BrokenSectionChance`), endpoints, or when the next position is blocked. Pole clearance squares go into the hash grid; blocked positions produce natural gaps. Author the two-pole asset to span ~`PowerLineSpacing`; after a two-pole section the march skips one spacing so chained assets never double a pole.

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
