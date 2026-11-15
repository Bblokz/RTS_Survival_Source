# Ger_PripyatMe410: the map

`/Game/RTS_Survival/Maps/GerMissions/Ger_PripyatMe410`, built 2026-11-14 from the v10 sketch (`PripyatMission.md`,
`MapDesigner/Pripyat/pripyat_01.layout.json`). Landscape, paint, water, foliage and dressing only: no units and no
mission scripting. Sites the mission will arm carry earthworks, cover and an editor-only note (`PRP_Note_*`).

## Rebuild

All tools are in `Source/Tools/PripyatMap`. The map is built in a throwaway editor that runs
`pripyat_editor_server.py` (file-driven: `python pripyat_cmd.py <script.py>`; command folder `Saved/Pripyat/cmd`).

```bash
python Source/Tools/PripyatMap/pripyat_terrain_gen.py    # heights, 9 paint layers, masks, layout -> Saved/Pripyat/terrain
python Source/Tools/PripyatMap/pripyat_dress_gen.py      # every tree, prop, site object, effect -> Pripyat_dress.json
python Source/Tools/PripyatMap/pripyat_cmd.py Saved/Pripyat/c_build2.py --timeout 2400   # in the editor
```

`c_build2.py` runs, in order: `tune_landscape_material`, `step_landscape`, `step_fit_level_actors`, `step_water`,
`step_airfield`, `step_dress`, `step_fx`, waits for the navmesh, runs `pripyat_nav_check.run()` and saves.
Set `BUILD_FULL = False` in front of it to redo only the dressing.

- Generated actors carry the label prefix `PRP_` (`PRP_D_*` Blueprint actors, `PRP_I_*` instance holders,
  `PRP_FX_*` effects, `PRP_Note_*` notes, `PRP_Water_Bog`) and sit under the outliner folder `Pripyat/`. The airfield
  kit actors keep the kit's own `Prypyat_*` labels. Each step clears and redoes its own actors.
- In-game review shots: `pripyat_ingame_shots.py` (play session, fog of war off). Scene captures of this map come out
  near black; only the unlit top view (`Saved/Pripyat/c_top.py`) is usable from the editor.
- The landscape material is built by `Source/Docs/Landscape/ls_pripyat_build.py` (a copy of the Berlin builder on
  photo textures that are already in the project). The nine layer infos have to be created in a headless
  `UnrealEditor-Cmd -run=pythonscript` run (`Saved/Pripyat/headless_layerinfos.py`): in the unattended full editor the
  console `set ... LayerName` command does nothing, and importing a landscape with unnamed layer infos crashes
  (`!HasTargetLayer`).

## Size and frame

- Play area 670 m (north-south) x 700 m (east-west), as the sketch says. Landscape 14 x 14 components (882 m), so
  about 100 m of camera margin a side.
- Heights: the bog water table is z = 0. One water sheet (`MI_Pripyat_AlgaeWater`, a child of the shore-foam water)
  lies at z = 0 under the whole landscape; every pool, ditch, river and lake is ground carved below it.

| Level | z (cm) |
|---|---|
| Open water bed | -165 to -300 |
| Bog ground | about +20 (pools down to -50) |
| Causeway crowns | 215 (south logs) to 300 (north rail) |
| Dry rises | 300; start rise 450; island 480 |
| Bench with the ring road | 750 |
| Ridge top (airfield) | 1240 |

## How ground units are kept off the bog

All four nav agents stop at 44 degrees, and the editor navmesh confirms the result (`pripyat_nav_check.py`: 0
problems). There is no nav volume:

- Every causeway, rise and the island stands at least 2.6 m above its foot behind a 60 degree bank with no level
  verge. A bank lower than about 2.4 m can be walked down obliquely on the 1 m landscape grid, so where the ground
  beside a road is higher than that (the hill's lower talus, shallow bog) a **side ditch** is cut 2.6 m below the
  crown. The ditches fill with water and read as the borrow ditches of the embankments.
- The hill has two scarps (talus, scarp, bench, scarp, top); only the roads cross them.
- The ford on the east lane is a gravel bar 20 cm under water with deep water on both sides.
- The generator checks this with a flood fill over the landscape triangles before anything is imported
  (`Saved/Pripyat/terrain/check.txt`).

## Deviations from the sketch

| Sketch | Map | Why |
|---|---|---|
| Runway 173 x 20 m, centre (11, -11) m | The airfield kit: 200 x 30 m runway with six bays, tower, hangar and dumps (216 x 140 m), heading 45 as drawn, centre (-4, 4) m | The kit must not be scaled; moved 15 m to the south-east so the bays stay on the top |
| Ridge top r 122 m | r 127 m | Room for the bays; the ring road and the bench did not move |
| Bench off the ring road impassable | The whole 45 m bench is level, open ground | Flat ground cannot be blocked without walls; it gives the ring fights room and cover |
| Boardwalk 4.5 m wide | The dyke under the 4.5 m plank tiles is 2.5 m wide | At 4.5 m the light and medium tank agents could use it; at 2.5 m only infantry gets a path (nav check) |
| Compound sites (command post, garrison, spotter) | Moved into the band south-east of the airfield kit | Their sketch positions lie on the kit's hangar and apron |
| Sketch water outlines, straight rivers | Rivers meander up to about 19 m (straight only at the two bridges and the ford), two extra oxbows, ragged flooded strips, an arm from the island lake to the north river | Critic round 1: the sketch outlines read as drawn with a ruler |
| Round hill | The bench edge swings up to 17 m in lobes with three gullies; the talus foot wanders more; the ring road and ramps are unchanged | Same |
| West road starts at the start rise | It enters from the west map edge, with a 26 m checkpoint pad | The player arrives from somewhere; room for a fence, hut and chicane |
| Rail on the whole north lane | Rails from the edge to the bridge only; past it the rails are lifted (salvage stack = metal) | No railway turns on a 15 m hairpin |
| (not in the sketch) | A dry sand rise with a khutor ruin beside the east lane, at about (-2, 257) m | A second beat on the 300 m east lane |

## What is on the map

Counts from `Pripyat_dress.json` (final build): 880 Blueprint actors, about 46,900 instances in 92 groups, 26 effects, 20
road splines, 43 decals, 62 notes; plus the 36 airfield kit actors.

| Site | What stands there |
|---|---|
| Start rise | Two log houses with shell holes, sweep well, stork poles, partisan memorial beside the road, tractor, pickup, the Pinsk sign |
| Checkpoint | Concrete fence with a gate across the pad, staggered road blocks, hedgehogs, guard hut, bus, two sandbag nests |
| AA-W / N / E / S | Two horseshoe gun pits each and wire on the bog side; W sandbags and stores, N wire enclosure with containers and ammunition stacks, E concrete wall, bunker and dragon's teeth, S hay shed with log breastworks |
| Ring | A worn two-track in 16 segments of two widths, a crushable pole line, motor pool (NE), signals post with a leaning mast (SE), dug-in picket with a bunker (SW), three hamlets of log houses |
| Ridge top | The airfield kit (closed state: barricades), crater scars, weeds, scrap in the bays, a stripped Ju 87; compound street with barracks, command bunker, workshop hangar, generator building, kolkhoz silo, radio mast, fenced yards, rim fence, west gate with two bunkers, pits at the other ramp heads; Radixite seep and aircraft boneyard (metal) |
| North lane | Rail track to the blackened rail bridge (deck, trusses, sunk span and pier), lifted rails beyond, fuel siding, rail halt on the pilot rise, fuel depot (tanks, wire, guard pit) |
| East lane | Sand ridge road with a two-track, ford with shoals and marker posts, khutor of three log houses, kill zone of six wrecks (one to scavenge), lone IS-3 |
| South lane | Log corduroy, log bridge, drowned trees at the bank foot, peat-cutters' yard, peat dam to the far Radixite |
| Island | Packed-sand strip, hangar under a net, generator building, command bunker, fuel tanks, mast, three AA pits |
| Demeter pod | The pod (Berlin mesh) half sunk on its hummock, reached by the plank walk (infantry only) |
| Sunken Demeter pod | `SM_Pripyat_SunkenDemeterPod` in the algae shallows 50 m south of the hummock (-24600, -30100, z 0): Act 1 clue, optional intel / lore find. In the generator; placed on the map with `pripyat_place_sunken_pod.py` |
| Bog | 2,700 instanced drowned trees, shrubs, logs, reed belts, cattails, tussocks, snags; two smouldering peat patches with smoke, pits, ash and a cottage each; five downed Ju 87s at the pilot sites |

Destructible or crushable: every Blueprint actor above (268 of them carry the shell-hole component, counted on the map; about 500 trees and 100 poles are crushable).
The instanced pieces are decoration (collision only on trusses, masts, the silo and the Ju 87s).

## Critic rounds

Three rounds with an adversarial critic agent; critiques and my answers are in `Saved/Pripyat/critic`
(`critique_r1..r3.md`, `response_r1..r3.md`), shots in `Saved/Pripyat/shots/r1..r3`.

| Round | Realism | Fun |
|---|---|---|
| 1 (first dressed pass) | 4 | 4.5 |
| 2 | 5.5 | 6 |
| 3 | 6.5 | 7 |
| Verification after the third improvement round (`verification_r3.md`) | 7 | 7.5 |
| Sign-off after a polish pass (`signoff.md`) | 7 | 7.5 |
| Final regrade of the finished map, fresh sweep of 42 views (`final_regrade.md`) | 7.5 | 7.5 |

After that verification a polish pass closed its leftovers, and the critic signed the landscape and dressing pass off
as complete for a map without units and scripting (`signoff.md`, grades unchanged at 7 and 7.5):

- a belt of reeds, tussocks and brush on every bank toe at the waterline (26,000 instances), and one more smoothing
  of the water outline;
- the fallen span at the north bridge sunk and tilted (the stone pier stub still stands in the water);
- the south corduroy as one continuous run, loose trunks only on real bends;
- keep-outs at the three ramp heads, so no tree stands on the west-gate fence line;
- ford posts on both edges of the bar; the memorial on the start rise (it stood in a ditch); a sign moved out of
  the ford; the salvage rails on the trackbed;
- after the sign-off: the surviving macadam segments on the ring are a wider worn two-track (the macadam tiles
  stayed pale under any tint of their material);
- waterlines: all ground at or below +45 cm is smoothed among itself, so the hard outer edge of a moat or ditch is a
  3 to 4 m slope and the waterline follows it. Cells above +45 cm take no part, so every blocking bank keeps its
  height and angle;
- brush on the bank faces of rises and causeways and a brush hedge along the rim of every rise;
- the Radixite seep moved off the kit platform to the rim.

The final regrade found nothing plainly broken in the 42 views and calls landscaping, layers, foliage and details
complete. Its cosmetic residue: a toothed run on the khutor north-east shore, short steps on the AA-N rim, the rim
hedge standing in single file in a few places, and the compound's value outliers.

## Compound material pass

After the final regrade, the compound's value outliers were toned into the marsh palette. Only materials changed: no
actor was moved, added or removed (1247 actors, 46,897 instances, 268 actors with the shell-hole component, nav check
0 problems before and after). `map_materials()` calls `compound_materials()`, which makes the assets below.
`apply_compound_overrides()` sets the per-actor overrides; `step_airfield` and the Blueprint pass of `step_dress` both
call it, because they respawn these actors. The table is `COMPOUND_OVERRIDES` in `pripyat_map_build.py`.

| Outlier | Actor (class) | Fix |
|---|---|---|
| Two tan platforms | `Prypyat_Defense_AA_Core/Ramp`, `Prypyat_Defense_Pit_Core/Ramp` (static meshes of the kit) | The kit's own `M_Prypyat_RidgeEarth` (used only by these four meshes): colour constant 0.72 0.57 0.38 -> 0.31 0.27 0.21 |
| White workshop hangar roof | `PRP_D_Workshop` (`BP_Industrial_Dest_Hangar`) | `StaticMesh` slot 0 -> `MI_Pripyat_Hangar_Weathered` |
| Pink ruin | `PRP_D_CommandPost` (`BP_Military_Dest_Bunker05`) | `StaticMesh` slot 0 and `GeometryCollection` slot 0 -> `MI_Pripyat_CommandPost_Concrete`. The pink came from the master's lerp toward red (`SandStrength`), and `Metallic_Multiplier` 0.6 made the bare concrete look silver |
| White generator fuel tank (became the brightest object in the low view) | `PRP_D_Gen_Fuel` (`BP_Industrial_Dest_OilTank2`) | `StaticMesh` slot 0 -> `MI_Pripyat_OilTank_Weathered` |
| Orange camo nets on the dumps (became the most saturated object) | `Prypyat_Runway_And_Buildings` (kit runway mesh) | Slot 6 -> `MI_Pripyat_DumpCamoNet` |

New assets are in `/Game/RTS_Survival/Blueprints/Environment/Prypyat/Map/`. The shared masters have no tint, so
`M_Pripyat_Env_Weathered` (copy of `MM_RTS_Env`), `M_Pripyat_BunkerConcrete_Weathered` (copy of
`M_RusBunker_Concrete`, WallGate VAT included) and `M_Pripyat_CamoTent_Weathered` (copy of
`M_Russian_CamoPattern_Tent`) each add `Desaturation(WeatherDesaturation) x WeatherTint` in front of the base colour.
The four `MI_Pripyat_*` instances above are children of these copies. Each carries over the source instance's textures,
scalars and switches. The shared Blueprints, meshes and materials are unchanged. None of these actors carries the
shell-hole component.

Destruction (PIE, `Saved/Pripyat/c_af_wreck.py`): the hangar and the fuel tank swap to their wreck meshes and keep
the override. For the tank this is correct, because its wreck mesh uses the same atlas. The hangar wreck mesh
normally uses `MI_RTS_Destroyed_Default`; with the override it shows the weathered sheet texture. Its UVs come from
the hangar (slot `M_hangar_001`) and it reads as collapsed sheet metal. The bunker's collapse chunks use the new
instance and no longer turn red.

Shots (same spots, `Saved/Pripyat/make_af_shots.py` + `c_af_shots.py`, three top views at the gameplay pitch and two
low obliques at -24 degrees, plus a generator view added in round 3): `Saved/Pripyat/shots/airfield_fix/before`,
`after` (after a reload of the saved map), iterations `after1..after5`, wrecks in `wreck`.

Still bright in these views, and left alone: the chrome aircraft fuselages and pipe stacks of the boneyard (metal
resource, meant to read as salvage), and the pale concrete fence panels (`M_ScorchAtlas`, map-wide).

## Phytophage trees and the sunken pod (added after the build)

- **Trees:** `Prypyat/Phytophage/Trees/`: `SM_PhytoTree_Creeping`, `_Shrouded`, `_Cocooned` (variants of
  `SM_RTS_DeadTree_1`, same pivot, 5.8k / 8.5k / 8.2k triangles, 3 LODs), crushable `BP_PhytoTree_*`, master
  `M_PhytoTree` / `MI_PhytoTree` (wet gloss, panning normal, Fresnel hairs, slow sway on the growth only by vertex
  colour R, vein pulse by G). Import: `assets/phyto_trees_import.py`. Blender source:
  `BlenderTanks/CustomMapItems/Phytophage/Trees`. Not placed on the map.
- **Sunken pod:** `Prypyat/SunkenPod/`: `SM_Pripyat_SunkenDemeterPod` (14.7k / 6.7k triangles, 4 hulls, pivot at the
  waterline), `M_SDP_Hull` / `MI_SDP_Hull`; growth slot uses `MI_PhytoTree`, canisters `MI_DP_DemeterLabel`. Import:
  `assets/sunken_pod_import.py` (headless). Blender source: `BlenderTanks/CustomMapItems/PripyatSunkenPod`.
- **Placing without a rebuild:** the map has hand edits since the build, so do not rerun `step_dress` for this.
  `pripyat_place_sunken_pod.py` adds only `PRP_I_SunkenDemeterPod` and `PRP_Note_sunken_pod` to the open level and does
  not save; it can be run in any editor that has the map open (`py "<path>"` in the Output Log).
- Checked in a play session in an unsaved test level: `Saved/Pripyat/shots/sunken_pod/ingame_b`.

## Play-session test

`pripyat_play_test.py` (results `Saved/Pripyat/play_test.txt`, `play_test_2.txt`), with a Panzer III of the default
start force:

| Test | Result |
|---|---|
| Driven through: two Polesie spruces, road sign, stork pole, sweep well, airfield crates, airfield drums | Removed (crushed) |
| Driven through a telegraph pole | Mesh hidden, collision off |
| 12 hits of 600 damage: both log houses, kit sandbags, memorial | Removed |
| 12 hits of 600 damage: kit revetment | Mesh hidden, collision off |
| 12 hits of 600 damage: concrete fence panel, Russian bunker (existing project Blueprints) | Nine seconds later still present, tilted and sunk 0.6 m and 2.4 m; not confirmed as removed |

Shell holes and full destruction (`pripyat_hole_test.py`, results `Saved/Pripyat/hole_test.txt`, `hole_test_2.txt`):

| Test | Result |
|---|---|
| The shell-hole component's own test hit (6 hits on the bounding-box faces) | Hole count rises on both log houses, kit sandbags, both concrete fence types, the Russian bunker and the hay shed. 0 on the memorial, a kit revetment, a stork pole and the fence gate: the hit points probably missed the mesh, so unproven either way |
| Damage until health is gone, look 12 s later | Kit classes (log houses, memorial, revetment, sandbags) are removed. Existing fence, house, building and command-bunker classes reach zero health and leave their wreck mesh. The Russian bunker's health does not react to plain damage (it stayed at 300); it needs a check under real weapons |

The second log was taken with a 1 s look instead of 12 s, so its "still present" column says nothing about removal.

Placed actors with the shell-hole component: 268 (61 + 57 dead trees and willows, 42 + 8 + 3 concrete fence pieces
and gates, 26 wire fence panels, 12 log houses, the kit props, sandbags and revetments, signs, poles, wells, bunkers,
the memorial). `BP_Dest_PostApo_House`, `BP_Airport_Dest_Building01`, `BP_Military_Dest_Bunker05` and
`BP_Industrial_Dest_Hangar` are destructible but carry no hole component.

## Open items

- **Rim teeth:** the top rim of a 60 degree bank is a sawtooth on the 1 m grid where it runs diagonally (khutor
  north-east shore, AA-N). Brush hides most of it. The bank cannot be blurred without opening the bog to units.
- **Not tested:** real weapon fire (holes were made with the component's test hit), the peat smoke cost, frame rate with all instances, and how
  columns play on the lanes and the ring.
- **Ground surface bake:** the nine `Prip*` layer names are not in `URTSGroundSurfaceSettings`, so track smoke treats
  the whole map as dirt (same as Berlin).
- **No landscape grass output** in the Pripyat master; low growth is instanced.
- **Kit looks left alone:** the airfield kit's pale bay walls. The two platforms and the white workshop hangar roof are
  fixed by the compound material pass below.
- `SM_PrypyatCausewayWreckBlock` is imported but not placed: it would seal a lane. It is for the mission's wreck-seal
  markers (`PRP_Note_wb_*`).
- The play session spawns the template's default start units at the start rise; unit placement and all mission
  actors are for the mission scripter (`PRP_Note_*` marks every site).
