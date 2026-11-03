# 11 — Level design: SM_Kessel_01

Map: `/Game/RTS_Survival/SurvivalMode/Maps/SM_Kessel_01`, "The Kessel": a German position encircled on a plateau. The
player always starts in the middle, there is no start selection, and the waves come from every side. Terrain, paint and
dressing come from the scripts in `LevelTools/` (`kessel_*.py`), so a change is an edit and a rerun, not hand work.

Words in the owner's brief: "red oxide" is radixite, "racks" is wrecks, "virtual animated texture animals" are the VAT farm
animals, "Stuck Marder" is `Ger_MarderStugMission`.

## Size

`Ger_MarderStugMission` is a 51 000 cm square (landscape and fog of war). The Kessel has 20 % more area: a playable square of
**55 800 cm**, ±27 900 round (0, 0) (51 000 × √1.2 = 55 868). The first build read the 20 % per side (61 200 cm, 1.44 × the
area); the owner's earlier "too large" on SM_Desert_01 decided for area. `PLAYABLE_HALF` in `kessel_terrain_gen.py` switches it.

The landscape is larger than the play area: 12 × 12 components of 63 quads, 100 cm quads, 75 600 cm square. The 9 900 cm
past the play area is a hill rim with spruce and fir, so the camera never sees the end of the world.

## Landscape

Material `MI_RTS_Landscape_DryCastle_Inst`, the same instance as the Marder map. Its sun light function (cloud shadows),
post process, sky light, fog and the two RVT volumes are copied from that map. RVT volumes must cover the landscape exactly.

What the layers look like (one wedge per layer, 2026-11-03): GrassCastle green, Desert cream, GraySand yellow sand, RTSSand
salmon, RockDesert grey, MountainTop cream. The large dark blotches over all layers are the master's own colour variation,
not paint.

| Tier | Z | What |
|---|---|---|
| Plateau | 1 120 | Flat to r 8 500, `BuildAreaRadius` 8 000: the base. Generator check: z range 0. |
| Shelves | 590-750 | Six exactly level discs of r 2 800-3 300 on the terrace, at r ~15 000: expansion ground. |
| Terrace | ~640 | Rolling ground round the plateau, r 9 000-16 000, carries the lane that links the shelves. |
| Valley | ~330 | The outer ring, with two broken arms of a dry wash (r ~24 000) painted in sand. |
| Hills and ridges | up to +700 | Five hills and four ridges with a pass each, for sight lines and cover. |
| Rim | up to ~2 000 | Outside the play area only. |

Six gates (bearings 15, 75, 135, 195, 255, 315; +X is north) cut the plateau's bluff; between them the bluff is steep enough
that navigation refuses it (over 44° at every bearing, generator check 60°), so ground attacks funnel into the gates. It has
to be checked per bearing: where the terrace runs high the drop is smaller, and a 5.6 m wide bluff let infantry climb there.
Ramp slope along the gate centre: 9-11°.

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
  `RTS_NavFilter_Characters`: without a filter the blocker does not show.
- **Power lines:** two runs of `BP_Civilian_Dest_CrushablePole_Electric1` from the map edge to the plateau, ~34 m spans,
  linked by `ConnectToActor`; each pole's `DestructibleWire` builds the wires at BeginPlay.
- **Rocks:** Brushify boulders at the foot of the bluffs (on the terrace, so nothing overhangs) and on hills and ridges.
- **Trees:** 150 crushable `BP_Wpo*` firs, spruces and leaf trees in copses, 240 more in the rim.
- **Foliage:** 70 patches of the CastleStone colorable plants, painted through the UECP `paint_foliage` action.

## Rebuild

1. `python kessel_terrain_gen.py D:/UE5Projects/RTS_Survival/Saved/Kessel/terrain` (absolute path: the editor reads the files).
   It prints the checks: plateau and shelf z range, gate slopes, site counts. It writes a preview png.
2. In an editor on the map: `kmb.load_layout()`, then one bridge call each: `step_landscape` + `step_fit_level_actors`,
   `step_survival_actors` + `step_sites`, `step_power_lines` + `step_sandbags`, `step_rocks`, `step_trees`,
   `step_content_volume`, saving after each. `step_survival_actors` removes every actor labelled `Survival*`.
3. Foliage through `paint_foliage`, for `LAYOUT["plants"]`.
4. Rebuild navigation and save only once it is complete (25 MB at 55 800 cm). After a map load or a rebuild the editor regrows the navmesh as a
   disc from the centre for about two minutes, and `IsNavigationBeingBuilt` can read false at the very start. Check coverage
   on a grid for all four agents first. A save in that window drops tiles: the `.umap` came out at 10 MB and no wave could
   spawn. Complete it is 27 MB. Starting PIE in that window copies the partial navmesh, so squads stop at the edge of the disc.

`step_generated_audit(path)` measures every generated actor's visible LOD0 vertices against the height file. Seat by that
and by grazing views, never by actor bounds. Foliage needs its own check: `paint_foliage` traces onto trees, rocks and wires, so
instances land on top of them (19 of 1 070, one at 60 m). Compare each instance with `ground_z` and remove the strays with
`clear_foliage` at radius 25, or repaint before the dressing goes in.

## Verified (2026-11-03, final 55 800 cm build)

- A 22-minute run from the main menu through the SURVIVAL button, faction, variants and difficulty (DebugGame, content seed
  4242): no start picker, content generated, HQ landed, grace, waves from all four edges, a round survived, then the Defeated
  phase when the test army's HQ fell, and 12 more minutes running. No navmesh, path query, `Move to self` or wave launch error.
- Navmesh complete for all four agents; all 12 edge zones have complete paths to the centre; the bluff leak test
  (`RTS_NavFilter_Characters`, every 2.5 degrees of true bluff) finds no way up except round through a gate.
- Floating audit, static and skeletal meshes counted, against the height file. Every seed passes: no actor clear of the ground
  by more than 4 cm (worst 1.2 cm), none within reach of a pole, wall, rock or tree, none outside the play area.

  | Seed | Actors | Capture | Radixite | Metal | Wrecks | Animals | Verdict |
  |---|---|---|---|---|---|---|---|
  | 11 | 138 | 8 | 55 | 28 | 21 | 26 | pass |
  | 22 | 134 | 8 | 50 | 28 | 21 | 27 | pass |
  | 33 | 133 | 8 | 56 | 24 | 21 | 24 | pass |
  | 101 | 145 | 8 | 57 | 34 | 23 | 23 | pass |
  | 202 | 134 | 10 | 55 | 28 | 21 | 20 | pass |
  | 303 | 129 | 8 | 42 | 26 | 20 | 33 | pass |
  | 404 | 141 | 9 | 59 | 30 | 23 | 20 | pass |
  | 505 | 147 | 9 | 66 | 32 | 21 | 19 | pass |
  | 606 | 142 | 10 | 52 | 31 | 23 | 26 | pass |
  | 707 | 151 | 9 | 58 | 28 | 23 | 33 | pass |

- Foliage: every plant instance within 6 cm of the ground (strays from `paint_foliage` removed).
- In a live match: every pole built its wires. Four squads ordered into walls in the four compass quarters (bearings 32, 98, 212
  and 352) all seated at full strength: Faust 2 of 2, sniper 1 of 1, Hazmat 4 of 4, Jäger 4 of 4 (squad health 640 = 4 × 160).

## Known limits

- The track and dust systems have no baked maps for this map: track smoke uses its default surface, and no dust devils.
- No Soviet bases: they are generated on unpicked start locations, and this map has one start.
- The capture objects list `BP_SMD_CaptureLuchs` and `BP_SMD_CaptureMarderDual` from the desert map's Specials folder.
- The picker cards have no preview image.
