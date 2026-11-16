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
