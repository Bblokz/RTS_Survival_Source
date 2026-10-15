# Enemy Base Setup PCG Nodes

A family of custom PCG nodes for building enemy defensive positions — bunker lines, fortified
buildings, symmetrical defense rings and whole fortified base areas. Source:
`Source/RTS_Survival/Procedural/CustomNodes/EnemyBaseSetup/`.

They follow the same three-layer structure as the Scorched City node: designer settings, a
game-thread PCG element that resolves assets / spawns actors / emits outputs, and a pure,
UObject-free placement layer that does the deterministic geometry.

## 1. Architecture

| Layer | Files | Responsibility |
|---|---|---|
| Shared types | `PCGEnemyBaseSetupTypes.h/.cpp` | Designer enums & structs, the SAT `FEnemyFootprint`, the `FEnemyOccupancyGrid` spatial hash, and the result structs. |
| Placement engine | `EnemyBaseSetupPlacement.h/.cpp` | `FEnemyBaseSetupBuilder` — bunker rows, obstacle belts (line / arc / double row), barbed wire, sandbag walls, decorator / foliage / decal scatter, all world-space and bounds-aware into one occupancy grid. |
| Element helpers | `PCGEnemyBaseSetupShared.h/.cpp` | `EnemyBaseSetupShared::*` — asset resolution with real bounds measurement, input / aim-target reading, facing resolution, managed actor spawning, and the categorized point-with-bounds / total-bounds / occupancy / foliage / decal outputs. |
| Nodes | `PCGFortifiedBuilding`, `PCGBunkerLine`, `PCGSymmetricalDefenses`, `PCGEnemyBaseSetup` | Thin settings + element wrappers that resolve their assets and drive the builder. |

Every node is deterministic from its `RandomSeed` (the PCG component seed is ignored,
`UseSeed() == false`), runs on the game thread and is non-cacheable (it spawns managed actors).

All placement happens in **world XY**; the element line-traces each placement onto the landscape
when spawning and when emitting points. All overlap tests are SAT tests between oriented footprints
in the shared `FEnemyOccupancyGrid`, so nothing overlaps unless allowed. Structural props (bunkers,
sandbags, hedgehogs, wire) reserve footprints; foliage and decals do not.

## 2. The nodes

### Fortified Building (smallest)
One military building at each input point, with a sandbag wall (flanks / front+flanks / full ring)
and an optional obstacle belt and barbed wire in front.
- **In:** `Points` (one building per point, required), `AimTarget` (optional), `Exclusion` (optional).

### Bunker Line
A row of bunkers at each input point facing a chosen direction, with an obstacle belt and barbed
wire in front and an optional sandbag parapet wrapping the row.
- **In:** `LineCenters` (one line per point, required), `AimTarget` (optional), `Exclusion` (optional).

### Symmetrical Defenses
Selects a symmetric set of positions from candidate points around a defended center (rectangular
mirror, radial ring, or single-axis mirror) and builds a fortified setup at each, facing outward or
inward. Unused candidates pass through.
- **In:** `CandidatePoints` (required), `DefendCenter` (required), `AimTarget` (optional axis /
  facing target), `Exclusion` (optional).
- **Extra out:** `UnusedPoints`.

### Enemy Base Setup (macro)
Fortifies a whole area: wraps it in bunker lines (full perimeter / a front line facing the
`AimTarget` / corner strongpoints), each with belt + wire + sandbag parapet, and drops fortified
interior buildings (at `InteriorPoints`, or scattered) facing outward. Everything shares one
occupancy grid.
- **In:** `Area` (bounds define the perimeter, required), `InteriorPoints` (optional), `AimTarget`
  (optional), `Exclusion` (optional).

## 3. Facing & aim

Every setup faces a world direction (toward the threat) chosen by `EEnemyFacingMode`:
`UsePointRotation`, `FixedYaw` (exposes `FixedYawDegrees`), `AimTowardsTarget`, `AwayFromTarget`.
The Aim modes use the nearest point on the `AimTarget` input (point positions, or a spatial input's
bounds center). Symmetrical Defenses and the macro use the defended center as the facing reference,
so `AwayFromTarget` faces the setups outward.

## 4. Conditional options (enum-driven)

The `EditCondition` metadata reveals only the settings that fit the chosen layout, e.g.
`EEnemyObstacleFormation::Arc` exposes `ArcSpanDegrees` / `ArcRadiusOverride`; `DoubleRow` exposes
`RowSpacing`; `EEnemyPerimeterLayout::Corners` exposes `CornerLineLength`; `FixedYaw` exposes the
yaw. Spacing everywhere is designer-controlled: bunker spacing, obstacle spacing, wire segment
length, minimum sandbag center spacing, distances from the line / building, and decorator /
foliage / decal ring distances.
Sandbag entries additionally expose their Blueprint's local **Sandbag Front Axis** (`+X`, `-X`,
`+Y`, or `-Y`). Each segment is rotated so that axis faces away from the protected building.
Sandbag chaining expands that minimum spacing when required by each entry's measured or overridden
footprint bounds (including scale and pivot offset), keeping mixed assets from overlapping one
another or the building.

## 5. Outputs (points with bounds)

Every node exposes the total bounds of what it placed **and** per-category subsets:
- **TotalBounds** — one point per setup cluster whose bounds enclose everything that cluster placed
  (props + scattered foliage / decals). A `Cluster` int attribute identifies the cluster.
- **Bunkers / Sandbags / Obstacles / BarbedWire / Decorators** — one point per placed prop of that
  category; the point transform is the actor's spawn transform and the point bounds are the
  Blueprint's measured local bounds. `AssetIndex` and `Cluster` int attributes are set.
- **OccupiedBounds** — one oriented point-with-bounds per reserved footprint (all categories), with
  a `Category` int attribute — use with Difference / filters to keep other PCG content out.
- **Foliage** — points with a `Mesh` soft-object-path attribute; wire into a Static Mesh Spawner
  with mesh selection "By Attribute".

So a bunker line placed with sandbags yields, for example, one **TotalBounds** point for the whole
line, plus the **Bunkers** points and the **Sandbags** points as separate subsets.

## 6. Decorators, foliage & decals

- **DecoratorProfiles** — lists of Blueprints (ammo crates, fuel tanks...) scattered in a ring
  around each cluster; multiple profiles let ammo and fuel use different counts / distances. They
  reserve `Decorator` footprints so they never stack on the structure or each other.
- **Foliage** — static meshes emitted on the Foliage pin (ground-aligned, may overlap; it is
  groundcover).
- **Decals** — materials projected straight down around each cluster on one managed anchor actor.

## 7. Bounds measurement

Footprints come from measured real bounds: the element spawns a transient instance of each
Blueprint far below the world, reads `GetComponentsBoundingBox`, caches half extents + pivot offset,
and destroys it — unless `bUseFootprintOverride` supplies an explicit size. The measured local
bounds are also used as the point bounds on the per-category outputs, so the output boxes match the
spawned actors.

## Usage

1. Feed positions in (points for buildings / lines, or a spline / volume area for the macro) and,
   optionally, an `AimTarget` to orient the defenses.
2. Assign bunker / sandbag / hedgehog / wire / decorator Blueprints and foliage / decal assets.
3. Wire **Foliage** → Static Mesh Spawner (By Attribute, attribute `Mesh`).
4. Use **OccupiedBounds** or **TotalBounds** with Difference nodes to keep other PCG content clear
   of the defenses.
