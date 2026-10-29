# Track smoke per ground surface

Vehicle track smoke (`NS_Track_Smokes`) takes its tint and opacity from the ground under the vehicle:
orange and thick on sand, darker on grass, grey and faint on concrete.

## How it works

1. **Bake (editor, per map).** `RTS.GroundSurface.BakeSurfaceMap` writes `SurfaceMap_<MapName>`
   (`UGroundSurfaceMap`, in `/Game/RTS_Survival/Blueprints/Environment/GroundSurface/SurfaceMaps`). It stores one
   `EGroundSurfaceType` per 4 m cell (Dirt, Grass, Sand, Rock, Concrete), sampled at 5 points per cell.
   - **Landscape:** paint-layer weights, mapped to surfaces by layer name (`Surfaces[].LayerNames`). Unlisted layers
     count as `UnlistedLayerSurface` (Dirt), and the bake log lists them.
   - **Meshes on the landscape** (roads, ground paths, rails) override it where they lie. They match by `MeshRules`
     (actor class and/or static mesh, first match wins, `None` excludes a mesh), or by an actor tag such as
     `GroundSurface_Concrete`. Hits more than 3 m above the landscape count as overhead (train wires) and are ignored.
     Mesh collision is switched to query-only for the bake and restored afterwards.
2. **Runtime.** `UGroundSurfaceSubsystem` loads the map's surface map at begin play. A lookup is one array read.
3. **Smoke.** `UChassisAnimInstance::UpdateTrackVFX` (every 0.33 s per vehicle, only while the smoke is on) reads
   the look under the vehicle. It eases towards that look (`SmokeLookBlendPerUpdate`) and writes
   `User.SmokeSurfaceLook` (Vector4: RGB tint, A opacity) only while the value changes.
4. **Niagara and material.** Each emitter copies `User.SmokeSurfaceLook` into `Particles.DynamicMaterialParameter`
   at particle spawn, so particles already in the air keep their colour and a ground border fades over the smoke's
   lifetime. `M_TrackSmoke` multiplies the smoke texture by the dynamic parameter's RGB (emissive) and the opacity by
   its A. The parameter's default (0.88, 0.682, 0.55, 1) is the old fixed tan.

Cost: no traces at runtime. Per moving vehicle there is one grid read every 0.33 s, and a Niagara parameter write
only for the few updates after the ground changes.

## Tuning and baking

- Project Settings -> Game -> **RTS Ground Surfaces**: layer lists, smoke tint/opacity per surface, mesh rules,
  cell size, blend speed.
- Bake headless: `Source/Tools/GroundSurface/bake_surface_maps.sh [map paths]`. The default is the five German
  missions. Previews go to `Saved/GroundSurface/SurfaceMap_<Map>.bmp`: green grass, orange sand, brown dirt, grey
  rock, white concrete, black none. The log prints `RTS_SURFACE_MAP MESH` lines that show which meshes matched
  which rule.
- Bake again after repainting a landscape or moving roads. A map without a surface map keeps `NoSurfaceSmokeLook`.

## Limits

- Buildings placed at runtime and ground that changes during play (craters, radixite blight) are not in the bake.
- "Less smoke" is lower opacity, not fewer particles: concrete smoke still costs the same overdraw.
