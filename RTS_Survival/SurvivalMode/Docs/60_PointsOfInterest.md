# 60 — Points of interest and wildlife

Owner: POI worker. Code: `Source/RTS_Survival/SurvivalMode/PointsOfInterest/`. Content:
`Content/RTS_Survival/SurvivalMode/PointsOfInterest/` (`PCG_SurvivalPointsOfInterest`, `DA_SurvivalPointsOfInterest`).
Review rounds: `Saved/AgentNotes/poi_review/round_N/`.

Small areas between the benches that give the player a reason to leave the base: a small battlefield, a ruined hangar
with capturable vehicles, and one fixed landmark per sector between two neighbouring starts (crash site, dead convoy,
pump station, tank graveyard, relay station). Plus the desert's animals: goat herds on the
oasis bank, ravens and crows over wrecks, cobalt rollers at the oasis edge, kites high over the basin.
Everything is generated per match from a seed, at runtime, through a PCG graph.

## Hook-up

`ASurvivalModeDirector::GenerateStartLocationContent` (Director/SurvivalModeDirector.cpp, after the Soviet base loop)
calls `USurvivalPointsOfInterestSubsystem::StartForMatch(this)`. That is the only line outside this folder (plus its
include). The subsystem is a `UWorldSubsystem` (game and PIE worlds), so the director needs no new component.

## The graph

`PCG_SurvivalPointsOfInterest` runs on an `ASurvivalPOIPCGVolume` (box over the whole landscape, `GenerateOnDemand`,
seed = match seed) the subsystem spawns:

```
Survival POI Sites ──► Survival POI Compose ──► Survival POI Spawn
        └────────────► Survival Wildlife ───────────┘
```

- **Sites** (time sliced: it returns unfinished until the height field is filled) samples the landscape on a 750 cm grid,
  builds the sectors and picks the sites. Output: one point per site, attributes `SiteIndex POIType Landmark Sector OwnerStart SubLayout Radius`.
- **Compose** lays out the composition of every site point it receives (a stock filter in between drops sites).
  Output: one point per object, `AssetKey Role SiteIndex Sink`.
- **Wildlife** places herds and flocks (the flocks over POIs read the site points). Output: `Entry Count SiteIndex FlightHeight SemiX SemiY GroupSeed`.
- **Spawn** sleeps while the subsystem loads every asset its points name (one async load) and spawns them in batches
  (`ObjectsPerSpawnStep`, 8 per frame), seats them and tags them `SurvivalPOI`.

Like the Soviet bases, all of it ticks with the world: a paused world (setup screens, or a bare `-SurvivalAutoStart`
run nobody unpauses) shows only the volume. The preview tools unpause, as the player's start button does.

## Sectors, sites and fairness

- Starts are read from the director at generation time and sorted counter-clockwise around their centroid; each
  neighbouring pair is a **sector**. Nothing about the starts is hard-coded, so the planned move of the starts inward
  and the crop of the map change the sectors automatically.
- Each sector gets its **landmark** (by the two starts' display names, `SectorLandmarks`; a landmark type not in
  `BuiltLandmarkTypes` is skipped and logged) and one generic site (`GenericSitesPerSector` = 1) owned by the sector's
  first start, on that start's side; its type (battlefield or hangar) is drawn by sector and seed. So every start owns one
  generic site and has a landmark on each side.
- A site must keep: 18 000 from every start centre (start content reaches 17 500); 9 000 from the play-area edge (the landscape
  bounds, clipped to `PlayableAreaMin/Max` when the map is cropped: SM_Desert_01 x -69 400..66 200, y -64 400..71 200);
  every start's `ReservedContentSpots` (ramps, gate aprons; read through reflection) + 3 000;
  6 000 from every edge spawn zone segment; Soviet base PCG volumes + 2 000; the exclusion zones of the settings
  (the oasis: basin outline centroid (16000, -3525), radius 6 150, + 3 000 rim); its radius plus 6 000 from other sites.
- Terrain (real heights): centre at most 450 above the sector's basin floor (25th percentile of its candidate heights);
  height range inside the site and slope per grid cell under the type's limits (hangar 120 cm / 4°, battlefield
  260 cm / 8°); no rise or drop over 400 cm on a ring 3 000 outside the site (no cliff, pad rim or ramp next to it);
  traces at the centre and four ring points must hit landscape only (no water, rock or building).
- Distance band to the owning start per type (19 000-27 000 for generic sites, up to 34 000 for landmarks). The best 30%
  of candidates are drawn from at random, so seeds move sites. When nothing fits, relax level 1 drops the sector-third window
  and widens the band x1.2, level 2 widens it x1.45 and accepts 1.4x the height range and slope (all logged as plan notes,
  with the rejection counts per reason).
- Every chosen site must be drivable for a HeavyTank from its owning start's deploy centre, and its chained drive length must lie in
  the type's `OwnerNavDistance` band (20 000-34 000 for generic sites), so every start's nearest POIs are a comparable drive away.
  Up to 8 candidates per request are path-tested.
- Map layout exclusions: `Tools/poi_sync_layout.py` copies the layout JSON's ramps and tracks (corridor +-1 100), their gate feet
  (`GateFootClearance`, 1 750 since the round 9 re-sync), the landform and world pads (no site overlaps a pad; the salt pan is
  allowed), the oasis basin (+1 750), the power-line pylons (1 500) and the routes (as lines, see below) into `Placement.Layout`,
  with the JSON's MD5. Rerun it (headless editor Python) whenever the terrain worker says the map changed; the subsystem logs a
  warning when the stored MD5 no longer matches the file.
- The seed chooses among all valid candidates, weighted by score (not best-first), so two seeds rarely share a site.
- **Order and fallback (2026-11-02):** every sector's landmark is planned first, then the generic sites. A generic site that
  falls back must not take a later landmark's ground. A generic site keeps its owner. It tries its home sector (relax 0-2),
  then the owner's other sector (the neighbouring sector on the owner's side, relax 0-2), then the last resort (relax 3) in
  both. A hangar that still finds nothing becomes a battlefield through the same chain, so its start keeps a site with
  capture targets. Landmarks and battlefields shrink at relax 2-3 (x0.8 / x0.65); hangars keep their size, because
  shrunk hangars lost their vehicles.
- **Anchor test with rewards:** landmarks and hangars are composed in full while planning (same seed, same draws, same
  occupancy as the match). A candidate stands only if its main object fits and the composition keeps at least half of the
  scavengeables and capture targets it tries. At the last resort, one is enough. A main object that does not fit tries the
  other models of its pool, smallest first. Battlefields are composed too (since 2026-11-02, round 5): one stands only if it
  keeps at least one capture target (gun pit) at every relax level, because it is its start's capture site when the hangar
  found no ground (capture spread median 1.56 over 200 seeds). Crash, pump, relay, graveyard and convoy sites also try the flipped layout
  (yaw +180); hangars do not, because their door faces the owner.
- **Routes:** `Layout.Routes` (the layout JSON's `routes`, copied by `poi_sync_layout.py`) replaces the old circle chain
  in `Obstacles`.
  - A site keeps its radius - 900 off a route's line.
  - Every standing object keeps `RouteHalfWidth` (1 250) + its footprint + 100 off the line, so 2 500 cm stay drivable.
    Decals and craters may lie on the route.
  - Standing objects also keep the gate foot clearance + their footprint + 100 from every gate foot.
  - The **dead convoy may lie along a route.** When a route passes inside a convoy candidate, the site moves beside it:
    the column runs parallel, 500 outside the free width. The ambush side faces the road, so craters pock the verge
    while pushed wrecks and cargo lie off the road. Only the route's two directions are tried.
- One site request runs per frame (the search is time sliced: height field slices of 4 ms, then candidates, then requests).

## Compositions (local +X = the story direction)

| Type | Story | What stands |
|---|---|---|
| Small battlefield | A Soviet armoured attack came in along +X from the frontier and died on a German gun line | 2-3 capturable gun pits facing the attack on a straight, bent or angled line, sandbags in the gaps, ammo behind the guns, 1-2 scavengeable German hulks behind the line, 2-4 Soviet hulls thinning out toward the line (the first 1-2 scavengeable `Env_Dest_*_NoFlame`, the rest burnt cover), scorch beside hulls, craters on the line and along the axis |
| Ruined hangar | Vehicles abandoned on the apron, nose out | `BP_Airport_Dest_Hangar`, 1-2 `BP_CaptureBrummbar` on the apron (outside the hangar's nav box) with tracks out onto the sand, 3-5 barrel and crate stacks along the walls, vehicle parts and a car wreck to scavenge, a few craters |
| Crash site (landmark) | A transport ploughed in along its flight line | DC-3 or Avenger fuselage (40 cm into the sand), impact crater and gouge decals back along the line, the torn-off wing section on the scar, debris thinning out, 3 scavenge piles, ammo crates further out |
| Dead convoy (landmark) | A supply column ambushed from one side | 5-6 vehicles nose to tail along +X with gaps, a knocked-out escort at head and tail, 1-2 pushed off the track away from the ambush side, Soviet trucks (`Env_Dest_ZIS5V`, `ZilTroops`, `Zil_*`, `SovietTanker`) and cars, tyre track decals along the road, craters on the ambush side |
| Tank graveyard (landmark) | An armoured attack that died here | 6-8 hulls (2-3 scavengeable), two thrown turrets downrange of Soviet hulls, craters everywhere |
| Pump station (landmark) | The dry well line's pump | water tower, pump engine and tanks, a pipe run along +X, fuel drums, vehicle parts and a car to scavenge |
| Relay station (landmark) | Where the power line turns | wire-fenced yard (gate on +X) round a steel mast, a pole line out, a fallen scavengeable mast outside the fence |

Sub layouts 0-2 per site (seeded): odd ones are mirrored, and each type varies line shape, counts and spacing.

Capturable vehicles: `BP_CaptureBrummbar` is the only one in the project (a `BP_CaptureActor` child: infantry capture it,
`Player1ClassOnCapture` spawns `Ger_Vehicle_Brummbar`). More need a copy per vehicle with its hull mesh and
`Player1ClassOnCapture` (casemate vehicles are the easy ones: Hetzer, Jagdpanther, SU-85/100, ISU-152).
`BP_Scav_Marder` / `BP_Scav_PzJager` (scavenge to get the tank) are the other option.

## Seating

Every object is seated on its **visible** meshes, never on actor bounds (root sprites, geometry collections). The
footprint (the visible LOD0 bounding box in actor space) is sampled 5 x 5 by traces from below the ground up against the
visible mesh components; each quadrant's lowest underside point against the ground under it is that quadrant's gap.
`AlignToGround` objects are tilted to the plane through the ground under the footprint corners (up to the asset's slope
limit), then rest on their two lowest quadrants (wheels, tracks, legs; a quadrant with nothing under it, such as an overhang or
the wire between two posts, may stand clear), sinking none more than 30 cm (plus a planned sink, e.g. the plane's 40 cm). The
manifest's `gap` is that second-lowest quadrant gap; `max_quadrant_gap` the highest.
`PivotOnGround` (dug-in pits, crater decals) snaps the pivot. Objects without collision fall back to the box bottom.
POI objects do not receive decals. The dirt and tyre-track decal materials rendered as dark slabs under `ADecalActor`, so their pools
are empty and the ground response is crater Blueprints only (the crash scar is a chain of small craters).

Spawned props that tick are throttled to `PropTickInterval` (0.5 s; Niagara components keep their own tick); the preview's tick
audit lists what still ticks per class.

## Wildlife

| Entry | Where | Size | Notes |
|---|---|---|---|
| Goats (`BP_Wanderer_Goat`) | 3 herds on the oasis bank, 3 000-4 600 outside the basin outline | 2-5 | wander radius 1 500, flee 900, alarm 1 500 (any unit that close makes the herd bolt; the subsystem checks every 0.5 s); the whole wander patch must be under 20 deg and dry |
| Ravens (`BP_BirdFlock_InkRaven`) | over battlefields and the graveyard (70%) | 4-7 | 1 300-1 900 high, ellipse 18-26 x 11-17 m |
| Crows (`BP_BirdFlock_Crow`) | over the crash site and the convoy | 5-8 | |
| Cobalt rollers | one small flock at the oasis edge | 3-5 | 700-1 000 high |
| Kites (`NS_AzureDesertKite_Flock`) | 2 sectors, over the basin | Niagara | 1 500-2 500 high (under the RTS camera) |

Wanderer and flock settings are protected Blueprint structs; the subsystem sets `MovementSettings.WanderRadius`,
`FleeSettings.FleeDistance`, `Ellipse.SemiAxisX/Y` and `FlightSettings.*` through reflection before finishing the spawn.

## Testing (`RTS_WITH_MAP_TESTS` builds only)

All inside a real match: `-SurvivalAutoStart [-SurvivalStartIndex=N]`, plus:

- `-POISeed=N` fixes the match seed (otherwise `SeedMode`: random per match, or `FixedSeed`).
- `-SurvivalPOIPreview`: at `-SurvivalPOICaptureAt` seconds (90) after generation start writes `census_<seed>.txt`,
  takes the held-camera views (a camera actor as the view target, main viewport screenshots; the minimap shot with UI),
  samples the animals at +0/+5/+10 s, measures perf (600 frames POIs on, off, on at the densest POI's rts5200 pose and the
  whole-map pose), writes `poi_<seed>.json`, `cameras_poi_<seed>.json`, `perf_<seed>.txt`, `render_settings.json`, exits.
- `-SurvivalPOIWatch`: the same without pictures and perf.
- `-SurvivalPOISweep=N`: plans seeds 1..N on the same height field without spawning, writes `sweep.csv`, exits.
- `-SurvivalPOIDisable`: the match without points of interest; with or without it the subsystem logs the worst frame in the 20 s after
  the director's call (`SURVIVAL_POI frame watch`), to attribute generation hitches.
- The preview presses the start button (`UPlayerStartGameControl::PlayerStartedGame`), takes close-ups at the animal's position at
  shot time, runs the flee test (a Tiger sent at the first goat herd, frames at arrival +0/+3/+8 s), 10-frame flock strips, and
  perf as 5 alternating on/off pairs per pose with a tick audit.
- `-SurvivalPOIDir=<dir>` output folder. Add `-ExecCmds="r.TextureStreaming 0"`.
- `Tools/poi_post.py <dir> <seed>`: map overlay with sectors, starts, oasis rim, POIs and animals; contact sheets; the
  exclusion check against the layout JSON (ramp corridors, gate feet, pads, basin outline) the runtime cannot see.

## Cooking

`DA_SurvivalPointsOfInterest` serializes every pool and wildlife entry as soft references and sits in the always-cooked
`SurvivalMode` folder, so the cooker follows them; `ASurvivalPOIProp` is C++. If a packaged run shows missing classes,
the folders to add to `DirectoriesToAlwaysCook` (through the coordinator) are `Blueprints/Environment/Animated`,
`Meshes/Wrecks/PlaneParts`, `Blueprints/Environment/ScavengeObjects/CrashedPlanes`, `Materials/Decals/DirtTracks` and the
vehicle folders of the `Env_Dest_*` wrecks.

## Not done / known limits

- The runtime sees the layout's ramps, gate feet, pads, basins, pylons and routes only through `Placement.Layout` (rerun
  `Tools/poi_sync_layout.py` after map changes); `Tools/poi_post.py` checks the spawned actors against the layout JSON.
- Sweep 2026-11-02 (200 seeds): 10 of 10 sites on 81% of seeds (else 9); convoy 100%, crash site 95%, relay 98.5%;
  straight-line scavenge spread median 1.53, capture spread median 1.62 (target 1.5 not met).
- Only one capturable vehicle type exists; hangar vehicles stand on the apron, because the hangar Blueprint's nav
  obstacle covers its whole footprint (a vehicle inside would be boxed in).
- POIs do not show on the minimap as their own marks yet.
- Footprint radii in the pools are estimates; the manifest prints each actor's measured LOD0 box.
