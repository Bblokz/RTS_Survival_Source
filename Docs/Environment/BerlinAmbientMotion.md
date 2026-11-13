# Ger_Berlin ambient motion: street litter, hanging wires and curtains, crows (2026-11-13)

Three cheap systems that make the ruined city move. Nothing here ticks per object, nothing has collision, nothing
touches navigation. Built and tested on a slice of the real map in a throwaway editor; **not yet applied to
`Ger_Berlin`** (one build step, see "Putting it on the map").

| System | What draws it | Count on the map | Where it lives |
|---|---|---|---|
| Paper, ash, dust wisps along the lanes | ONE Niagara system of three stateless emitters, one component per lane stretch | 85 stretches | `Berlin/Ambient/Litter` |
| Curtains, shutters, broken cables, lamps, sagging wires | static meshes moved in the vertex shader by the project's `MF_BF_Flutter` / `MF_BF_Sway`, as components of the ruin they hang on | 736 pieces on 383 ruins | `Berlin/Ambient/Dressing` |
| Crows | perched: one instanced mesh per group on its ruin (head moves in the vertex shader); circling: the existing `BP_BirdFlock_Crow` | 34 groups, 151 birds; 3 circling groups | `Berlin/Ambient/Dressing`, `DesertFlocks` |

Content root: `/Game/RTS_Survival/Blueprints/Environment/Berlin/Ambient/`. Scripts: `Source/Tools/BerlinMap/ambient/`.
Blender sources: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/BerlinAmbient/`.

## Putting it on the map

```bash
python Source/Tools/BerlinMap/ambient/ambient_gen.py      # only after the dress file or the tuning changed
```

then, in the throwaway editor with `Ger_Berlin` open, after `step_ruins()`:

```python
bmb.step_ambient()        # clears every AMB_ actor and ambient component, places all of it again
bmb.save_level()
```

- **No navmesh rebuild.** No collision, `can_ever_affect_navigation` off on everything.
- `step_ambient()` needs the `RUIN_` actors, and it refuses to run when `Ger_Berlin_ambient.json` was generated for a
  different ruin list than the dress file holds (count and hash in `stamp`), or when a `RUIN_` label does not match its
  piece. After `berlin_dress_gen.py` rolled new ruins: run `ambient_gen.py`, then `step_ruins()`, then `step_ambient()`.
- `step_ruins()` destroys the ruin actors and with them their dressing and crows: rerun `step_ambient()` after it.
- The step is in `berlin_map_build.py` as a thin hook; the work is `ambient/ambient_place.py` (`run()`).

## Rebuilding the assets

`sh Source/Tools/BerlinMap/ambient/ambient_build.sh` (add `probe` when the ruin kit meshes changed). In order:

| Step | Script | Makes |
|---|---|---|
| textures | `ambient_textures_gen.py` (Pillow) | `T_BerlinAmbient_Litter` (4x4 atlas: 12 tumbling paper frames, 4 ash flakes), `T_BerlinAmbient_Cloth` |
| meshes | `BerlinAmbient/generate_berlin_ambient.py` (headless Blender) | 9 FBX, `SM_BerlinAmb.blend`, `Previews/sheet.png`, `build_info.json` |
| attachment points | `BerlinAmbient/kit_attach_probe.py` (headless Blender) | `kit_attach_table.json`: windows, perches, spans per kit piece |
| Unreal dressing | `ambient_dressing_build.py` (commandlet) | meshes, `M_BerlinAmbient_Dressing` + 3 instances, `M_BerlinAmbient_Crow` |
| Unreal litter | `ambient_litter_build.py` (commandlet) | litter materials, `NET_BerlinAmbient`, `NS_BerlinStreetLitter` |
| placement | `ambient_gen.py` | `Saved/Berlin/terrain/Ger_Berlin_ambient.json` |

All of them can be rerun. The Unreal steps run in `UnrealEditor-Cmd -run=pythonscript -nullrhi` (`run_headless.sh`);
the open editor must not have the Ambient assets loaded when they save.

Existing assets are only **used**, never edited: `MF_BF_Flutter`, `MF_BF_Sway` (called from the new master),
`T_AmbientDust_Puffs` (sampled by the wisp material), `BP_BirdFlock_Crow` (placed, its ellipse and flight settings set
per actor), the engine's `FountainLightweight` (emitter template). Two project tools do what Python cannot:
`RTS.ImpactVfx.BuildFromRecipe` (builds the Niagara system from a recipe) and
`NomadicDeployEditorLibrary.connect_world_position_offset` (Python's `MaterialProperty` has no World Position Offset).

## 1. Street litter

`NS_BerlinStreetLitter` holds three **stateless ("lightweight") emitters**. A stateless emitter keeps no particle
buffer and runs no simulation: a particle's position is a closed-form function of its age, evaluated on the GPU when it
is drawn. So there is no collision, no per-particle light and no per-particle CPU work by construction.

| Emitter | Look | Alive per stretch (steady / in a gust) | Material |
|---|---|---|---|
| Paper | 30-46 cm sheets tumbling through 12 atlas frames, 1.7-4.3 m/s, curl noise | 22 / 45 | `M_BerlinAmbient_Litter`: masked, lit |
| Ash | 7-14 cm flakes, grey to charred, rising a little | 35 / 65 | same material, atlas frames 12-15 |
| Wisp | 2.6-4.3 m soft dust sprites hugging the ground | 3 / 5 | `M_BerlinAmbient_Wisp`: lit translucent (volumetric non-directional), depth fade |

- **Why one component per lane stretch, and not one emitter fed with lane data:** every custom Niagara emitter in this
  project is authored by a C++ console command, and C++ was off limits for this pass. The stateless emitter needs none:
  the existing recipe builder makes it. A stretch is a box 30 m long and as wide as the lane; the component is rotated
  so local +X runs down the lane. All stretches share the one system asset, its three compiled emitters and two materials.
- **Wind:** `ambient_gen.py` reads `WindYawDegrees` of the RTS Ambient Effects settings (35 unless `DefaultGame.ini`
  overrides it) and points every stretch down its lane in the direction the wind pushes; lanes across the wind keep
  40 % of their stretches, lanes along it 92 %. The wind is baked into the placement: after changing the setting,
  run `ambient_gen.py` and `step_ambient()` again.
- **Gusts:** each emitter spawns a steady trickle plus two or three bursts close together once per loop; every stretch
  draws its own loop length (11-17 s), so neighbouring stretches gust at different moments.
- **Culling:** the effect type `NET_BerlinAmbient` puts a component to sleep when it is further than 95 m from the
  camera, and each emitter has the same distance in its own emitter state. Measured: with the camera on the test slice
  3 of 6 components were awake (the two placed 300 m away were asleep); with the camera moved 300 m away, the only one
  awake was the far one it stood next to.
- **Lighting:** the paper is lit, with a fixed world-up normal. A sprite's own normal faces the camera, which made every
  sheet black when seen against the sun (first test run); with the up normal a sheet is lit like the ground it blows
  over and is dark in a shadowed street.
- Masked sprites go through the base pass: no blending and no sorting. Only the wisps are translucent (3-5 per stretch,
  peak opacity 0.34 times the puff texture).

Tuning (`ambient_litter_build.py`, then rebuild): rates, bursts, sizes, speeds, colours per layer in `write_recipe()`;
`STRETCH_LENGTH`, `CULL_DISTANCE`. Density on the map (`ambient_gen.py`): `STRETCH_SPACING`, `LITTER_KEEP_ALONG`,
`LITTER_KEEP_ACROSS`, `LITTER_MIN_GAP`.

## 2. Wires and curtains

Nine small meshes, generated by a Blender script, pivot at the attachment:

| Mesh | Tris | Motion | Hangs |
|---|---|---|---|
| `SM_BerlinAmb_Curtain_A` / `_B` | 96 / 72 | flutter: 0 at the rail, full at the torn hem; blows out of the window | from the lintel of a framed window, on the outer face |
| `SM_BerlinAmb_Shutter` | 120 | rigid pendulum about its top hinge | at a top corner of a window |
| `SM_BerlinAmb_CableDangle_A` / `_B` | 125 / 113 | flutter, growing down the cable, away from the wall | over a wall top, down a free wall face (3 m / 1.9 m) |
| `SM_BerlinAmb_Lamp` | 68 | bracket fixed, cord and shade swing | on a wall face 55 cm below the top |
| `SM_BerlinAmb_WireSpan_A` / `_B` | 202 / 244 | flutter: pinned at both ends, most in the middle, torn droppers | between two wall tops of one ruin, 4.5-11.5 m apart |

- **Weights** are in UV channels 2 (sway, lift) and 3 (direction), exactly as `MF_BF_Flutter` reads them
  (`Docs/Rendering/BreakthroughMotion.md`). The rigid pieces carry weight 1 on the swinging part in channel 2, which the
  master feeds to `MF_BF_Sway`'s Weight input.
- **Material:** `M_BerlinAmbient_Dressing`, static switches `UseTexture`, `MotionFlutter`, `MotionSway`. Instances:
  `MI_BerlinAmb_Cloth` (cloth texture, masked, sway 80 cm, lift 12 cm), `MI_BerlinAmb_Wire` (flat dark colour, opaque,
  sway 24 cm), `MI_BerlinAmb_Rigid` (opaque, 9 degrees). The motion follows the kit's clock, so it stops with the
  trees' wind setting (`MPC_RTSWPO.WPOIntensity` 0) and takes `MotionTimeScale` for filming.
- **Where things can hang** comes from the kit meshes themselves: `kit_attach_probe.py` raycasts every kit FBX on a
  20 cm grid and finds framed openings with a sill (windows; doors and shop fronts are left out), high flat points on
  wall tops with a free drop beside them (perches) and pairs of perches with clear air between them (spans).
  `Previews/attach_<piece>.png` shows them. The table is in each piece's Unreal local space (Blender Y mirrored; checked
  against the Unreal bounds of `B2_Facade2St_B` and `E1_CornerTower`, and by eye in the test: curtains sit in their
  openings, crows on the wall tops).
- **Placement:** `ambient_gen.py` rolls per window, perch and span with a random stream per ruin; at most 3 pieces per
  ruin (6 on the E1-E3 heroes). The transforms are relative to the ruin.
- **Why components of the ruin and not one instanced mesh for the map:** the ruins are destructible. A component of
  the ruin actor is destroyed with it and sinks with it if the death moves the actor; a map-wide instanced mesh would
  need a runtime writer to remove instances. The price is one primitive per piece instead of one per mesh type; the
  renderer still merges equal meshes into one draw (dynamic instancing), see "Cost".
- **Per component:** Movable, no collision, no overlap events, no shadow (a moving vertex shader redraws its virtual
  shadow map page every frame), no tick, `World Position Offset Disable Distance` 90 m, max draw distance 130 m, tag
  `BerlinAmbient`. Mesh bounds are padded by 95 cm so a blown-out curtain does not pop at the screen edge.
- **When the wall dies:** the kit's current death hides the ruin's own mesh at once and destroys the actor 4.5 s later.
  The dressing stays visible for those 4.5 s, then goes with the actor (tested: a crow component was visible 1 s after
  the kill and gone, with the actor, 8 s after). To make it vanish with the burst, the death graph in
  `BP_BerlinRuin_Base` should hide the children too: `SetVisibility(false, propagate)` on the root, or hide every
  component with tag `BerlinAmbient`. That graph belongs to the ruin death work; nothing here does it.

Tuning: chances and caps at the top of `ambient_gen.py`; amplitudes in `ambient_dressing_build.py` (`run()`); shapes in
`generate_berlin_ambient.py`.

## 3. Crows

- **Perched:** `SM_BerlinAmb_CrowPerched` (154 triangles, a generated hooded crow: grey body, black head, wings and
  tail; two flat colours picked by UV0.x, no texture). One `InstancedStaticMeshComponent` per group, named `AMB_Crows`,
  on the ruin, tags `BerlinAmbient` and `BerlinCrowPerch`. `M_BerlinAmbient_Crow` turns and dips the head in the vertex
  shader (holds, looks left or right, pecks), each bird on its own phase.
- **Circling:** three `BP_BirdFlock_Crow` actors (the existing `ABirdFlock`, unchanged) over the densest fields of tall
  ruins, 6-8 birds each, ellipse semi-axes 50-70 m and 28-36 m, scale 0.42-0.5 (about 1 m wingspan). Each is one actor
  tick that updates 6-8 instance transforms, and one draw.
  - They fly 15 m above the ground (measured in the test: 12.4-17.7 m with the per-bird spread and drift). The game
    camera is about 28 m up, and a bird near the lens fills the screen, so the band must stay well under it; that is
    lower than the tallest ruins. `ABirdFlock` does not steer, so `ambient_gen.py` picks each ellipse to pass over many
    low ruins and no ruin (or landmark) taller than 11 m: 0 of 40 path samples near a tall ruin on all three.
- **Take-off when units or gunfire come near is NOT built.** `ABirdFlock` cannot perch or scatter, and the trigger needs
  to know where units and shots are, which is game code. What is in place is the asset side and its contract.

### The crow manager (design, needs C++; nothing was written)

Per-instance custom data of every `BerlinCrowPerch` component (`CROW_INSTANCE_DATA` in `ambient_dressing_build.py`):

| Slot | Meaning | Written by |
|---|---|---|
| 0 | phase, seconds | the placement |
| 1 | rate multiplier | the placement |
| 2 | airborne: 0 perched, 1 folds the perched bird onto its pivot (invisible) | the manager |

`UCrowPerchSubsystem` (tickable world subsystem, about 250 lines):
1. **Begin play:** collect the components with tag `BerlinCrowPerch`; a group is one component (weak pointer, centre,
   radius). No groups: never tickable. Create one instanced mesh of `SM_Crow_VAT` / `MI_Crow_VAT` (the existing flying
   crow; instance data 0 phase, 1 rate, as `BirdFlockInstanceData`) with as many instances as may fly at once (48),
   parked at zero scale.
2. **Scares:** `ReportNoise(Location, Radius)` called next to the existing hooks: the small-arms tracer hook that feeds
   `UBulletFlyByAudioSubsystem::OnTracerFired`, shell impacts, and `URTSCombatPingSubsystem::ReportUnitHit`. Plus, every
   0.5 s, one sphere overlap for pawns (12 m) per group, only for groups within 100 m of the camera focus.
3. **Per group:** Perched -> TakeOff (birds leave 0-0.6 s apart: slot 2 = 1, a flying instance starts at the perch) ->
   Circle (8-14 m circle, 6-10 m above the group, the maths of `ABirdFlock::AdvanceBird`) -> after 12-20 s without a
   scare, Land (glide to the perch, slot 2 = 0, flying instance parked).
4. **Cost:** ticks only while a group is airborne; perched groups cost nothing. A dead ruin invalidates the weak
   pointer: its birds fly off and shrink away.
5. **Settings:** scare radius, calm time and the airborne cap in `URTSAmbientFxSettings`; `rts.AmbientFx` 0 hides the
   perch components.

## Cost

Measured in the throwaway editor, PIE on `RTS_ToCopySmall`, 1600 x 900, with a slice of the real map: 21 ruins within
75 m of ruin 135 (B3), their 32 dressing pieces, 3 crow groups (16 birds), 4 litter stretches plus 2 far ones, 1 flock.
The test map's own squads and emplacements were alive in the same view, and other editors were running on the machine.

**Shader instructions** (`MaterialEditingLibrary.get_statistics`, vertex / pixel):

| Material | VS | PS | Texture samples (VS / PS) |
|---|---|---|---|
| `MI_BerlinAmb_Cloth` | 417 | 304 | 3 / 1 |
| `MI_BerlinAmb_Wire` | 414 | 295 | 0 / 3 |
| `MI_BerlinAmb_Rigid` | 398 | 295 | 0 / 3 |
| `M_BerlinAmbient_Crow` | 441 | 300 | 3 / 0 |
| `M_BerlinAmbient_Litter` | 289 | 282 | 0 / 4 |
| `M_BerlinAmbient_Wisp` | 345 | 248 | 0 / 5 |
| for comparison: `MI_BerlinRuin_Wall` (no motion) | 312 | 292 | 0 / 3 |
| for comparison: `MI_BF_Trim_Flutter` (the depot's cloth) | 417 | 319 | 3 / 3 |
| for comparison: `MI_Crow_VAT` (the flying crow) | 515 | 297 | 3 / 1 |

The motion costs about 90-130 vertex instructions on these small meshes; the pixel shaders are at the level of the
plain ruin wall. (The sample counts are the engine's totals for the shader, engine-internal lookups included.)

**Counts** (hardware independent):

| | On the map | Worst case in one game-camera view (65 x 36 m) |
|---|---|---|
| Dressing components | 736 | 18 (from the placement file) |
| Perched crow groups / birds | 34 / 151 | 1 / 5 |
| Litter components | 85 | at most 13 awake at once (within 95 m of the camera), 2-3 in view |
| Particles | about 75 per awake stretch, 115 in a gust | 150-350 in view; at most about 1000 alive |
| Ticking actors | 3 (`ABirdFlock`) | |

**Draw calls:**
- **Equal dressing components merge into one draw call.** Measured with 40 copies of `SM_BerlinAmb_Curtain_A`, set up
  like the placed components and alone in the view: 40 mesh draw commands in **1** draw call (instancing state bucket)
  in each of the depth, base and velocity passes; nothing in the shadow passes (`cast_shadow` reads back False in the
  game). So the dressing costs at most one draw call per mesh type per pass: at most 8 x 3 = 24 however many pieces
  are in view.
- In the busiest game-camera view of the slice the renderer drew 12 dressing components, 1 crow component and 1 litter
  component (`was_recently_rendered`); the placement file's worst view on the map holds 18 dressing pieces.
- A crow group is one draw per pass (3); a circling group one draw per pass.
- A litter stretch in view is 3 sprite draws (paper and ash also in the depth pass), all sprites of an emitter in one
  instanced draw. Not counted in the test.
- The velocity pass is the third pass: a mesh with World Position Offset is drawn once more there.

**Frame time**, everything shown against everything hidden, off/on pairs of 240 frames each (median frame time of
the whole editor frame, uncapped). Fifteen pair differences over four runs, sorted:
-1.12, -0.50, -0.27, -0.11, -0.05, +0.08, +0.36, **+0.41**, +0.50, +0.55, +0.72, +0.81, +0.87, +1.12, +1.67 ms (one more
pair, the first of a run, was a warm-up outlier of -11 ms and is left out). Median +0.4 ms on a 10-14 ms frame, with a
spread four times the value. Read it as "under a millisecond for 3 awake litter stretches, 32 dressing pieces, 16 perched
crows and a flock, in PIE inside an editor on a busy machine", not as a cost per part:
- the single-part states (only litter, only dressing, only crows) did not separate from the all-off frames;
- in the "off" state the litter components were hidden and told to deactivate, but most still reported active, so part
  of their tick is in both halves of each pair.
A proper number needs the map itself in a DebugGame or Shipping build with nothing else running.

## What the test showed

`sh Source/Tools/BerlinMap/ambient/ambient_run.sh <logname>` (about 6 minutes; never saves the map), then
`python Source/Tools/BerlinMap/ambient/ambient_test_report.py Saved/Berlin/ambient/test/<logname>.log` (result.json's numbers and the copies check).
Pictures: `Saved/Berlin/ambient/test/` (`<view>_a/b/c.png` are one second apart; `_0` only settles the camera).

- The placement code is the code of the build step: the test spawns the slice's ruins as `RUIN_<piece>_<index>` and
  calls `ambient_place.place_litter / place_dressing / place_crows / place_flocks`, clears, and places again (same
  counts, no leftover or unnamed components).
- **Litter:** from the game camera a stretch shows 10-25 pale sheets that have all moved a second later, with one or two
  soft dust patches; ash flakes are 2-3 pixels and only read from closer.
- **Curtains:** sit in their window openings with the torn hem; the hem swings visibly between frames.
- **Wires, cables, lamp, shutter:** hang where the probe put them (wall tops, window corner, wall face).
- **Crows:** stand on the wall tops in twos and threes; heads turn and dip between frames. The circling group flies
  under the camera and reads as small dark birds with ground shadows (`flock_rts_a/b.png`).
- **Airborne slot:** writing 1 into custom data slot 2 of two instances made exactly those two birds vanish, and 0
  brought them back. (`airborne_0` / `airborne_1`: a screenshot lands a frame after it is asked for, so the picture
  named `0_all_perched` shows the folded state and `1_two_folded` all three birds.)
- **The ruins still work with the extra components:** the hole component finds its mesh by name (`StaticMesh`), and
  the dressed ruin killed in the test collapsed and was destroyed as before.
- **Death:** see "When the wall dies". (In the death pictures a wall still stands: dress ruins 7 and 8 are two
  `B7_WallCore` pieces 1 m apart; 7 died with its crows, 8 and its wire remain. The doubled wall core is in the dress
  file, not something this work made.)

## Weaker than asked, or not verified

- **Crows do not take off.** Design above; the perched birds and the circling groups are independent of each other.
- **From the game camera's steep angle a curtain is a small light patch** on a facade seen almost edge-on, and a wire
  is a two-pixel line. They read at the closer zoom levels. Litter and crows read at every zoom.
- **The litter is not one system instance for the map** but 85 components of one system, of which the cull distance
  keeps at most 13 awake. A single GPU emitter reading lane samples from a texture or array would need a C++ authoring
  command like the dust devils' (`DustDevilNiagaraAuthoring.cpp`); the placement file already has what it would read.
- **Litter ignores walls:** particles drift straight down the lane heading of their stretch and pass through anything
  standing in the carriageway. Lanes bend, so near a bend some sheets leave the cobbles.
- **No graphics setting reaches these systems.** Not the Ambient Effects setting (`rts.AmbientFx`), and not the
  engine's effects quality either: the effect type only culls by distance. Hook, one place in
  `ULandscapeAmbientFxSubsystem` (C++): at level 0 hide and deactivate every component tagged `BerlinAmbient`.
- **The litter ignores the trees' wind setting.** With `MPC_RTSWPO.WPOIntensity` at 0 the curtains and wires stop;
  the paper keeps blowing.
- **Particle counts are computed** from the rates, bursts and lifetimes, not counted in the game.
- **A second crow beside the first is not checked against the wall top:** where the top is narrower or lower beside the
  perch, a bird may overhang or stand a little in the air.
- **Scripts that ask a `RUIN_` actor for its mesh:** after `step_ambient()` such an actor has several static mesh
  components. `get_component_by_class(StaticMeshComponent)` may return a curtain; take the component named `StaticMesh`.
- **Saving and loading** the level with these instance components was only covered by PIE (which duplicates the
  level); the map itself was never saved with them.
- **Not verified:** that the motion stops beyond the WPO disable distance (the property is set and reads back 9000 in
  the game; no picture from 90 m), a cooked build (the Ambient folder is referenced by the map, so it cooks with it),
  the look on Berlin's own dark ground and lighting (the test map is orange sand under an orange sky), 736 pieces at
  once on the real map (the slice had 32).
- **A wire span belongs to one ruin.** Wires between two different ruins were left out: they would hang in the air
  when the other ruin dies.
- **Velocity pass:** a mesh with World Position Offset is drawn once more in the velocity pass, so the dressing in view
  costs three passes (depth, base, velocity), not two.
- The first idea for the perched crow, the project's skeletal crow frozen in its "look" clip, was dropped: applied in
  Blender the wing cards come out mangled (`Previews/crow_views_skeletal.png`). The generated bird has no texture.
