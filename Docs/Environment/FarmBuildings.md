# Destructible farm buildings (Ger_Trains)

Four buildings for the farm part of Ger_Trains. Each one collapses into a pre-fractured geometry collection when it
dies.

| Blueprint | Size | Health row |
|---|---|---|
| `BP_Farm_Dest_FarmHouse` | 11 x 8 m two-storey timber frame, plaster infill, beaver-tail roof, chimney | `Dest_FarmHouse` 1800 |
| `BP_Farm_Dest_Barn` | 18 x 10 m board-and-batten barn, wagon doors in both gables | `Dest_FarmBarn` 1500 |
| `BP_Farm_Dest_Stable` | 14 x 7 m brick cow stable | `Dest_FarmStable` 2200 |
| `BP_Farm_Dest_HayShed` | 10 x 6 m open pole shed with a corrugated roof and straw bales | `Dest_FarmHayShed` 600 |

Content lives in `/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Farm/`:
- `Meshes/`: `SM_<Name>`, `SM_<Name>_Fractured`, `SM_<Name>_Rubble`, and `SM_HayShed_Bales` (which uses `MI_FarmCrops`).
- `Geometry/`: `GC_<Name>`.
- `Textures/`: `T_FarmBuildings_D/N/ORM`, a 4096 trim sheet.
- `Materials/`: `M_FarmBuildings`, `MI_FarmBuildings`.

## How it works
- **Base class.** `BP_Farm_Dest_Building` is a child of `BP_DestructableEnvActor`. Its components are:
  - `BuildingMesh` (intact)
  - `GeometryCollection` (hidden)
  - `RubbleMesh` (hidden, no collision)
  - `PropsMesh`
  - `DirtDecal`
- **Construction script.** It calls `SetupComponentCollisions` and reads `Health` from `DT_RTS_DestructableEnvActorData`,
  using the row in `DataRowName`.
- **Death (`BP_OnUnitDies`).** It runs these steps:
  1. `CollapseMesh` with `GeoCollection`, `CollapseDuration`, `CollapseForce` and `CollapseFX`. All four are class
     defaults that each child sets.
  2. It turns off the intact mesh's collision.
  3. It shows the rubble mesh, so the falling pieces land on it.
- **Navigation.** `SetupComponentCollisions` takes `BuildingMesh` off the navmesh on purpose.
  - At BeginPlay, `RTSNavCollisionEnvObstacle.SetUpNavModifierVolumeAsUnionOfComponents([BuildingMesh], DefaultSceneRoot)`
    sizes the obstacle to the mesh bounds. It is a world AABB, so the rotated yard gets a slightly larger box.
  - On death, `EnableAffectNavmesh(false)` frees the footprint.
  - `RubbleMesh`, `PropsMesh` and `GeometryCollection` have no collision and never affect navigation. A collidable
    hidden rubble box used to cut the navmesh, and it would have kept blocking after death.
  - Verified 2026-10-27 with `Saved/FarmBuildings/nav_check.py`, which measures path length across each building:
    - Editor world (no BeginPlay): every path runs straight through.
    - Simulate: every path detours (house 30 m vs 18 m straight, barn 42 vs 19, stable 31 vs 16, shed 25 vs 19).
    - After `TriggerDestroyActor`: the barn and stable paths run straight again; the living buildings still detour.
  - The centre of a living building still projects onto the navmesh, because the obstacle is the `NavArea_Obstacle`
    area, not a hole.
- **Clean-up.** After `CollapsedGeometryLifeTime` (16 s) the GC component is destroyed. The rubble stays.
- **Collection hierarchy.** Each collection is root -> one cluster per section (wall, gable, roof side, chimney) ->
  pieces.
- **Damage thresholds.** They are `[1e6, 120, 1e6]`. The collapse strain field breaks the root into falling sections,
  and those shatter into their pieces when they hit the ground. Resting pieces shrink away after 6-12 s.
- **Collision.** Piece collision is shrunk by 12 %. Without that, touching and overlapping pieces fling debris across
  the map while they push each other apart.

## Rebuilding
Pipeline: `D:/Blender/BlenderProjects/BlenderTanks/FarmBuildings/`. `ADVERSARIAL_REVIEW.md` there records the art
review and what changed.
1. `python build_atlas.py && python build_pbr.py`. The atlas bands, `BAND_GAIN` (the in-game value correction) and the
   door/fracture items are defined in `build_atlas.py`.
2. `blender -b -P build_meshes.py -- [Name]` (Blender 4.0). This writes the intact, fractured and rubble FBX.
   - Pieces are convex parts, cut into Voronoi cells in `fb_lib.py`.
   - Vertex colour R holds the section id (encoded as section * 16 + 8), and B holds a per-part tint.
3. `render_preview.py` renders previews. `context` composites them onto the in-game screenshot.
4. In the editor, copy `ue_import.py` / `ue_place_farmbuildings.py` to `Saved/FarmBuildings/` and import them from
   there.
5. Run `RTS.FarmBuildings.BuildCollection <.../SM_<Name>_Fractured> <.../Geometry/GC_<Name>> Overwrite`. The source
   is `Source/RTS_Survival/Environment/DestructableEnvActor/FarmBuildings/FarmBuildingCollectionCommands.cpp`.
   - It splits the fractured mesh into its islands.
   - It decodes the sections from the vertex colours, accepting either raw or sRGB-linearised values.
   - It clusters and saves the collection. Rebuilding in place keeps the Blueprint references valid.
6. Placement: `ue_place_farmbuildings.place()`. It is re-runnable and replaces the `FarmBld_*` actors.
   - The yard is rotated -24 deg to run parallel to `TrainTrack_0` and sits about 9 m east of it.

## Gotchas
- `M_FarmBuildings` multiplies base colour by `0.7 + 0.6 * VertexColor.B`. The importer linearises vertex colours, so
  the neutral 0.5 tint reads as about 0.83. Tune overall brightness with `BaseTint` on `MI_FarmBuildings`.
- Testing collapses on Ger_Trains: use Simulate (`LevelEditorSubsystem.editor_play_simulate()`) with the level
  viewport tab in front. In PIE, the mission intro and scripted camera pans take the camera away from the farm.
