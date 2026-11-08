# Showcase screenshot map

`/Game/RTS_Survival/Maps/Showcase/RTS_Showcase` is a map for marketing stills: hero line-ups (8–12 vehicles,
World-of-Tanks-key-art style) and large faction overviews. The map and everything under `Maps/Showcase/` is
**gitignored**; it is rebuilt by the scripts in `Source/Tools/Showcase/`.

## Stages

Each stage sits 20 km from the others with its own generated landscape, lighting set and camera presets. A stage's
actors live in its own streaming sublevel `RTS_Showcase_<Stage>` (all loaded in the editor, none at game start);
the cameras stay in the persistent level. Loading every stage at once needs more than 30 GB of RAM, so the capture
streams in one stage at a time.

| Stage | Content | Ground / backdrop |
|---|---|---|
| `HeroSteppe` | 10 Soviet heavies in an arrowhead | farmland grass, farm buildings, orchard, green hills |
| `HeroAlpine` | 10 Breakthrough heavies on a snowy shelf | snow and rock, dark spruces, snowy mountains |
| `HeroMesa` | 11 Strike Division vehicles | desert, rocks, cactus bushes, desert mountains |
| `SovietArmy` | every regular Soviet tank, truck, squad, team weapon, aircraft | desert, Soviet base buildings and bunkers |
| `SovietIndustrial` | Soviet column on a concrete yard | `BP_Ground_A/B/C` tiles with `M_RTSGround_WithSand`, refineries |
| `ForgeArmy` | the Director of Forge (`_Soviet` / `_Forge`) vehicles | scorched desert, `_Soviet` buildings and walls |
| `StrikeDivision` | all Strike Division vehicles and variants, squads, team weapons, pits, deployed nomadic buildings, aircraft | dry steppe |
| `Breakthrough` | the same for Breakthrough with its BF buildings and bunkers | green meadow, spruce forest |

Every stage has at least one `Vertical1x2` (portrait 1:2) and one `Horizontal16x9` camera; overviews add
aerial and 2:1 panorama cameras.

## Taking stills

Cameras are `ARTSShowcaseCamera` actors (`Source/RTS_Survival/Showcase/`). The filmback follows the camera's
`ShotFormat`, so piloting a camera in the editor shows the saved framing. `LongEdgePixels` (default 7680) sets the
output size: 7680x4320, 3840x7680 or 7680x3840.

- **In PIE:** start the game on the map, then in the console:
  `RTS.Showcase.Shot <all|stage|shot|wildcard> [scale]`, e.g. `RTS.Showcase.Shot HeroAlpine` or
  `RTS.Showcase.Shot *_V 0.5`. `RTS.Showcase.List` lists the cameras. Stills go to `Saved/Showcase/<timestamp>/`.
- **Headless batch:** `python Source/Tools/Showcase/sc_shoot.py [filter] [--scale 0.25]` runs a windowed `-game`
  on the DebugGame editor binary, writes the stills and `contact_<Stage>.jpg` sheets, and quits. Measured on the
  RTX 4070: all 47 cameras at `--scale 0.25` take about 5.5 minutes; one 7680x4320 still about 3 minutes
  (48 converging frames at full size), so a full-resolution set of every camera is a 2-3 hour batch. Render a
  stage or a single shot (`sc_shoot.py HeroAlpine__Hero_H`) when you only need that one.
- **Select a camera in the editor** and press `Activate Stage Lighting` (details panel) to see that stage's sun,
  sky and grading; all stage lights are on at once otherwise.

During a batch the subsystem waits for the game to start; per stage it streams the stage sublevel in (and the
previous one out), spawns marker units, warms up for 6 s, puts back any unit that drove, turned or flew off,
then freezes every unit (custom time dilation
0), hides health bars, takes units out of the fog of war (and un-hides enemy-owned ones), hides exhaust/smoke
Niagara puffs and force-finishes nomadic buildings. Per shot it switches the stage lighting (actor tags
`ShowcaseLight_<Stage>`), puts the player view on the camera, boosts texture streaming for the still's
resolution, settles 2.5 s and renders 48 frames at the output size before saving the last one (temporal AA and
Lumen converge). Quality cvars (cinematic scalability, far LODs, sharper VSM) are set for the batch and restored.

Command-line switches: `-ShowcaseShots=<filter>` (required for a batch), `-ShowcaseScale=`, `-ShowcaseFrames=`,
`-ShowcaseSettle=`, `-ShowcaseWarmup=`, `-ShowcaseDir=`, `-ShowcaseNoFreeze`, `-ShowcaseNoStreaming`
(load all mips; heavy). Run with `-unattended` so the difficulty picker does not wait for a balance pick.
The tool compiles out of shipping (`RTS_WITH_MAP_TESTS`).

## Review history

Three adversarial review rounds (critic agent with the owner's three reference images) drove the layout:
`Saved/Showcase/round1_review.md` .. `round3_review.md` hold their summaries; preview sets are in
`Saved/Showcase/round0b` .. `round3`. Open points from the last review: the parade grids could be broken into
clusters, the industrial yard has no joints, rails or markings, the Brushify snow reads mottled up close,
Breakthrough's mesh-only buildings read grey from afar, and the turrets are not aimed at the camera.

## Rebuilding the map

The scripts run inside a **DebugGame** editor (the shared Development editor needs a rebuild before it knows
`ARTSShowcaseCamera`; never save this map from an editor that does not, or the cameras are stripped):

```python
import sys; sys.path.append("D:/UE5Projects/RTS_Survival/Source/Tools/Showcase")
import uecp; uecp.allow_bulk_loads()          # only when run through the UECP bridge
import sc_build; sc_build.run(["HeroAlpine"])  # or sc_build.run() for every stage
```

- `sc_common.py`: terrain generator (value-noise field, open front, ridges behind, flats for building rows),
  landscape creation through `URTSShortsLandscapeLibrary::CreatePaintedLandscape`, placement helpers, stage
  lighting, orbit cameras. Actors are labelled `SC_<Stage>_...`; a stage rebuild removes exactly its own actors.
- `sc_stages.py`: rosters (by asset name), formations, dressing and cameras per stage.
- `sc_build.py`: entry point; also creates the gitignored airborne copies of the PE-8, Pe-2 and Il-2
  (`Maps/Showcase/Units/Showcase_*_Airborne`, `bM_StartedAsLanded` is defaults-only).
- First build on a new machine: copy `Maps/ToCopy/RTS_ToCopySmall.umap` to `Maps/Showcase/RTS_Showcase.umap` on
  disk before opening the editor (duplicating the map in the editor and loading it crashed).

## Gotchas

- A unit whose child turret actor has a timeline (Puma Overcharged Beam / Radixite Sniper, Panzer III M
  Overcharged, Panzer III J Radixite Cannon, ...) cannot be saved in a level ("Illegal reference to private
  object"). The build replaces such units with `TargetPoint`s tagged `ShowcaseSpawn=<class>`; the subsystem spawns
  them at play.
- Brushify distance mountains are ~70 m meshes: they are sized and placed from their bounds 2.5–4 km out. Placed
  closer they swallow the stage.
- Soviet units belong to player 2: the fog hides them until the still prep shows them again.
- Nomadic trucks get `bStartAsConvertedToBuilding`; the prep force-finishes the deploy animation.
- Airborne aircraft fly off to land at a deployed airbase, and some vehicles turn during the warm-up; the prep
  restores every pawn to its placed transform before freezing.
- `BP_Ground_A/B/C` are 10-30 m tall industrial plots, not flat ground. The concrete yard uses
  `BP_Airport_Ground1` slabs (24 m, `M_RTSGround_WithSand`).
- Some building Blueprints carry a component parked at the world origin, so `get_actor_bounds` spans kilometres;
  `sc_common.local_bounds` measures only mesh components near the pivot.
- On the 12 GB RTX 4070 close the DebugGame editor before a batch: with it open the capture ran out of video
  memory. Full 7680 px stills need the most headroom.
