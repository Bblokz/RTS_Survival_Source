# Ore Mountains mineral-flora trees (2026-11-22)

Eight crushable dead trees and trunks for `Ger_2ndOreMountains` whose foliage carries the map's two mineral ground
layers: **IronOxide** (brick / hematite red) and **Ochre** (yellow-brown limonite). They sit beside the dead spruce kit
(`OreMountainsMap.md`, "Dead spruce kit in the map") and use its pipeline and its Wpo master materials. Built by builder A
of a two-builder round (builder B: eight bushes, `.../MineralFlora/Bushes`). **Not placed on any map.**

## What exists

Unreal: `/Game/RTS_Survival/Blueprints/Environment/OreMountains/MineralFlora/Trees/`

| Folder | Assets |
|---|---|
| `Textures/` | `T_OreFloraTrees_BC` (sRGB, alpha = card mask, alpha-coverage mips at 0.333), `T_OreFloraTrees_N`, `T_OreFloraTrees_RMA`. One 2048 atlas for all eight. |
| `Materials/` | `MI_OreFloraTrees_Wood` / `_Cards` (instances of `WpoTreeFoliage/MasterMats/M_Trunks_01` / `M_Foilage_00`, wind on), `_Wood_Still` / `_Cards_Still` (wind off, the log). No new master. |
| `Meshes/` | `SM_OreFlora_<Piece>`, 3 LODs each, Nanite off, one UCX trunk hull, simple-as-complex. |
| root | `BP_OreFlora_<Piece>` (8). |

| Piece | Size | LOD0 / 1 / 2 tris | Height x crown | Crushed by | Nav area | Blueprint copied from |
|---|---|---|---|---|---|---|
| RustSpruce_16m | large | 584 / 284 / 64 | 15.8 m x 6.1 m | HeavyTank | `RTS_NavArea_BlockCharLightMedium` | `BP_OreDeadSpruce_Survivor_9m` (AWpoTree) |
| RustSpruce_8m | medium | 424 / 174 / 40 | 8.2 x 3.3 | MediumOrHeavyTank | `RTS_NavArea_BlockCharLight` | same |
| OchreLarch_15m | large | 432 / 214 / 58 | 15.0 x 6.4 | HeavyTank | `BlockCharLightMedium` | same |
| OchreLarch_7m | medium | 294 / 136 / 34 | 7.1 x 2.8 | MediumOrHeavyTank | `BlockCharLight` | same |
| RedBroadleaf_15m | large | 472 / 214 / 31 | 14.7 x 6.8 | HeavyTank | `BlockCharLightMedium` | same |
| OchreBirch_8m | medium | 436 / 236 / 41 | 7.8 x 5.1 | MediumOrHeavyTank | `BlockCharLight` | same |
| RustLog_7m | medium | 204 / 120 / 58 | 0.8 x 8.7 (lying along +X, butt at the pivot) | MediumOrHeavyTank | `BlockCharLight` (NavBlock box 7.3 x 1.1 m) | `BP_OreCrushRock_Base` (sinks) |
| OchreSnag_5m | medium | 177 / 131 / 37 | 5.7 x 2.2 (trunk 4.8 m + splinters) | MediumOrHeavyTank | `BlockCharLight` | `BP_OreDeadSpruce_Survivor_9m` |

Heights and widths are the mesh at scale 1. The copied construction script scales every standing tree at random
(0.9-1.08, XY times 0.95-1.1, as the dead spruces), so a 12.8 m spruce can stand about 13.8 m.

The dead spruce kit is 300-500 tris per tree at LOD0 (LOD1 up to 224, LOD2 up to 52). Spruce_13m, the broadleaf and the
birch are 10-20 % over at LOD0; LOD1 and LOD2 are inside the kit's range. The default game frame shows LOD1 (screen
sizes 1.0 / 0.45 / 0.06, as the kit): there the broadleaf and birch keep one dome per clump and the main limbs only
(3-sided); at LOD2 they are a trunk plus one large dome per main limb pulled in over the trunk, no limb tubes.

Source: `D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\OreMineralFlora\Trees\`
(`Model/mft_atlas.py`, `Model/mft_build.py`, `Model/mft_render.py`, `OreFloraTrees_pieces.json`, `Model/OreFloraTrees.blend`, `Model/FBX/`).
Unreal tools: `Source/Tools/OreMineralFlora/trees/`. Pictures and reports: `Saved/OreMountains/flora/trees/`.

## Shapes

- **Rust spruces:** the dead spruce kit's whorl generator with rust-red needle branch cards on a near-black trunk (the
  user's "red tree" is `BP_OreDeadSpruce_Survivor_9m`). The 13 m tree is dense; the 8 m one is half shed (bare twig cards
  below 42 % of its height, rust cards above).
- **Ochre larches:** same generator, level, slightly upturned branches carrying ochre needle tufts (larch in autumn).
- **Red broadleaf:** a beech / rowan type: grey trunk forking at 2.9 m into six limbs; the crown is leaf clumps drawn as
  two stacked top-facing domes (what the RTS camera sees), a few hanging leaf sprays, and some dry gaps (bare twig cards).
- **Ochre birch:** pale birch bark with black lenticels and ochre lichen spots, a central leader with nine ascending
  branches, oval ochre clumps and hanging ochre leaf sprays.
- **Rust log:** fallen spruce: snapped butt with red-rot end grain, a red-rot split on top, bracket fungi on the sides,
  rust needle branches at the crown end and a rust needle litter disc on the ground under them.
- **Ochre snag:** broken stump with a jagged top and red-rot heart, silver bark under ochre crust lichen, nine ochre
  bracket shelves (they face up, so they read from the RTS camera), beard lichen and an ochre lichen disc at the foot.

## Colour

Targets come from the map's own game-camera views (`Saved/OreMountains/critic/min3/53_d_iron0.png`, `55_e_ochre0.png`;
`sample_ground_colours.py` -> `Saved/OreMountains/flora/trees/ground_colours.json`), sRGB means:

| Ground patch (min3) | Mean sRGB | Hue | Saturation | Value |
|---|---|---|---|---|
| IronOxide core a / b | (104, 61, 45) / (143, 88, 68) | 16 / 17 deg | 0.57 / 0.53 | 0.41 / 0.56 |
| Ochre core a / b | (162, 131, 90) / (141, 111, 76) | 34 / 33 deg | 0.45 / 0.46 | 0.64 / 0.55 |
| Pale ground | (154, 130, 109) | 29 deg | 0.29 | 0.60 |

Engine measurement (`measure_colours.py`): every close-up is shot twice from the same camera, once with the piece
hidden; the piece's pixels are those whose chromaticity changed, minus pixels that are only a darker copy of the
background (its shadow). "All" is the mean of those pixels (crown undersides and trunk included; the foliage / wood
split in the script is inert, nearly every piece pixel passes its saturation cut). "Lit" is the brightest 30 % of them,
which is what the eye reads as the colour. Runs 3 and 4 used the same atlas (v3); the spread between them is noise from
the random yaw / scale and the wind (`OchreLarch_7m` lit 37 deg in run 3, 28 in run 4).

| Piece | Lit sRGB, run 3 | Lit hue run 3 / run 4 | Lit saturation | All: hue / sat (run 3) |
|---|---|---|---|---|
| RustSpruce_13m | (174, 62, 25) | 15 / 18 | 0.86 / 0.80 | 19 / 0.80 |
| RustSpruce_8m | (181, 90, 43) | 20 / 15 | 0.76 / 0.87 | 23 / 0.75 |
| RedBroadleaf_10m | (194, 91, 40) | 20 / 19 | 0.79 / 0.84 | 21 / 0.77 |
| RustLog_7m | (174, 90, 42) | 22 / 23 | 0.76 / 0.74 | 22 / 0.77 |
| OchreLarch_12m | (209, 152, 48) | 39 / 39 | 0.77 / 0.77 | 33 / 0.75 |
| OchreLarch_7m | (207, 145, 49) | 37 / 28 | 0.77 / 0.81 | 28 / 0.76 |
| OchreBirch_8m | (209, 157, 49) | 40 / 31 | 0.77 / 0.74 | 31 / 0.78 |
| OchreSnag_4m (fungi, lichen) | (206, 152, 59) | 38 / 38 | 0.71 / 0.73 | 28 / 0.74 |
| BP_OreDeadSpruce_Survivor_9m (reference) | (178, 114, 59) | 27 / 27 | 0.67 / 0.68 | 26 / 0.70 |

- **What the numbers show.** Lit, the red family is 15-23 deg and the ochre family 28-40 deg (mostly 37-40), with the
  reference rust spruce at 27 deg between them; in the pictures the two families read apart (brick red vs golden
  ochre). On "all" pixels the families overlap more (red 19-23, ochre 28-33), because shaded card undersides pull
  everything towards dark orange. The pictures carry the "clearly red / clearly yellow" claim more than the means do.
- **Against the layers.** The ground samples above are means of lit and shaded ground (IronOxide 16-17 deg, Ochre
  33-34 deg). The red family sits at or a few degrees warmer than IronOxide; the ochre family at or a few degrees
  yellower than Ochre; both 0.2-0.3 more saturated than their ground, so a tree on its own layer stands out by
  saturation and value instead of vanishing. This was **not** checked on the layers themselves (see below).
- **The test map is not the Ore map.** `RTS_ToCopySmall` is orange desert ground (130, 75, 36), hue 25, saturation 0.72,
  under warm light. Estimate only (two different masks on one control tree): the reference spruce reads hue 26-27 /
  saturation ~0.7 here and about 26 / 0.63 in the min3 Ore views, so hue seems to carry over and saturation may be
  ~0.07 lower on the Ore map.
- **Iterations.** v1 (red authored at 7 deg, ochre at 39 deg, saturation 0.9) rendered candy red (about 7 deg / 0.95) and
  lemon yellow (about 42 deg / 0.83) by a hue-window estimate. v2 authored on the layer hues (red 12-20, ochre 33-37);
  v3 (current) authors red ~2-4 deg lower (9-17) and gives the birch its own yellower set (36-40). The v2 -> v3 change
  is near the measurement noise; v3 was kept, and colour iteration on this map stopped there. Pictures: v2 in
  `Saved/OreMountains/flora/trees/v2_colours/`, v3 run 3 in `v3_run3/`, run 4 in the folder itself.
- Trunks stay near-black (spruce), grey (larch, beech, log), pale (birch).
- Values are the atlas constants `RED*`, `OCH*`, `BIR*` in `mft_atlas.py`; `COLOUR_GAIN` there scales them for a later
  correction without touching the drawing code.
- An attempt to put layer-coloured swatches under three trees each in run 4 failed (`GameplayStatics` has no
  deferred spawn in python); `swatch_iron.png` / `swatch_ochre.png` show those trees on the orange test ground only.
## How the crush works

- Standing pieces are `AWpoTree` duplicates: the copied `BP_WpoTree` graph calls `SetupCrushDestructionOverlap(Tree,
  TankDestructionType, VerticalCollapse)`; the tree tips over the crushing tank's direction and despawns after 5 s.
  The HP comes from the copied construction script's data-table row (360-410 at spawn), as on the dead spruces; the class
  default `Health` stays 300.
- The log is a duplicate of `BP_OreCrushRock_Base` (the crushable-rock graph): it sinks `CrushSinkDeltaZ` 128 cm in
  2.6 s with a near-zero lean (`CrushTipSpeedScale` 0.02). A WpoTree tip-over would swing a lying 7 m log on end or
  into the ground. Its nav box is a `NavBlock` box over the trunk hull padded by 25 cm. Death by damage throws the four
  wood-hole splinters with `Cue_Collapse_Tree`. The crush sound and puff are the rock graph's literals
  (`Collapse_Cue`, `NS_DestructionSmoke`): a rubble sound on a log; changing it needs a graph edit.

## Test and verification

**Fresh-process read-back** (`ue_trees_verify.py`, `Saved/OreMountains/flora/trees/ue_verify_report.json`): 0 problems.
LODs and triangles match the build, screen sizes 1.0 / 0.45 / 0.06, Nanite off, one hull each, slot materials, crush
type, nav area, `Tree` / `MainMesh` mesh, `Health` 300, atlas alpha coverage 0.333 with scaled mips. No asset depends on
`/DeadForest/`, `/OrchardTrees/` or `/CrushableRocks/`; outside the folder they depend on `BP_WpoTree`,
`BP_NaturalCover`, the nav areas, `DT_RTS_DestructableEnvActorData`, `NS_DestructionSmoke`, `Collapse_Cue`,
`Cue_Collapse_Tree`, the collapse sound settings and the four `SM_WoodHoleSplinter_*`.

**Engine test** (`flora_tree_test.py` via `run_tree_test.ps1`; one throwaway DebugGame editor on
`Maps/UnitTests/RTS_ToCopySmall`, never saved). Four runs; the results below are run 4 (final meshes,
`Saved/OreMountains/flora/trees/test/result.json`, `trees4.log`); run 3 gave the same verdicts on every shared lane
(`v3_run3/result_run3.json`). Each vehicle starts 24 m before its piece and is ordered 24 m past it:

| Lane | Piece (rule) | Vehicle | Result (run 4) |
|---|---|---|---|
| L1 | RustSpruce_13m (Heavy) | Pz II Luchs | pathed round, 2.7 m to the side; tree untouched |
| L2 | RedBroadleaf_10m (Heavy) | Pz IV G | pathed round, 3.1 m; untouched |
| L3 | OchreLarch_12m (Heavy) | Tiger II | drove through 10 cm off the line; crush 9.6 s after the order; actor gone at 15.8 s |
| L9 | RedBroadleaf_10m (Heavy) | Tiger II | through, 0 cm; crush 9.6 s; gone 15.8 s |
| L10 | RustSpruce_13m (Heavy) | Tiger II | through, 0 cm; crush 9.6 s; gone 15.8 s |
| L4 | OchreLarch_7m (Medium) | Pz IV G | through, 3 cm; crush 5.2 s; gone 12.5 s |
| L6 | RustLog_7m (Medium) | Pz IV G | through the log's middle, 2 cm; crush 5.1 s (sinks); gone 8.7 s |
| L7 | OchreSnag_4m (Medium) | Pz IV G | through, 2 cm; crush 5.1 s; gone 12.4 s |
| L5 | RustSpruce_8m (Medium) | Pz II Luchs | pathed round, 2.7 m; untouched |
| L8 | OchreBirch_8m (Medium) | Pz II Luchs | pathed round, 2.7 m; untouched |
| H1a | unmodified `BP_OreDeadSpruce_Skeleton_11m` (Medium) | **Tiger H1** | through, 1 cm; **no crush** |
| H1b | OchreLarch_7m (Medium), trunk 1.5 m off the lane line | **Tiger H1** | through; trunk under the left track; **crushed** at 4.6 s, gone 10.4 s |

Paths across every piece (14 m, each agent on its own navmesh with its own filter), before -> after (run 4):

| Piece class | Character | LightTank | MediumTank | HeavyTank |
|---|---|---|---|---|
| Heavy (13 m spruce, 12 m larch, broadleaf) | 1450-1458 detour -> 1400 straight | 1532-1541 detour -> 1400 | 1587-1598 detour -> 1400 | 1400 straight |
| Medium trees (7 m larch, 8 m spruce, birch, snag) | 1448-1450 detour -> 1400 | 1535-1538 detour -> 1400 | 1400 straight | 1400 straight |
| RustLog_7m (middle / butt / crown end) | 1651 / 1426 / 1426 detour -> 1400 | 1814 / 1492 / 1492 detour -> 1400 | 1400 | 1400 |

("-> 1400" applies to pieces that were crushed; the uncrushed lanes keep their detours.)

**Death by damage of the log** (run 4): `apply_damage` 1e7 on the lineup log: health 300 -> 0, life span 3 s at once,
actor gone 7 s later. The splinter burst is not visible in `test/kill_log.png` (the mesh hides at once and the frame is
dark), so the debris itself is unverified.

- **Tiger H1 does not crush a WpoTree when it drives straight over the trunk.** Six attempts in runs 1-3 (four heavy
  trees, two medium larches) and H1a on an unmodified dead-spruce-kit tree: no crush, each time 0-8 cm off the line.
  With the trunk 1.5 m to the side (under a track) the same Tiger H1 crushed the same medium larch (H1b). `NavAgentType`
  reads HeavyTank at runtime. Tiger II and Pz IV G crush on the centre line. So the Tiger H1's overlapping collision
  does not reach the narrow trunk hull between its tracks; it is pre-existing and hits the existing dead spruces the same
  way. Not fixed here: widening these trunk hulls to ~3 m would also widen every nav footprint (the nav modifier follows
  the collision). It needs a look at the Tiger H1 collision (user decision / C++ or Blueprint owner).
  A python overlap listener bound in the test logged nothing for any vehicle, so it gave no evidence either way.
- The picture timing bug of run 1 (a high-res shot renders a frame later, so moving the camera in the next tick put
  the next camera in the picture) is fixed by a 2 s cooldown after each shot.
**Pictures** (`Saved/OreMountains/flora/trees/`): `lineup_rts_all.png` (all ten in a row with the lanes behind, far RTS
view), `lineup_rts_a.png` / `_b.png` (55 deg down, ~36 m: reference spruce, parked Pz IV, the eight pieces),
`lineup_low.png`, `close_<piece>.png` (+ `_bg` without the piece), `swatch_*.png`, crush stills in `test/crush_*.png`,
`test/lanes_before.png` / `lanes_after.png`, `test/kill_log.png`; Blender previews (final meshes, v3 atlas, Cycles) `blender_preview_lod0|1|2_rts|side.png`.

## Not verified

- The look on `Ger_2ndOreMountains` itself: no picture on the IronOxide or Ochre ground (the map was not opened).
- Wind motion (only stills), LOD transitions in motion, the cost of many instances (no profile).
- Fire / shell damage on these pieces; the log's splinter burst on death by damage; the log's crush sound.
- Which Tiger H1 component misses the trunk (only shown: centre line no, under a track yes).

## Open points / weak spots seen in the pictures

- The broadleaf crown is a stack of hexagonal "umbrellas": the polygon edges of the leaf domes show in close-ups.
- The snag is small from the game camera (a 4 m stump); its colour comes from the fungi and the lichen disc at its foot.
- The log's crush puff is the rock graph's white `NS_DestructionSmoke`, oversized for a log; the crush sound is a rubble
  cue. Both are literals in the copied graph.
- The rust needle litter disc under the log lies flat: on a slope it will float or sink.
- Conifers seen from the side are thin (cards face up), the same as the dead spruce kit.
- Birch (592) and broadleaf (556) LOD0 are above the dead spruce kit's 300-500.
- LOD2 broadleaf: the domes float over the trunk without limbs (far zoom only).

## Revision 2 (2026-11-22, after the on-map review)

Review: `Saved/OreMountains/flora/review/REVIEW.md` (all pieces placed on `Ger_2ndOreMountains`, unsaved). Decisions by the
orchestrating session; one revision round. Tables above are updated to rev2; the run 1-4 sections below describe v3 and
the old piece names (`RustSpruce_13m`, `OchreLarch_12m`, `RedBroadleaf_10m`, `OchreSnag_4m`), which were **deleted**
(`ue_trees_import.py` `RENAMED`; Blueprints and meshes, plus their FBX files).

| Finding | Change |
|---|---|
| Broadleaf: hexagonal umbrella domes, a ring with an empty centre from above, a living full-leaved tree, 2-10x the screen area of any tree | Rebuilt as `RedBroadleaf_15m` with `card_crown()` in `mft_build.py`: 6 limbs spread by the golden angle, irregular clusters of top-facing branch cards (4 at each limb end, 3 at the middle, 2 per sub-branch, 6 over the centre), 42 % of the cards bare twigs. Crown 6.8 x 6.4 m (was 9.3), trunk radius 0.45 m (1.6x), fork at 5 m, 14.7 m tall. No domes except the LOD2 far fans. |
| Birch: "C" crown, in leaf | Same generator with a leader: 9 branches by the golden angle, 3 clusters over the leader, 45 % bare twigs, hanging leaf sprays. Crown 5.0 x 4.3 m. |
| Reds crimson (on-map hue 11-16) | `RED*` +5 deg hue, -0.08 saturation (`RED` (154,72,42)). |
| Ochres lemon-gold (on-map hue 36-40, value 0.77-0.85) | `OCH*` -4 deg hue, value x0.9 (`OCH` (176,113,51)); birch set only 2 deg yellower (`BIR` (180,120,52)), no lemon set. |
| Heavy-only trees no bigger than the kit's medium-crushable trees | `RustSpruce_16m` (15.8 m, r0 0.36 = 1.5x, crown 6.1 m; the low quarter carries bare / sparse fans like the reference), `OchreLarch_15m` (15.0 m, r0 0.33, crown 6.4 m), `RedBroadleaf_15m` (thick trunk, see above). |
| Snag reads as a kit stump | `OchreSnag_5m`: trunk 4.8 m + splinters (5.7 m top), radius 0.43 (1.3x); two upward-facing ochre crust-lichen sheets round the upper trunk (at 60 % and 80 % of the height, 1.6-2.1 m across). |

Unchanged as decided: RustSpruce_8m, both larch shapes (the 7 m one), RustLog_7m (its rock-graph smoke puff and rubble
sound stay a user decision), the Tiger H1 problem (not touched).

**Verification.** Full re-import 0 failures (8 old-named assets deleted, all `True`); fresh-process read-back 0 problems.
Engine test rev2 (`Saved/OreMountains/flora/trees/rev2/test/result.json`), all 10 lanes passed:

| Lane | Piece | Vehicle | Result |
|---|---|---|---|
| L1 / L4 | RustSpruce_16m (Heavy) | Pz IV G / Tiger II | round, 3.0 m to the side / crushed 12.4 s, gone 19.0 s |
| L2 / L5 | OchreLarch_15m (Heavy) | Luchs / Tiger II | round, 2.6 m / crushed 12.4 s |
| L3 / L6 | RedBroadleaf_15m (Heavy) | Pz IV G / Tiger II | round, 3.1 m / crushed 12.3 s |
| L7 / L8 | OchreBirch_8m (Medium) | Pz IV G / Luchs | crushed 6.4 s / round, 2.7 m |
| L9 / L10 | OchreSnag_5m (Medium) | Pz IV G / Luchs | crushed 6.3 s / round, 2.6 m |

Paths across (14 m): heavy pieces Character 1448-1451 (side 161-166 cm), LightTank 1532-1540, MediumTank 1587-1597
(side 291-299), HeavyTank 1400 straight; medium pieces Character ~1450, LightTank ~1535, Medium / Heavy 1400. After a crush
every agent is straight. The thicker trunk hulls widened the detours by at most ~5 cm against v3 (MediumTank side 291-299
vs 290-301): the footprint did not grow noticeably.

**Colour, rev2, test map** (`rev2/engine_colours.json`, lit 30 %, same method as above):

| Piece | Lit sRGB | Hue | Sat | Value |
|---|---|---|---|---|
| RustSpruce_16m | (188, 104, 45) | 25 | 0.76 | 0.74 |
| RedBroadleaf_15m | (177, 105, 48) | 26 | 0.73 | 0.70 |
| OchreLarch_15m | (191, 125, 44) | 33 | 0.77 | 0.75 |
| OchreBirch_8m | (194, 133, 62) | 32 | 0.68 | 0.76 |
| OchreSnag_5m | (178, 111, 52) | 28 | 0.71 | 0.70 |
| Survivor_9m reference | (149, 89, 44) | 26 | 0.71 | 0.58 |

Targets on the map: red ~(160, 85, 56) hue 17 / sat 0.65 / value 0.63; ochre ~(185, 135, 72) hue 33 / 0.61 / 0.73.
On the test map the v3 reds read ~5 deg warmer than on the Ore map (test 15-23 vs map 11-16), so the rev2 reds should land
at roughly hue 20-21 on the map: between IronOxide (17-20) and the reference spruce (23-26), not crimson. The ochres
measure on target hue (32-33) with value 0.75-0.76 here; the map read the v3 ochres ~0.04 brighter in value than the test
map (estimate), so ~0.75-0.8 there, near the 0.73 target. **Not re-measured on the Ore map** (the map was not opened).

Pictures: `Saved/OreMountains/flora/trees/rev2/` `lineup_rts_all|a|b.png`, `lineup_low.png`, top-down `top_all|a|b.png`
(the camera yaw puts the lineup vertical in the top views; the lineup is the right-hand column in `top_all`), close-ups
`close_<piece>.png` (+ `_bg`) for broadleaf, birch, snag, the two big conifers and the reference. Blender previews
`blender_preview_lod0|1|2_rts|side|top.png`.

Still weak after rev2:
- The broadleaf crown is now sparse: from the RTS camera it reads as a tall bare beech with rust leaf tufts at the limb
  ends; the red mass is much smaller than v1 (wanted), but it may read thin next to the spruces.
- The broadleaf's long bare limbs (fork at 5 m) show as dark spokes from above.
- The snag stays small against 15 m trees; its colour now sits on its upper trunk, but the top-down mark is ~2 m.
- Tiger II crush times rose from 9.6 s (run 4) to 12.3-12.4 s with the same start distance; cause not looked into.

## Rebuild

```
# atlas (system python)          -> Model/Textures/T_OreFloraTrees_*.png
python -B D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreMineralFlora/Trees/Model/mft_atlas.py
# meshes, LODs, FBX, pieces json (Blender 4.0; 5.0 also works)
D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup --python .../Trees/Model/mft_build.py
D:/Blender/BlenderSoftware/4.0/blender.exe -b .../Trees/Model/OreFloraTrees.blend --python .../Trees/Model/mft_render.py -- 0   # 0|1|2 = LOD
# Pitfall: take the uv layer and colour attribute references only AFTER creating both. color_attributes.new() reallocates
# the layers; a uv_layers.new() reference taken before it dangles, and writing through it corrupted the heap on 4.0 (crash
# sites moved around: use_nodes, the FBX exporter, a MemoryError). 5.0 happened not to crash. OreDeadForest/odf_build.py
# has the same order.
# Unreal (PowerShell, Source/Tools/OreMineralFlora/trees); every run takes Saved/OreMountains/flora/ue.lock
powershell -File run_ue_cmd.ps1 -Script ue_trees_import.py -NullRhi            # all; -ScriptArgs mat = textures + MIs only
powershell -File run_ue_cmd.ps1 -Script ue_trees_verify.py -NullRhi            # fresh-process read-back
powershell -File run_tree_test.ps1 -Name trees4                                # editor test + pictures (needs >= 4 GB free VRAM)
python -B measure_colours.py                                                   # colours from the close-ups
```

`ue_trees_import.py` is idempotent: it re-imports meshes in place and never deletes a Blueprint (it duplicates the source
only when the Blueprint is missing).
