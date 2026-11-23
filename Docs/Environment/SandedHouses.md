# Sanded Fachwerk ruins (desert maps)

Six ruins of the farm kit's Fachwerk house (`BP_Farm_Dest_FarmHouse`, the house on Ger_IntroMission), abandoned for
decades on desert sand. They were picked from the v01 concept sheet on 2026-11-22: 1, 2, 4, 7, 9 and 12.

| # | Blueprint | Read from the camera |
|---|---|---|
| 1 | `BP_SandedHouse_Wehe` | Roof intact but holed; a dune ramps up the right gable to the eaves |
| 2 | `BP_SandedHouse_Sattelbruch` | Ridge sagged into a V with a ragged hole; a saguaro grows out of it |
| 4 | `BP_SandedHouse_Gerippe` | No tiles, bleached rafters, half the upper floor dropped onto a dune inside, cacti between rafters |
| 7 | `BP_SandedHouse_Schieflage` | Whole house sunk and tilted 15 deg; left end buried to the upper windows, crack down the front |
| 9 | `BP_SandedHouse_Mauergarten` | Ragged ground-floor walls around a cactus garden, chimney stack and joists |
| 12 | `BP_SandedHouse_Kakteenhaus` | Standing house; saguaros through three roof holes and a window, scoured front, brick gables |

Content: `/Game/RTS_Survival/Blueprints/Environment/Desert/SandedHouses/`.

## Sand takes the map's ground colour (RVT)
The sand is its own static mesh per ruin, `Meshes/SM_SandedHouse_<Name>_Sand`. Its material
`Materials/MI_SandedHouse_Sand` is a child of `M_FieldTrench_Bank` (see `FieldTrenchBank.md`):
- **Base colour:** `RVT_Environment`, so the sand takes the colour of the landscape under it on any map with an RVT
  volume. Without a volume it falls back to `FallbackColor`.
- **Vertex colour R:** paler drift up the mound (`FreshTint`), using the same values as `MI_BuriedArmour_Sand`. The user
  preferred keeping it close to the ground colour.
- **Vertex colour G:** foot blend; toward the foot, the landscape's RVT normal takes over so no crease shows.
- **Skirt:** the outer edge drops to -4 m, so the mound also sits on uneven ground.

The house shell uses the farm atlas (`MI_FarmBuildings`) and carries no sand colour. All sand is sand-mesh geometry.

## Blueprint layout
`BP_SandedHouse_<Name>` (Actor, nothing destructible):
- `House`: the shell mesh. `Sand`: the mound. Both use complex-as-simple collision on WorldStatic. They block
  Visibility, Projectile and the enemy and player traces, overlap BuildingPlacement, and do not affect navigation.
  Camera is not blocked, so the RTS camera does not pop over the roofs.
- `Plant_NN`: existing meshes, with no collision.
  - Saguaro: `SM_Desert_Cactus1-4`.
  - Prickly pear and cactus bushes: `SM_RTS_CactusBush_*_Green`.
  - Agave: `SM_RTS_CactusBush_Small_Blue`.
  - Thistle: `SM_DeadThistle_A-C`.
  - Each is scaled to a target height per kind.
- `NavBlock_NN`: hidden boxes with no collision, used as dynamic nav obstacles (`NavArea_Null`). They cover the shell
  and any sand steeper or higher than about 1.2 m.

## Pipeline
- **Blender 4.0:** `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/SandedHouses/Model`.
  - `sh_lib.py` builds the farm kit's `farmhouse()` parts. It imports `FarmBuildings/build_meshes.py` and never edits it.
  - Damage: Voronoi `shatter` with a kill field (broken faces get the atlas fracture band), `warp` (sag, tilt), `move`,
    and window swaps.
  - Sand: a numpy heightfield, decimated with vertex colours. Buried house faces are culled.
  - Plants and blockers go to JSON. Previews come from a Cycles render at the RTS camera.
  - One spec per ruin in `ruins/rNN_<name>.py`. Run it with
    `blender.exe -b --factory-startup -P ruins/r01_wehe.py [-- --no-render]`.
  - Outputs go to `Export/`.
- **Unreal:** `Source/Tools/SandedHouses/ue_sanded_houses.py`, run in a throwaway full editor on Ger_IntroMission.
  - It imports the meshes, builds the MI and the Blueprints, spawns the ruins on flat spots and takes shots to
    `Saved/SandedHouses/shots`. It never saves the map.
  - Env flags: `SH_ONLY`, `SH_NO_SHOTS`, `SH_SKIP_IMPORT`.
  - Blender to Unreal coordinates: x*100, -y*100, z*100, and yaw becomes -yaw. The report checks the Y flip per ruin.

## Gotchas
- **Polynomial smooth max:** the classic version adds k/4 wherever both inputs are 0, which leaves a flat 20 cm plateau
  with a hard rim. `sh_lib.smax` shrinks the blend width with the lower input.
- **Hard steps in the sand field:** a boolean mask outside the walls shows as a dark seam in Unreal. Use `falloff`.
  Switch between interior and exterior sand at the inner wall faces (`inside_house`), where the walls hide the step.
- **Shared GPU:** with several Blenders and editors open, Cycles runs out of GPU memory. `sh_lib.preview` retries on the
  CPU (`SH_CPU=1` forces it).

## Placed on Ger_IntroMission (2026-11-22)
Five ruins are placed on open, flat ground away from props, paths and the mission flow. They are labelled
`SandedHouse_<Name>` in the outliner folder `SandedHouses`.

| Ruin | Location (cm) | Yaw |
|---|---|---|
| Wehe | (-12000, -19000) | 0 |
| Gerippe | (0, -21500) | 35 |
| Mauergarten | (-25000, -15500) | -20 |
| Kakteenhaus | (2000, 16000) | -15 (cactus slope faces the camera) |
| Schieflage | (-1000, 21500) | 75 |

Sattelbruch is not placed. The map was saved from the shared editor after `RebuildNavigation`; the navmesh generates
`DynamicModifiersOnly`, so the NavBlock boxes apply at runtime. The backup is in
`Saved/SandedHouses/Ger_IntroMission_before_ruins.umap`.

**Trap: the south-west quadrant (x < -10400, y > 2600) is backdrop mountain.** `S_Desert_Mountain_02` and `_04` have
no collision and sit on flat landscape, so ground traces read the area as empty, flat land. Check the
StaticMeshComponent bounds (`SystemLibrary.get_component_bounds`), not traces or actor bounds. Actor bounds include the
origin for many BPs.

## Not done
- **Collision and nav are untested in play.** The shots are editor-viewport captures only. Units will wade through the
  low drift ring, because only sand above about 1.2 m is blocked.
- **Back-lit mound rim:** against the sun, a slightly more saturated rim shows at the mound foot. It does not show in
  the game or top-down views.
- **Sand wear on the house:** the shell has none, apart from Kakteenhaus' brick gables (which show faint course seams
  where they were sliced).
- Wall holes and death or collapse.
- Placement on any map.
- Crushing the plants.
- Destruction via `DT_RTS_DestructableEnvActorData`.
