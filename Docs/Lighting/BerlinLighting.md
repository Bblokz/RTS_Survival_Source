# Ger_Berlin lighting

## Sun light function (2026-11-13)

The sun of `Ger_Berlin` uses `/Game/RTS_Survival/Materials/Sky/M_LF_BerlinSmokePall`: soft cloud and smoke shadows that
drift slowly east along the lanes, plus a fixed term per district. The map has no `ExponentialHeightFog` actor (the
owner's rule: no engine default fog on this map); `step_lighting` and `step_sun_light_function` both remove one if a
rebuild from the template brings it back.

Assets (same construction as the First Mission and Trains sunbreaks):

- `M_LF_BerlinSmokePall`: light function, absolute world position -> field -> transmission. Rebuilt from scratch by
  the script on every run.
- `MF_BerlinSmokePallField`: one Custom node. Output R = direct sunlight multiplier, G = opening, B = fine variation,
  so a later effect (shafts, ground haze) can sample the same field at a world position.
- `MPC_BerlinSmokePall`: the controls. The parameter names are the First Mission ones (the collection is a copy); their
  Berlin meaning:

| Parameter | Value | Berlin meaning |
| --- | --- | --- |
| SunbreakWorldSize | 5200 cm | Size of the largest cloud shapes; two finer octaves at about 1/2 and 1/4. |
| CloudCoverPower | 0.5 | Cover threshold on the 0..1 noise: lower puts more of the city in shade. |
| SunbreakContrast | 1.35 | Edge hardness. 1 is soft clouds; raise it for harder smoke shadows. |
| ShadeTransmission / SunTransmission | 0.38 / 1.0 | Sunlight in full shade and in the open. Keep the floor: units must stay readable. |
| RayDetailDrift x CloudDriftScale | 140 x 1.0 | Wind speed in cm/s. Zero stops the drift. |
| RayDetailWorldSize / Strength / Floor | 3500 / 0.25 / 0.88 | Faint variation inside the open stretches. |
| LegacyWeight | 1.0 | Strength of the district term; 0 lights the whole city alike. |

The noise is procedural (three octaves of smooth value noise in the Custom node), not the project's
`T_RTS_PackedNoise`: that texture is veined, so thresholded it gave marbling and read small it gave hard flecks. The
owner asked for the noise to look like softer clouds (2026-11-13).

District term (fixed in the world, in the Custom code; +X north, +Y east): a standing pall over the crater (full
within 45 m, gone at 150 m) and over the east (y from 140 m to 340 m), which dims even the openings by up to 14 % (it barely
shifts the cover threshold: a strong shift left the east in uniform shade); the dead park in the west and the north along the Spree are clearer. The wind blows from
the west, a little to the north (0.22, 1.0).

Sun settings set by `step_sun_light_function`: light function fade distance 150000 cm (the pattern must hold at RTS
camera heights), disabled brightness 1.0. Colour and intensity are unchanged (`step_lighting`).

### Rebuild

```
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/lighting/berlin_light_function_build.py -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
```

then `berlin_map_build.step_sun_light_function()` on the map. `BERLIN_LF_DEBUG=1` in the environment builds an
exaggerated version (black shade, a gale) for looking at the pattern; rebuild without it afterwards. An editor that
has the three assets loaded locks their files: run the script inside that editor (`import
berlin_light_function_build as b; b.run()`), not from a second process.

### Checked

Throwaway editor on Ger_Berlin, shots in `Saved/Berlin/lighting/` (`real_h14000`, `real_h30000`, `real_mitte`,
`real_east`; `it_*` are the exaggerated runs; `fm_*` the First Mission function on the same sun for comparison).
The pattern is soft and cloud-shaped from 140 m and 300 m up, and two frames 88 s apart differ (mean 25 of 255 grey
levels at 320 x 180), so it drifts. Not checked: a game run (fog of war, game camera), GPU cost, and whether the
light function is applied beyond the shadow distance (from 920 m up no pattern showed with any light function).

### Traps met

- Repointing the copied First Mission light function at the Berlin field dropped its pin links: the sun went black.
  The script now rebuilds the material and wires it explicitly.
- `post_edit_change` does not exist on these Python objects; use the setters.

### Second pass (same day)

The owner did not see clouds at the player start: the first values (90 m shapes, shade 0.5, 0.6 m/s, and a pall that
shifted the cover threshold) left the east in near-uniform shade, and one cloud was larger than a game view. The table
above holds the second values: about 50 m shapes so a game-height view always holds a cloud edge, shade 0.38,
1.4 m/s. Shots: `v2_start`, `v2_start_h9000`, `v2_mitte` (frame difference at the start 20 of 255 grey levels).

## Atmosphere per district (2026-11-13)

Smouldering ruins across the whole map, low smoke and dust in Mitte, a faint green glow and heat shimmer at the crater,
fires with flickering light and smoke columns in the east. **No engine fog of any kind**: no height fog, no volumetric
fog, no fog volumes, no sky atmosphere. Everything is sprites, cards and five small lights. Built and looked at in a
throwaway editor (PIE on Ger_Berlin, never saved); **not yet applied to `Ger_Berlin`** (one build step, see "Putting it
on the map").

Assets: `/Game/RTS_Survival/Blueprints/Environment/Berlin/Atmosphere/`. Scripts: `Source/Tools/BerlinMap/atmosphere/`.

### What exists

| Layer | What draws it | On the map | Knob |
|---|---|---|---|
| **Smouldering ruins** (the core, map-wide) | `NS_BerlinAtmo_Smoulder_Wisp / _Shell / _Hero`: one stateless Niagara emitter each, soft sprites rising, pushed downwind harder the higher they get, widening and thinning. Material `M_BerlinAtmo_Smoulder` takes the district colour from the mask at the particle (grey, darker in the east, a little green at the crater) | 70 systems: 23 wisps (small heaps, stubs, collapse heaps), 40 shells, 7 heroes. East 36, Mitte 18, crater 9, west 7 | `RuinSmoke`, `RuinSmokeColor` |
| Mitte smoke banks and dust | `NS_BerlinAtmo_MitteSmoke`: two stateless emitters (Bank, Dust), a 50 m box per ruin block, drifting with the wind | 12 systems | `MitteSmoke`, `MitteColor` |
| Haze sheets (**optional**, own knob) | `M_BerlinAtmo_Sheet` on the engine plane, 80 m tiles at 3.0 m and 6.5 m: procedural drifting smoke x the district mask, depth fade, dimmed by the sun's own cloud field (`MF_BerlinSmokePallField` is only called). Carries Mitte's low haze, the crater's green mist and the east's low smoke | 67 cards on 41 tiles | `HazeSheets` (0 drops them all) and the district knobs |
| Crater glow | green mist in the sheet (G of the mask), `NS_BerlinAtmo_CraterVapour`, one dim unshadowed green point light with a slow pulse (`M_LF_BerlinAtmo_Crater`) | 1 system, 1 light | `CraterGlow`, `CraterColor` |
| Crater heat shimmer | seven ground-parallel 26 m cards a little above the rim (`ATM_Shimmer_N`, `M_BerlinAtmo_ShimmerCard`): they draw nothing and only bend what is behind them (refraction by pixel normal offset, strength 0.12), in patches that drift with the wind | 7 cards | `CraterShimmer` |
| Fires of the east | `NS_BerlinAtmo_Fire_Ruin / _Wreck`: a copy of the project's `N_RTS_Fire05_DitherAA` without its smoke, plus a stateless smoke column that leans with the wind and fades near the camera (`M_BerlinAtmo_EastColumn`) | 5: three hero ruins, the Alexanderplatz bus, one wrecked Pz II | `EastSmoke`, `EastSmokeColor` |
| Fire light | one point light per fire: unshadowed, radius 11.5 m, flicker in the light function (`M_LF_BerlinAtmo_Flame`, instances `MI_LF_BerlinAtmo_Flame_0..5` = six phases). No actor ticks, no particle lights | 5 | `EastFire` |

- **The knobs** are the scalars and colours of `MPC_BerlinAtmosphere` (one place). 1 = as built, 0 = off; `EastFire` and
  `CraterGlow` reach the lights through their light functions and saturate at 2. `CraterGlow` ships at 0.6. A rebuild
  keeps the values the collection holds (`atmosphere_build.py reset` puts the defaults back).
- **District mask** `T_BerlinAtmo_Districts` (256 x 512, written by `atmosphere_gen.py`): R Mitte (ruin density, cut on
  the carriageways), G crater, B east (a plume downwind of every fire, nothing within 55 m of the player start).
- **Wind**: direction (0.22, 1.0) as the sun's light function. The sheet reads the speed from `MPC_BerlinSmokePall`; the
  sprite velocities are baked (rebuild after changing the wind). The street litter of the ambient layer uses the
  project's `WindYawDegrees` (35), which is not this direction.
- **Culling**: effect type `NET_BerlinAtmosphere` pauses a system further than 150 m from the camera.
  **Pause, not deactivate**: with the litter's setting (deactivate and resume) every stateless emitter stayed dead
  after the camera came back; the smoke columns were gone on the second visit. `NET_BerlinAmbient` has that setting.
- **Fires and their hosts**: fire and light stand on the host but are not attached to it (an attached fire would sink
  and tilt with a dying hero ruin, and the engine would still not destroy it). Both carry the actor tags
  `BerlinAtmosphere` and `AtmHost_<host label>`: a death graph that wants the fire gone destroys the actors with its
  own tag. Until then a fire keeps burning over the remnant. Smoulder systems are free actors too: when a ruin dies
  its smoke keeps rising from the remnant or heap.
- **Placement rules** (`atmosphere_gen.py`, measured distances are in the json): fires only on hero ruins (inside the
  shell, spot from `fire_host_table.json`) or wrecks, east of y 20000, 90 m from the start, off the lanes, 20 m from
  passes, 6 m from cover pieces, 50 m apart. Smoulder: 17 % of ruins anywhere, more in the east and round the crater,
  about a third of that in the north and at the park edge, none on lanes, within 70 m of the start or 18 m of a fire.
  Collapse heaps are cover: the smoke rises from the heap's middle, the prone sockets are at its foot.

### Putting it on the map

```bash
python Source/Tools/BerlinMap/atmosphere/atmosphere_gen.py     # only after the dress file or the tuning changed
sh Source/Tools/BerlinMap/atmosphere/run_headless.sh "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/atmosphere/atmosphere_build.py" build   # only then, too
```

then, with `Ger_Berlin` open, after `step_ruins()` and `step_dress()`:

```python
bmb.step_atmosphere()      # clears every ATM_ actor, places all of it again; removes a height fog actor if one is back
bmb.save_level()
```

No navmesh rebuild (no collision, nothing affects navigation). `step_ruins()` and `step_dress()` destroy the fire
hosts: rerun `step_atmosphere()` after them. Rebuilding: materials can be rebuilt inside an editor that holds them
(`atmosphere_place.clear()` first, then `atmosphere_build.run(systems=False)`); the Niagara systems must be rebuilt
headless with no editor holding them (rebuilt in place, their components never activated again).

### Cost (PIE in a throwaway editor, 1600 x 900, game camera 28 m up, other editors running on the machine)

| | Mitte view | Smoulder view | Crater | East (bus fire) |
|---|---|---|---|---|
| Translucency pass, layer hidden / shown (ProfileGPU, median of 2-4 dumps) | 0.17 / 0.25 ms | 0.10 / 0.17 ms | 0.12 / 0.26 ms | 0.10 / 0.26 ms |
| Draws in that pass, hidden / shown | 7 / 25 | 5 / 26 | 7 / 24 | 8 / 19 |
| Distortion pass | - | - | 0.11 ms, 4 draws | - |
| Sheet cards / sprite systems / lights drawn | 7 / 7 / 0 | 8 / 10 / 1 | 7 / 3 / 1 | 4 / 4 / 2 |

- A quieter run before the smoulder layer existed gave the same order: 0.04 / 0.31 ms in Mitte, 0.03 / 0.28 ms at the
  crater. The light function atlas pass is 0.01 ms for all six functions.
- **Frame time** on / off (median of 250-300 frames per state, uncapped): Mitte pairs -1.4 to +0.5 ms, crater +0.3 to
  +0.4 ms on a 9-12 ms frame in the quiet run; in the last run the machine was busy and the pairs scatter by +-10 ms.
  Read it as "below the noise of this setup", not as a number.
- **Particles** (computed from rate x lifetime): wisp 4, shell 10, hero 19 sprites alive per system; Mitte system 15;
  crater 29; a fire's column 46 (ruin) or 30 (wreck) plus the copied fire's own about 40. Worst case from the placement
  file: 4 smoulder systems in one game view, 28 awake within 150 m (about 260 sprites).
- **Pixel shader instructions** (`get_statistics` in the editor): sheet 388, sprite materials 115-117, shimmer 82 (the sprite version; the card was not read), light
  functions 129-139 (the old flicker). `M_BerlinAtmo_Smoulder` (a sprite material plus one mask sample) was not read.

### What the shots show (`Saved/Berlin/atmosphere/shots/`)

`final_<view>_on / _on_b / _off` are the layer shown, shown a few seconds later, hidden; `final_sheet_a/b.png` are
contact sheets. Mean frame difference at 320 x 180 (of 255): Mitte 9.8 between the two frames and 19.4 on/off, crater
5.5 and 22.0, east 4.1 and 8.9, smoulder view 3.4 and 6.0.

- **Mitte**: a pale drifting haze that the wall tops stand out of, plus brown-grey puffs; `final_mitte_nosheets` is the
  same without the sheets (only the puffs remain: the sheets carry most of Mitte's look).
- **Crater**: at `CraterGlow` 1.0 the bowl filled with green (`final_crater_on`); at the shipped 0.6 it is a faint
  green cast with a few brighter vapour puffs, fading out past the rim (`last_crater_default`), and with
  `HazeSheets` 0 only a trace of it is left (`last_crater_sheets0`).
- **East**: dark columns leaning east from the bus and the ruins, a warm pool of light at each, the column is back on
  the second visit (`final_east_return`). All five fires were seen burning (`last_fire0/3/4`, `last_sheet_a.png`).
  **The flicker** swings the light's own part of the brightness by about a quarter: measured on three fires, ten frames
  about 1 s apart each (`fx4_<ruin|wreck|tower>_0..9`, `_off` = `EastFire` 0): -27..+27 %, -26..+22 % and -31..+17 %
  of its mean (`fx4_sheet.png`). The screenshot tool does not deliver frames closer than about 0.9 s.
- **Smoulder**: grey smoke from heaps and shells; from the game camera a wisp is small and a shell reads clearly only
  against shadow. `final_hero_on` shows a hero.
- **Game run** (`game_*.png`, `game_sheet1/2.png`; PIE on `RTS_ToCopySmall`, the real camera pawn, fog of war on, one
  test instance laying Mitte's mask over the map): unexplored fog of war stays black over every layer (the fog post
  process runs after translucency); riflemen stay readable under full Mitte haze, which does pale the picture; the
  selection rings show through. Not judged there: the shimmer (the units were fighting in the pictures), explored but
  unseen ground.

### Open points

- **Why the first flicker did not show** (fixed): the function was applied and its time ran, but it summed three slow
  sines (about +-8 %) and an 11 Hz jitter of +-10 % that the eye and the frame blending average away. Now two smooth
  random walks at about 3 and 7 changes a second, +-28 % and +-12 %. Not the light function atlas: with
  `r.LightFunctionAtlas` 0 and 1 a test light flickered the same.
- **Why the first shimmer did not show** (fixed): it was a refraction material on stateless Niagara sprites. The same
  graph on a static mesh card bends the picture at every strength tried (0.085 to 3), on the sprites it bent nothing
  at any; why the sprite renderer drops it was not found. Also needed: `refraction_coverage_mode` = coverage ignored,
  because the card's opacity is 0. So the shimmer is cards now. Shots: `fx4_shimmer_pair.png` (on / off, crater),
  frame difference 0.6 s apart 8.7 with it and 2.9 without (`fix_shimmer_diff.png`). In the game run on
  `RTS_ToCopySmall` (`game_shimmer_sheet.png`) the fog of war edge is the same with and without it and at three times
  the strength nothing smears round the riflemen; on that map's plain sand the shimmer itself hardly shows, and the
  selection rings could not be compared frame for frame (the units were fighting).
- **Collision of the cards** (fixed in `quiet()`): the first map save brought every haze sheet back as `BlockAll`
  (the engine plane's default: the component still used its mesh's default collision), so 67 invisible cards at 3.0 and
  6.5 m blocked traces. The step now sets the profile `NoCollision` and switches the default collision off; reads back
  `NoCollision`, default collision False, and a trace from above reaches the ground again.
- Replaced and no longer built or placed (delete once no editor holds them): `M_BerlinAtmo_Shimmer`,
  `M_LF_BerlinAtmo_Fire`, `MI_LF_BerlinAtmo_Fire_0..5`, `NS_BerlinAtmo_Crater`.
- Knobs exercised in the game: `EastFire` 0 (light off), `EastSmoke` 0 (columns gone), `RuinSmoke` 0, `HazeSheets` 0.
  `HazeSheets` 0 makes the 67 cards invisible but they are still drawn; to drop them for good, skip `place_sheets()`
  in `atmosphere_place.run()`. `EastFire` dims the lights only: the copied flames have no knob.
- 18 of the 70 smoulder spots are collapse heaps, which are cover; the smoulder placement does not keep clear of
  cover heaps the way the fire placement does.
- The sheet's 388 instructions were read before the `MaskRect` parameter and the `HazeSheets` multiply were added.
- Smoulder strength was raised once after the first look; if it is still thin, `RuinSmoke` goes above 1.
- Unlit smoke does not darken under the sun's cloud shadows (only the sheets do) and ignores the fire lights.
- No graphics setting reaches these systems; `rts.AmbientFx` does not hide them.
- Not verified: a cooked build, saving the level with these actors, the fires after their host has died.
- The ambient layer's dust wisps draw as flat grey squares (`M_BerlinAmbient_Wisp` reads the puff texture's alpha; the
  density is in R). Seen in the crater shots with this layer hidden; not touched here.
