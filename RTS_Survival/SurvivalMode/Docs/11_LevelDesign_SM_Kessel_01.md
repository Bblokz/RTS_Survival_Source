# 11 — Level design: SM_Kessel_01

Map: `/Game/RTS_Survival/SurvivalMode/Maps/SM_Kessel_01`, "The Kessel": a German position encircled on a plateau. The
player always starts in the middle, there is no start selection, and the waves come from every side. Terrain, paint and
dressing come from the scripts in `LevelTools/` (`kessel_*.py`), so a change is an edit and a rerun, not hand work.

Words in the owner's brief: "red oxide" is radixite, "racks" is wrecks, "virtual animated texture animals" are the VAT farm
animals, "Stuck Marder" is `Ger_MarderStugMission`.

## Size

`Ger_MarderStugMission` is a 51 000 cm square (landscape and fog of war). The Kessel first had 20 % more area (55 800 cm a
side), then the owner asked for 10 % more on top: a playable square of **58 520 cm**, ±29 260 round (0, 0)
(55 800 × √1.1). `PLAYABLE_HALF` in `kessel_terrain_gen.py` sets it.

The landscape is larger than the play area: 12 × 12 components of 63 quads, 100 cm quads, 75 600 cm square. The 8 500 cm
past the play area is a hill rim with spruce and fir, so the camera never sees the end of the world.

## Landscape

Material `MI_RTS_Landscape_DryCastle_Kessel`, a child of `MI_RTS_Landscape_DryCastle_Inst` (the Marder map's instance), so
the same shader and settings. It switches on `LS_GroundGrass` (grass clumps; rebuild grass maps with `RTS.Radixite.RebuildGrass`
and save after any paint change) and widens the radixite blight edges. Two switches were tried and left off:
`LS_NoiseBreakup` re-thresholds the 1 m weightmap into stair-stepped soil splotches from the game camera, and
`HighQuality_WorldAligned` made parts of the landscape vanish. The sun light function (cloud shadows), post process, sky
light, fog and the two RVT volumes are copied from the Marder map. RVT volumes must cover the landscape exactly.

**Every component carries every layer.** A component whose weightmaps miss two layers renders through another layer
permutation that comes out washed pale: straight-edged pale rectangles on the 63-quad component grid. `paint()` gives each
component a 2/255 trace of any missing layer at one vertex (`TRACE_WEIGHT`).

What the layers look like (one wedge per layer, 2026-11-03): GrassCastle green, Desert cream, GraySand yellow sand, RTSSand
salmon, RockDesert grey, MountainTop cream. The large dark blotches over all layers are the master's own colour variation,
not paint.

| Tier | Z | What |
|---|---|---|
| Plateau | 1 120 | The base, 19 800 m² flat, `BuildAreaRadius` 7 200. Its rim is 74-120 m from the centre (below). |
| Knolls | 1 450-1 550 | Three flat tops on the rim, 3.3-4.3 m over the plateau, each with one ramp down into the base. |
| Shelves | 590-750 | Four exactly level discs of r 2 600-3 300 on the terrace, at r ~15 000-16 000: expansion ground. |
| POI pads | 540-640 | Five exactly level pads of r 3 300 beside the approach roads, one scene each (below). |
| Terrace | ~640 | Rolling ground round the plateau, r 9 000-16 000, carries the lane that links the shelves. |
| Valley | ~330 | The outer ring, with two broken arms of a dry wash (r ~24 000) painted in sand. |
| Hills and ridges | up to +700 | Five hills and four ridges with a pass each, for sight lines and cover. |
| Rim | up to ~2 000 | Outside the play area only. |

**Plateau outline.** Not a disc: `plateau_r(bearing)` is four overlapping waves plus `PLATEAU_LOBES`, a long prow to the
south-east (bearing 140, 120 m), a deep bay to the south-west (205, 74 m), a second prow at 318 and smaller bays and noses.
No two sides look alike; the first round read as a dull circle.

**Gates.** Five gates of different widths cut the bluff (bearing, half width at the rim; +X is north): 22 (1 150),
101 (1 450), 163 (1 050), 238 (1 350), 304 (1 200). The spacing is uneven on purpose: 79, 62, 75, 66 and 78 degrees. Between
them the bluff is steep enough that navigation refuses it (over 44° at every bearing, generator check 56°), so ground
attacks funnel into the gates. It has to be checked per bearing: where the terrace runs high the drop is smaller, and a
5.6 m wide bluff let infantry climb there. `BLUFF_WIDTH` 300 keeps it steep. Ramp slope along the gate centre: 9-10°.

**Knolls.** `KNOLLS`: bearings 130, 272 and 345, top radius 1 800-2 300, on the rim between gates. They are for dug-in tanks
with a view over the terrace. Each has one 11° ramp, 650 cm half width, from the plateau up; the other faces are 52° or
more, so from outside the only way up is through a gate and the ramp. German sandbag walls line the outer edge of each top.

**Points of interest.** One scene per approach, each on its own pad beside the road past the ramp foot, alternating sides
of the road (`POI_SCENES` in `kessel_map_build.py`, `GATES` in the generator):

| Gate | Scene |
|---|---|
| 22 | Farmstead: fenced yard with farmhouse, barn, hay shed, hay bales, STZ-1 tractor, an orchard with one tree cut down; three fenced crop fields (wheat, burned wheat with ash and dead trees, pumpkins) |
| 101 | Destroyed convoy: ZIS-5V, ZiL troop, fuel and ammo trucks nose to tail on an old rutted road (`old_road`), the fuel truck jackknifed, two cars shoved off, the T-34 escort at the head, craters |
| 163 | Crash site: a DC-3 at the end of an 80 m furrow (lowered trough with lips, dark earth decals), cargo strewn along it, burnt trees |
| 238 | Trench line: an overrun Soviet line from the Red Line kit (straights with sandbagged joints, MG nest, communication trench, dugout under a camo net, gun bay), wire, a loose hedgehog belt, a knocked-out Panzer III |
| 304 | Waterworks: gravel yard with water tower, two silos, pipe runs, barrels, S-65 tractor, an abandoned sedan |

The trench line is `TRENCH_LINE` / `TRENCH_EXTRAS` in the generator (layout `trench_line`), Red Line pieces seated on the
ground with their earth slot swapped to `M_Kessel_Soil` (world-aligned landscape desert texture). The earlier `BP_Trench_*`
ground blocks (`TRENCHES`, `dig_trenches`, `kessel_trench_profiles.json`) are kept in the code but unused: their flat tops
conform through the height RVT and read as blurred slabs from the game camera.

Crater decals are plain `DecalActor`s with the brown `MI_Kessel_Crater_0N` instances (`spawn_crater`): the crater Blueprint
picks its material again on every map load, so a tint set on it never reaches the game.

## Survival setup

- One `BP_SurvivalStartLocation` at (0, 0). `ASurvivalModeDirector::ShowStartLocationPicker` skips the picker when a map has
  one start location. With no other starts there are no Soviet bases on this map.
- 12 `BP_SurvivalEdgeSpawnZone`, three per edge, all active from minute 0 with weight 1, 2 600 cm inside the edge. All 12
  have a complete path to the centre.
- `DA_SurvivalMode_Kessel` is the default settings with `DA_SurvivalStartContent_Kessel`: no fields, wrecks, pits or close
  resources (the content graph owns those), 10 craters only.
- `RTS_BP_EnemyController` is a copy of the SM_Desert_01 instance: the decision tree, enemy global abilities and mission
  settings live on the placed actor, so the template's bare controller reports "No stochastic decision tree".
- The menu: `UW_SurvivalMapPicker` (code-built, `DA_SurvivalUiStyle`) opens from the SURVIVAL button and lists
  `USurvivalModeLauncher::GetSurvivalMapChoices`. Tests use `-RTSSurvivalMenuFlow -SurvivalMenuMapIndex=1`.

## Content graph

`Maps/PCG/PCG_Kessel_Content` runs once per match on `KesselContentVolume` (`ASurvivalMapContentVolume`) with a new seed, from
`ASurvivalModeDirector::GenerateStartLocationContent`. A map with such a volume skips the generic points of interest.
`FixedSeed` on the volume or `-SurvivalMapContentSeed=N` pins the seed. Rerun `kessel_pcg_build.build_graph()` after a pool edit.

The graph has one branch per kind of content: **Survival Site Picker → Spawn Actor** (spawn by the `ActorClass` attribute,
no merging). The picker reads the level's `ASurvivalMapSite` actors of a category, shuffles them with the seed, goes round
the compass taking one site per wedge, and outputs the actors to spawn from weighted pools. A site is a spot the generator
vetted for flatness, slope and distance from tracks, gates, poles, sandbags and other sites, so every seed stands on good
ground, and a designer can drag one in the editor. Site markers are in the outliner under `KesselSites/`.

| Branch | Sites per match | Ring (from centre) | Pool |
|---|---|---|---|
| Capture | 8-10 | all | gun pits, big guns and bunkers (at most one each), Brummbar, Luchs, Marder; they face outward |
| RadixiteBase, MetalBase | 2 each | plateau | single nodes the base can reach at once |
| RadixiteNear, MetalNear | 3-4 each | to 18 500 | a field: one centre actor and scattered members |
| RadixiteFar, MetalFar | 3-4 each | beyond 18 500 | the same, worth going out for |
| TankHulks | 12-16 | all | scavengeable German hulks; Tiger at most two |
| VehicleParts | 7-9 | all | part piles and car wrecks |
| Herds | 5-7 | all | 3-6 of one VAT animal per site: sheep, cows, goats, pigs or chickens |

Pools live at the top of `kessel_pcg_build.py`. Wrecks, capture objects and rails align to the ground. A pool entry with
`EvenGroundRadius` only takes ground that does not bend under it (the 6 m rail bundle bridged a dip by 19 cm otherwise).

Never save the level with generated actors in it: `cleanup_local(True)` on the volume's PCG component first. Generate with
`pcg.set_editor_property("seed", n); pcg.generate_local(True)`. The UECP `generate_pcg` and `get_foliage_summary` actions
crash the editor on this map (access violation in UECPLevelDesignExt).

## Dressing

All in `kessel_map_build.py`, labelled `KSL_`, outliner `KesselDressing/`.

- **Sandbags:** `Ger_SandBagWall_FieldConstruction` walls just back from the crest of the bluff, either side of each gate and
  midway along each bluff. They are the German field construction, owned by player 1, with a `Cargo` component: infantry
  garrison them. Three rules came out of the match tests. Keep them off the gate shoulders, where paths cross. Fit the nav
  blocker to the mesh (`SANDBAG_NAV_EXTENT`): the blueprint's is 400 cm deep round a 180 cm mesh, and an enemy taking cover
  against the bags then stands inside it with no start poly, so every path fails and the squad stays pinned. Keep a walkable strip
  behind them: enemy infantry take cover on the far side, and on the crest that is off the navmesh. Probes must use
  `RTS_NavFilter_Characters`: without a filter the blocker does not show. The fitted extent only counts once the modifier
  measures again (`remeasure_nav_modifiers`); a fresh rebuild without it baked the 400 cm blocker and two flamethrower
  troopers who took cover at a wall by gate 163 stayed pinned for the rest of the match.
- **Power lines:** two runs of `BP_Civilian_Dest_CrushablePole_Electric1` from the map edge to the plateau, ~34 m spans,
  linked by `ConnectToActor`; each pole's `DestructibleWire` builds the wires at BeginPlay.
- **Rocks:** Brushify boulders in five outcrops of different size at the foot of the bluffs (bearings 48, 77, 186, 215, 322;
  not a ring), and on the hills and ridges.
- **Trees:** 150 crushable `BP_Wpo*` firs, spruces and leaf trees in copses, 240 more in the rim. Their infantry blockers
  are fitted to the trunk (`TREE_NAV_EXTENT`, measured with `measure_tree_trunks`): the Blueprints' 100-125 cm boxes round
  18-30 cm trunks let a rifleman be pushed into one, off the navmesh, and he stayed pinned (`Move to self path finding
  failed`). A changed `FailsafeExtent` only counts once the modifier measures again, so `fit_tree_blocker` nudges the tree.
- **Foliage:** 70 patches of the CastleStone colorable plants, painted through the UECP `paint_foliage` action. Plants that
  land on a tree, rock or wire, or inside a point-of-interest footprint, are removed after painting.

## Detail pass (2026-11-04)

Six rounds of changes, each reviewed by an adversarial art critic from 22 RTS-pitch shots (`Saved/Kessel/shots/r1`-`r6`,
critiques in `Saved/Kessel/critiques`). Review shots switch the sun's cloud-shadow light function off and back on (it hides
the ground in stills; it stays in the game). Highlights:

- Terrain: knolls with wobbling outlines and varied faces, lobed pad edges, a ragged bluff lip with buttresses, a talus
  apron, hills with off-centre crests and one steep side, ridges with 2-4 humps and saddles, a ploughed crash furrow.
- Paint: grass follows hollows, wash banks, slope feet and shade; a 4-12 m octave with a contrast curve; trampled ground
  (base, knolls, tracks, yards) on MountainTop, the one soft layer the dry tufts never grow on; scree tongues under outcrops;
  bare rock on steep faces; worn lobes round the scenes and walls.
- Placement: curved roads with spurs and a hub loop in the base, rock outcrops as size-graded clusters, ~150 face rocks
  (mostly on the shaded bearings round 270), copses, hedgerows, field-edge bushes, lone tree groups and a ragged forest
  fringe; ground-stone foliage (`FT_Kessel_GS_*`) round outcrops and the base rim; knoll gun pits as open Vs with stores.
- Critic verdict: 2/10 in round 1, 5/10 after round 6 ("competent procedural with good set-pieces"). Left open: the shaded
  bluff faces still read as a dark band (the rock layer stretches; world-aligned projection broke the landscape), the ground
  is bright cream with few darks, faint radial stripes below the ramps (cause not found), grey props (pipes, crate stacks,
  drum rows), the radixite blight circles (C++ subsystem).

## Rebuild

1. `python kessel_terrain_gen.py D:/UE5Projects/RTS_Survival/Saved/Kessel/terrain` (absolute path: the editor reads the files).
   It prints the checks: plateau and shelf z range, gate slopes, site counts. It writes a preview png.
2. In an editor on the map: `kmb.load_layout()`, then one bridge call each: `step_landscape` + `step_fit_level_actors`,
   `step_survival_actors` + `step_sites`, `step_power_lines` + `step_sandbags`, `step_rocks`, `step_trees`,
   `step_content_volume`, `step_pois`, `step_fields`, `step_paths`, `step_pit_stores`, saving after each; then
   `step_boundaries`. `step_survival_actors` removes every actor
   labelled `Survival*`. `step_poi_overlap_report` lists point-of-interest footprints that overlap.
3. Foliage through `paint_foliage`, for `LAYOUT["plants"]`, then remove the strays (below).
4. Rebuild navigation and save only once it is complete (27 MB at 58 520 cm). After a map load or a rebuild the editor regrows the navmesh as a
   disc from the centre for about two minutes, and `IsNavigationBeingBuilt` can read false at the very start. Check coverage
   on a grid for all four agents first. A save in that window drops tiles: the `.umap` came out at 10 MB and no wave could
   spawn. Complete it is 27 MB. Starting PIE in that window copies the partial navmesh, so squads stop at the edge of the disc.

`step_generated_audit(path)` measures every generated actor's visible LOD0 vertices against the height file. Seat by that
and by grazing views, never by actor bounds. Foliage needs its own check: `paint_foliage` traces onto trees, rocks and wires, so
instances land on top of them (19 of 1 070, one at 60 m). Compare each instance with `ground_z` and remove the strays with
`clear_foliage` at radius 25, or repaint before the dressing goes in.

## Verified

After the detail pass (2026-11-04):

- Generator checks: plateau, knoll tops, shelves and pads z range 0; bluff weakest 52.0°, knoll faces 49.8°; every site rule
  placed its full count.
- Navmesh complete: Character 729 of 729, LightTank 729, Medium 726, Heavy 724.
- Light, medium and heavy tanks have complete paths from the base centre to all three knoll tops (77-118 m).
- PIE garrison: a Jäger squad ordered into a gate wall and one into a knoll pit wall both seated at full strength.
- Floating audit on seeds 3101, 3505, 4242, 7777: nothing clear of the ground by more than 4 cm, no clashes.
- Not rerun: the full menu-flow match after the detail pass.

Second layout (58 520 cm, irregular plateau, knolls, scenes), 2026-11-03:

- Generator checks: plateau, knoll tops, shelves and point-of-interest pads all z range 0; ground round both trench blocks
  exactly the pad level; bluff weakest 56.5°, knoll faces weakest 52.3°; every site rule placed its full count.
- Navmesh complete: Character 729 of 729 grid probes, tanks 724-726 (the rest stand on rocks, walls and knoll faces). All 12
  edge zones have complete paths to the centre.
- Climb test (`RTS_NavFilter_Characters`, every 2.5 degrees of true bluff, three probes per knoll): no way up a bluff; the two
  short paths at bearings 220 and 222.5 go up the shoulder of gate 238's own ramp. Every knoll top is reached only through a
  gate and up its ramp (paths 108-168 m long).
- Floating audit on six new seeds (3101, 3202, 3303, 3404, 3505, 3606): 125-153 actors each, none clear of the ground by more
  than 4 cm, none within reach of a pole, wall, rock or tree. Two seeds share 5-14 placements of ~140.
- Foliage: no plant above the ground by more than 6 cm or inside a scene footprint (195 removed after painting).
- 12-minute run from the main menu (DebugGame, content seed 4242): no start picker, HQ landed on the plateau, waves from
  the edges, rounds 1 and 2 cleared, the Defeated phase in round 3 at 10.9 minutes (the test army). Three `Move to self
  path finding failed`, all one rifleman at one small leaf tree on the east approach (23 770, -16 220). The same
  tree caught a rifleman in two of three runs with this seed (the first layout's runs had none). A PIE query from that spot
  finds full paths to every target he was sent to, so it is not reproduced from PIE; the cause is open. Two earlier runs found map faults, both fixed: a leftover probe trench 200 m over
  the centre where the HQ landed, and unfitted sandbag blockers that pinned two flamethrower troopers (16 errors).

First layout (55 800 cm, round plateau, six gates):

- A 22-minute run from the main menu through the SURVIVAL button, faction, variants and difficulty (DebugGame, content seed
  4242): no start picker, content generated, HQ landed, grace, waves from all four edges, a round survived, then the Defeated
  phase when the test army's HQ fell, and 12 more minutes running. No navmesh, path query, `Move to self` or wave launch error.
- Floating audit on ten seeds: none clear of the ground by more than 4 cm (worst 1.2 cm).
- In a live match: every pole built its wires. Four squads ordered into walls in the four compass quarters all seated at full
  strength.

## Known limits

- The track and dust systems have no baked maps for this map: track smoke uses its default surface, and no dust devils.
- No Soviet bases: they are generated on unpicked start locations, and this map has one start.
- The capture objects list `BP_SMD_CaptureLuchs` and `BP_SMD_CaptureMarderDual` from the desert map's Specials folder.
- The picker cards have no preview image.
