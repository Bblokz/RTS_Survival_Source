# Battlefield Area PCG Node

`Create Battlefield Areas` builds one or more closed, non-convex battlefield splines from candidate
points and populates each area as three longitudinal slices:

1. Soviet weaponry and Soviet spoils of war.
2. No man's land containing barbed-wire runs and grouped hedgehogs.
3. German weaponry and German spoils of war.

Trees and decals are optional global populations and may use the complete area. All generated resources
are PCG-managed and are removed when the owning PCG component cleans up or regenerates.

## Pins

- **In** (Point, required): candidate area centers. Candidate order is shuffled deterministically.
- **Excluded Bounds** (Spatial, optional): the complete proposed polygon is rejected if it intersects this
  data. Individual placements also check it.
- **Area Bounds** (Point): one AABB point per accepted area with `BattlefieldAreaIndex` metadata. The spawned
  `PCG_BattlefieldAreaSpline_*` actor is the exact non-convex boundary.
- **Placements** (Point): actor and decal placements with `BattlefieldAreaIndex`,
  `BattlefieldPlacementRole`, and `BattlefieldSlice` metadata.

## Shape and slices

The two long boundary edges are sampled into `BoundaryStationsPerSide` stations. Random width variation
creates an irregular outline; `Min/MaxBoundaryIndentations` and `BoundaryIndentDepth` push selected
interior stations inward, creating reflex corners instead of a convex ellipse/rectangle. Boundary points
stay at one reference Z because the spline is used only as an XY polygon.

`SovietSliceProportion`, `NoMansLandSliceProportion`, and `GermanSliceProportion` are normalized, so they
do not need to add up to one. Soviet weaponry faces the area's forward axis and German weaponry faces its
inverse. `SovietWeaponrySpread` and `GermanWeaponrySpread` independently control how much of each faction
slice is used.

## Grouped obstacles

Barbed-wire entries declare the local axis along which the Blueprint is authored. One weighted entry and
one uniform scale are selected for a complete run; measured piece length, the configured piece gap, and
global placement clearance determine the end-to-end step. A run is committed only when every requested
piece fits, so it never contains collision-created holes.

Hedgehogs are generated as groups. Every new member is placed near an already accepted member using
`Min/MaxHedgehogSpacing`. Like wire runs, a group is committed atomically only when its requested count
fits.

## Collision and terrain

Blueprint classes are transiently spawned once per execution to measure their real component bounds.
Every accepted actor and decal reserves an oriented 2D rectangle. SAT overlap tests plus
`PlacementClearance` prevent overlaps across all section-specific and global populations. A conservative
radius additionally keeps the complete footprint inside the non-convex spline.

Every actor and decal first resolves the `ALandscapeProxy` covering its XY, then traces only that Landscape
from above its complete component bounds to below them. This brackets both sculpted peaks and eroded
depressions even when an input point has an unrelated Z. Rocks,
buildings, or other static geometry above the terrain cannot occlude or supply placement Z. Actors can align
their up axis to the landscape normal; their measured lower bounds plane is seated on the local landscape
tangent instead of lifting the downhill corner to the center height. `GroundTraceUp` and `GroundTraceDown`
provide extra safety margins beyond the full Landscape bounds, while `MaxGroundSlopeDegrees` controls which
slopes are valid.

## Setup

1. Add `Create Battlefield Areas` to a PCG graph and provide candidate points.
2. Configure area count and dimensions, then adjust indentation controls for the desired outline.
3. Assign the Soviet/German weaponry, wire, hedgehog, spoils, tree, and decal lists that are needed.
4. Set footprint overrides only for Blueprints whose decorative components make measured bounds unsuitable.
5. Feed roads, bases, protected volumes, or other blockers into **Excluded Bounds**.
