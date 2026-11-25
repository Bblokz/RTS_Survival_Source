# Horizon skirt: end-of-map planes on campaign maps

The survival map SM_Desert_01 hides the end of the world with flat sand planes, `SMD_HorizonSkirt_*`. They use the material `M_SM_HorizonSkirt`, which samples the rippled-sand textures that the DesertPaintable landscape's Sand layer also uses. The planes have no collision, navigation or shadow, are hidden from scene captures (so the minimap ignores them), and run out to 1.5 km. The survival landscape's rim falls to the planes' height, so there is no seam.

On 2026-11-16 the user asked for the same planes on Ger_IntroMission and Ger_FirstMission.

## Ger_IntroMission (simple boundary)

The rear-area tools build this map's skirt: `Source/Tools/IntroMission/rear/rear_build.py`, `step_skirt()`. See `Source/Docs/Campaign/IntroMissionRear.md`.

- **Strips:** four strips (`REAR_Skirt_*`) at z 97 around the landscape rectangle, tucked 4 m under it.
- **Rim:** the outer 5 m of the landscape rectangle eases down to the strips. This band lies beyond the playable blockers.
- **West-corner holes:** fill planes sit at each hole's border height. The landscape there is not eased, because the user's dressing sits on those borders.

## Ger_FirstMission (irregular, uneven boundary)

The landscape covers 2,795 of 3,864 sections in its rectangle, on a staircase outline. The boundary height runs from 1.4 m to 21 m (mountain flanks, lake beds). Before this work, about a third of what the camera can see (fog-of-war square ±450 m plus the forward view) traced to nothing.

Flat planes alone would leave cliffs and steps, so the existing terrain was not edited. Instead:

- **Ring:** 569 new landscape sections fill every place the camera saw void, up to 3 sections (45 m) from the existing landscape.
  - Heights continue the nearest boundary height and ease down to z 144 over 40 m.
  - Paint continues the nearest boundary paint and fades to Sand over 12 m.
  - Shared edge vertices keep their values. Re-exporting showed 0 changed height or paint vertices on the existing 2,795 sections.
- **Planes:** `SKIRT_Plane_*` at z 142 under the ring's outer flat edge, in the MapSkirt folder.
  - Four strips around the extended rectangle out to 1.5 km.
  - 14 fills for the more distant void inside the rectangle.
- **Left alone:** places the camera sees covered by water planes or mountain meshes, including the lake-bed holes.
- **Guards:** 22 `SKIRT_Guard_*` blocking volumes, 7–11 m high, cover the ring sections on the playable side of the map's blockers. So the playable area does not grow.
- **Result:** a 10 m trace grid over the camera-visible area finds no cell that is neither ground nor under a skirt plane (3,137 void cells before).
- **In game:** pictures at every edge and corner from the camera limits show ground everywhere. Two cosmetic leftovers:
  - a faint line where ring meets skirt;
  - stretched texture where the ring falls steeply from a 20 m mountain edge, in the far south-east corner at the camera limit.
- **Map file:** grew from 59.3 to 66.7 MB. A copy of the map before this work is at `Saved/MapSkirt/Ger_FirstMission_before_skirt.umap`.

### Tools (`Source/Tools/MapSkirt`, any map)

1. **`skirt_survey.py`:** read-only commandlet survey of landscapes, blockers, fog-of-war extent and edge meshes. Set the map with the `SKIRT_MAP` environment variable.
2. **`skirt_export.py`:** exports the full landscape (heights, paint, grid), actor positions and blockers to `Saved/MapSkirt/<Map>/`. Needs the DebugGame build, because it uses `URTSShortsLandscapeLibrary.ExportLandscapeRegion`.
3. **Void trace grid:** produces `void.json`, a 10 m grid of trace results over the camera-visible area, plus the water planes. Run `skirt_void_trace.py` in a throwaway editor on the map.
4. **`skirt_ring_plan.py <Map>`:** offline (numpy). Writes the ring, planes and guards plan plus the extended-grid height and paint files.
5. **`skirt_ring_apply.py`:** exec'd in a throwaway editor with `SKIRT_NAME` set. It adds the ring sections one row run at a time (lake holes are never filled), imports the files, and places the planes and guards. It is re-runnable.
6. **`skirt_plan.py`:** the simpler rim-easing planner. It suits maps whose edges sit near one height.

Rules both maps follow: never ease ground on the playable side or within 8 m of an actor, check the void grid again after applying, and wait for the navmesh to finish building before saving.

## Ger_Berlin (complete square, raised rubble rim, own material) — 2026-11-17

Tools: `Source/Tools/BerlinMap/skirt/`. Working files and pictures: `Saved/MapSkirt/Ger_Berlin/`.

### What the survey found

- **Camera:** the pawn is clamped to the fog-of-war square, ±500 m on both axes (not to the 520 m x 1000 m play area). Read in PIE: arm pitch -70, the player tilts it up by at most 18 degrees (-52) and turns it freely in yaw, widest zoom 5200, field of view 90, pawn height constant (z 23).
- **What that camera sees:** at the widest zoom and flattest pitch the view reaches about 60 m ahead of the pawn and 85 m to each side. The landscape (±598.5 m) ends 98.5 m past the camera limit, so the gameplay camera does not see the end of the map from any edge or corner, at any yaw (pictures `shots_before/`). With fog of war on, unexplored ground out there is near black anyway. The planes are a guard for anything that looks further: the cinematic camera settings (pitch 45), location intros, later changes of the limits.
- **Sky:** the map has no sky or fog behind the ground: past the landscape the picture is black.
- **Rim:** the landscape is a complete 19 x 19 square. Its outer band is one rubble plateau, 2.2 to 6.4 m high (median 4.2 m), noisy. Paint on the rim: 81 % rubble, 13 % ash.
- **What crosses the rim:** only the Spree trench (bed at -5 m), in the west at x 122-150 m and in the north at y 223-245 m; the water plane (`BERW_Spree_Water`, z -190) ends on the landscape edge. The rail lines end at the play edge (the last track pieces lie at y 500 m in the east and x ±260 m), not at the rim. Backdrop ruins stand out to 38 m from the rim.

### Method: rim easing to the plateau height (no ring)

Nothing is missing from the landscape rectangle and the rim is one plateau, so there is no void inside the rectangle to fill and no reason to add sections (the map file is already over 100 MB). `skirt_plan.py` was not used as it is: it eases the rim *down* to just under its lowest point, which here is the river bed.

- **Skirt height T = 422.66** (the plateau's median over the outer 30 m). Planes at T - 3 cm.
- **Flat band:** the outer 10 m of the landscape is exactly T, so the planes (tucked 3 m under it) never show a step or a gap at any landscape LOD.
- **Ease band:** 10 to 30 m from the rim, smoothstep between the untouched ground and T (raised where lower, lowered where higher; largest lowering 2.2 m).
- **River mouths:** where the trench crosses the band, the bed rises to T over 70 m instead (largest raise 9.3 m, about 13 degrees). The channel silts up with rubble and ends about 35 m before the rim; the water plane is untouched and ends at the new bank.
- **Paint:** fades to pure BerlinRubble with the same weight, because the plane material is that layer.
- **Guards:** a vertex within 8 m of an actor's bounds, an instanced mesh (per instance, not per container actor) or a mesh-hit cell of the trace grid keeps its ground; the weight fades in over the next 10 m. Nothing stands within 18 m of the flat band.
- **Changed:** 139,568 height vertices and 138,027 paint vertices, none further than 69 m from the rim, none inside the blockers' rectangle. A re-export after applying, and again after a fresh load, matches the plan vertex for vertex.
- **Planes:** `SKIRT_Plane_PX / NX / PY / NY` in the folder MapSkirt: engine Plane, no collision, no navigation, no shadow, hidden in scene captures, out to 1.5 km.
- **Not needed:** guards (no ground was added) and fills.

### Material: `M_Berlin_HorizonSkirt` / `MI_Berlin_HorizonSkirt`

In `/Game/RTS_Survival/Materials/ground/Landscape/Berlin/`, built by `berlin_skirt_material.py` (`M_SM_HorizonSkirt` is unchanged). It is the landscape's BerlinRubble layer on world XY, node for node: albedo at 5 m tiles mixed with the 0.23 x sample by the macro noise, roughness from albedo alpha, the rubble normal, and the colour variation of `MF_RTS_ColorVariation_Landscape` rebuilt on world position (the function reads landscape UVs, which a plane has not). It is lit, so the sun and its smoke-pall light function fall on both sides of the join alike.

Past the rim, by distance outside the landscape square:

| Parameter | Default | Effect |
|---|---|---|
| `FadeLength` / `FarBrightness` | 50000 cm / 0.02 | Brightness falls (quadratic) to near black 500 m past the rim: with a black sky and no fog the ground goes dark toward the horizon instead of ending in a line. 800 m left a lit plain against a hard horizon. |
| `SootSize`, `SootDark`, `SootLight`, `SootStrength`, `SootFadeIn` | 70000, 0.5, 1.25, 0.7, 15000 | Drifts hundreds of metres wide, faded in over the first 150 m so the join itself is the pure landscape look. |
| `FlattenLength` | 30000 | Roughness to 1, normal to flat, specular to 0: no glints at grazing angles. |
| `RimHalfSize` | 59850 | Half the landscape's side. |

Cost (`get_statistics`, same run): 380 pixel-shader instructions, 11 texture samples on the shared wrap sampler; `M_SM_HorizonSkirt` 306 instructions and 6 samples; `MI_RTS_Landscape_Berlin` 236 and 41. It only draws when the view reaches past the rim.

### Result

- **Void grid** (10 m cells over ±1500 m, traced; plane footprints tested apart because the planes have no collision): before 76,440 cells with nothing (8,640 of them within ±750 m), after 0.
- **Unchanged:** 7,029 actors before and after a fresh load (plus the 4 planes), the four boundary blockers, the nav bounds. Nav probe (which 10 m grid points lie on each agent's navmesh): 14-15 of 15,625 points differ from the survey load in each later load (a different set each time, the same points for all four agents), all in the Tiergarten among the tree nav blockers (x -110..50 m, y -400..-100 m), 190 m and more from the eased band. The cause was not pinned down (the blockers' scales are the same in every load); no probe point outside the park changed.
- **Navmesh on disk:** the map held a `RecastNavMeshDataChunk` before (the open incident of 2026-11-14) and still does. No `RebuildNavigation` was run; the save waited for the on-load build to go idle.
- **Map file:** 119,059,586 to 119,008,883 bytes. Copy from before: `Saved/MapSkirt/Ger_Berlin_before_skirt.umap`.
- **Pictures** (`Saved/MapSkirt/Ger_Berlin/shots_before`, `shots_after`): `edge_*`, `corner_*` at the camera limits (default view and turned outward at the flattest pitch), `river_*`, `rail_*`, `fog_*` with fog of war on; `cine_*` at pitch 45 (beyond the gameplay limit) show the horizon; `x_join_*` are taken from beyond the camera limit to look at the join itself.

### Traps met

- `MaterialEditingLibrary.delete_all_material_expressions` leaves about half of the nodes behind per call: loop until the count is 0.
- An exclusive-open lock test on the .umap from inside the editor that has it loaded always says locked (its own linker). Test from outside that no other UnrealEditor process runs, then judge by the save result and the file time.
- Headless `-game` recorder runs of other sessions on Ger_Berlin hold the .umap: the save needs a moment with none running.

## Ger_FrozenLake (one mesh ring, north backdrop) — 2026-11-24

Tools: `Source/Tools/FrozenLakeMap/fl_backdrop_gen.py`, `fl_backdrop_build.py` and `run_backdrop_build.py`. Details are
in `Source/Docs/Campaign/FrozenLakeMission.md`, "Build status: Svarog crater and dry lakebed".

- **No skirt before.** The landscape is a complete 756 m square. The camera square is ±293 m, so the gameplay camera
  never sees the end. The camera tour can.
- **One mesh, not planes.** `FL_Backdrop` rings the whole landscape and is built with the north backdrop: the dry
  lakebed and the Svarog crater.
  - East, west and south it continues the landscape edge height, tucked 30 cm under it, and eases to z 150 over 40 m.
    It is painted as burnt bog there.
  - Cells hidden under the landscape are left out.
  - The MapSkirt tools were not needed: the edge is one plateau (1.2-2.3 m) with no void inside the square.
- **Fog of war past the edge.** `MPP_FogOfWar_01` samples its render targets with a clamp sampler. Ground past the map
  edge therefore gets the fog state of the nearest edge texel: black unless that edge is explored or revealed. A tour
  shot that shows a backdrop needs a `RevealRadius` that covers the edge it looks across.
