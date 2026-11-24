# Ore Mountains mineral flora: crushable bushes (2026-11-22, rev2)

Nine crushable bush Blueprints for `Ger_2ndOreMountains`: red (IronOxide layer) and ochre-yellow (Ochre layer) plants,
the bush half of the user's 16-object request ("red and yellow dead trees, trunks and bushes ... crushable by vehicles,
large ones only by heavy"). The trees and trunks are a separate set (`.../MineralFlora/Trees`). **Not placed on any map.**

Rev2 is the one revision round after an adversarial review on the real map (`Saved/OreMountains/flora/review/REVIEW.md`):
single bushes were 10-38 px dots at the RTS camera, the reds overshot to crimson, the ochres to lemon-gold, the dogwood
vanished on red ground, the thicket read orange and like a big heath, heath and bilberry were the same idea twice and the
broom was a barrel. What changed is listed per finding under "Rev2 changes".

## What exists

Content: `/Game/RTS_Survival/Blueprints/Environment/OreMountains/MineralFlora/Bushes/`

| Folder | Assets |
|---|---|
| `Textures/` | `T_OreFloraBush_BC` (2048, sRGB, alpha = card mask, alpha-coverage mips at 0.333), `T_OreFloraBush_N` |
| `Materials/` | `MI_OreFloraBush`: instance of `WpoTreeFoliage/MasterMats/M_Foilage_00` (masked, two-sided foliage, ISM usage already saved on the master). No new master. |
| `Meshes/` | `SM_OreFlora_<Name>` (LOD0 + LOD1 at screen size 0.025) and `SM_OreFlora_<Name>_Crushed` for 9 meshes. No collision, Nanite off. |
| root | 9 Blueprints `BP_OreFlora_<Name>` (rev1's mixed `BP_OreFlora_BrambleThicket` was deleted, own asset) |

Source and scripts: `D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\OreMineralFlora\Bushes\`
(`bush_atlas.py`, `bush_build.py`, `render_preview.py`, `Bushes.blend`, `FBX/`, `Textures/`, `bushes.json`, `renders/`;
rev1 copies `*.rev1.bak`). Unreal tools: `Source/Tools/OreMineralFlora/bushes/` (`ue_build.py`, `bush_engine_test.py`,
`bush_rev2_measure.py`, `bush_colours.py` (rev1), `run_ue.sh`, `run_engine_test.sh`, `ue_probe.py`).
Pictures and reports: `Saved/OreMountains/flora/bushes/rev2/` (rev1 pictures one level up).

| Blueprint | Colour | Plants per actor (default) | Plant mesh X x Y x H (cm, before scale) | Plant scale | Tris LOD0 / LOD1 / crushed per plant | Crushed by | Nav |
|---|---|---|---|---|---|---|---|
| `RustHeath` (heather mound) | red | 4 (2 x 2 clump) | 215 x 229 x 120 | 1.30-1.50 | 134 / 53 / 36 | AnyTank | none |
| `RedDogwood` (fanned red stems) | red | 4 | 285 x 300 x 191 | 1.15-1.25 | 54 / 26 / 28 | AnyTank | none |
| `RedBilberryMat` (flat carpet) | red | 6 lobes in 3.5 x 2.5 m | 137 x 138 x 24 | 1.05-1.20 | 84 / 34 / 28 | AnyTank | none |
| `RustRowan` (saplings) | red | 4 | 206 x 196 x 259 | 1.15-1.30 | 122 / 62 / 32 | AnyTank | none |
| `OchreBroom` (open sprays) | ochre | 4 | 231 x 244 x 156 | 1.30-1.45 | 80 / 38 / 36 | AnyTank | none |
| `OchreBracken` (fern clump) | ochre | 3 | 175 x 190 x 85 | 1.35-1.50 | 120 / 56 / 36 | AnyTank | none |
| `OchreTussock` (reed clump) | ochre | 3 | 184 x 183 x 120 | 1.35-1.50 | 84 / 38 / 32 | AnyTank | none |
| `BrambleThicket_Rust` (hedge) | red | 9 clumps, 2 offset rows, ~6.3 x 2.9 m | 206 x 216 x 105 | 0.85-0.95 | 164 / 50 / 32 (1476 for the hedge) | MediumOrHeavyTank | `RTS_NavArea_BlockLight` per standing clump |
| `BrambleThicket_Ochre` (hedge) | ochre | 9 | 216 x 207 x 105 | 0.85-0.95 | 164 / 50 / 32 | MediumOrHeavyTank | same |

Plant counts and positions are fixed per Blueprint (layout seed is a class default) and were read back from the spawned
actors. Clump plants sit on a 2 x 2 grid 2.3 m apart, jittered 30 % (up to 69 cm), random yaw: e.g. RustHeath plants at
(-47,-78), (72,-126), (-55,47), (65,101) cm. Hedge clumps at y about -50 (5 clumps, 1.15 m apart) and +50 (4 clumps,
offset half a step).

## How a bush works (crush system)

All 9 are children of `ACrushableCropFieldActor` (C++, no Blueprint graph), as the existing crushable brush on
`Ger_IntroMission`. Seven are children of `BP_CropField_Wheat`, the two hedges of `BP_CropField_Vineyard`. Python sets only
class defaults: `Variants`, `Layout`, `CrushDestructionType`, `StandingPlantsNavArea`, shadows (standing on, crushed off),
`CrushFX` (vineyard set for woody plants and hedges, wheat set for soft plants) and the default `FieldOutline`.

- **One actor = one placeable object.** The default outline is the piece's footprint: a 4.6 m square for the clumps, a
  3.5 x 2.5 m rectangle for the mat, a trapezoid for the hedge (6 m at the back row, 3.4 m at the front) so the grid
  gives two offset rows of 5 + 4. Verified in a fresh process and after an in-memory recompile.
- **Crushing is per plant**: a plant is crushed when its origin is under a passing vehicle's footprint (+25 cm). A tank
  crossing a clump or hedge crushes only the plants in its lane; the rest stay (tested).
- **No collision at all**: shells, bullets and traces pass; infantry walk through (as the existing brush and vineyards).
- **Navigation**: clumps and the mat add no nav area. Each standing hedge clump marks a 1.45 x 1.3 m `BlockLight` cell
  (spacing + 15 cm each side): neighbouring cells overlap by 30 cm, and the rows overlap, so a light vehicle finds no gap
  while the hedge stands. Light agents path round; medium, heavy and infantry path through; crushed clumps drop their cell.
- **Detection box (open item)**: each actor's vehicle scan box is its plants plus 450 cm (`CrushableCropField::DetectionMargin`
  and `DetectionHalfHeight` 500, constants in `CrushableCropFieldActor.cpp`, set in `UpdateDetectionBox()`; not a
  property, so it cannot be set per Blueprint headless). Actor bounds read 10-17 m boxes. Clumps cut the actor count about
  4x against rev1's single bushes; shrinking the margin needs a C++ change (user decision).

## Placing

- Place the Blueprints as single actors and **vary the actor yaw** (the layout seed is a class default, so copies of one
  Blueprint have the same plant arrangement). Hedges are built along local X.
- Stretching an instance's `FieldOutline` makes a bigger patch on the same 2.3 m grid (the clump layout tested in rev2).
- Rebuild navigation after placing hedges.

## Rev2 changes (per review finding)

1. **Dots at the RTS camera** -> every Blueprint is a clump by default (3-4 plants in 4.6 m; mat 6 lobes; hedges 9 clumps)
   and every plant is scaled up (heath 1.3-1.5x, about 3 m; dogwood 2.2-2.4 m tall; tussock 1.6-1.8 m). In the
   reviewer's 34 m view each piece now covers 2.5-9.5 k px (rev1 single bushes on the map: 0.08-1.5 k). See "Measured".
2. **Reds overshoot to crimson** -> `bush_atlas.py` `RED_SHIFT` +5 deg hue, -0.08 saturation on the red palette (atlas
   heath card r/g 2.00 -> 1.66). The bilberry keeps half the shift (+2 deg, -0.04), so it stays the reddest ground cover.
3. **Dogwood vanishes on red ground** -> rebuilt as 6 separate stem fans tilted 35-45 deg outward plus 2 near-upright
   ones; each card widens to the tips (twigs, not a blade); colour tips (190, 72, 50), base x0.63 (~(120, 45, 31)).
4. **Thicket orange, reads as a heath** -> two single-colour Blueprints `BrambleThicket_Rust` / `_Ochre` sharing the atlas;
   a low hedge of 9 clumps (about 1 m high) in two offset rows, about 6.3 x 2.9 m; dark cane cards (new `canes` atlas
   region) arch out low between the leaf fronds plus 3 cane sticks per clump; leaves only in one colour per card.
5. **Heath and bilberry the same idea** -> the bilberry is a flat lobed carpet: 6 lobes of 12 near-flat mat strips (new
   `bilberry_mat` region) and 6 short sprigs, no domed card, 24 cm high before scale (26-29 cm placed), in a 3.5 x 2.5 m
   outline. Kept as several plants, not one wide plant: a tank crushes the lobes in its lane and leaves the rest, and
   each lobe stays a normal-size instance for LOD. The heath top card no longer has the dark stem knot in the centre
   (twigs start off-centre, a leaf cover is drawn over the middle).
6. **Ochres lemon-gold** -> `OCH_SHIFT` -4 deg hue, -0.08 value on the ochre palette.
7. **Broom barrel** -> no top card; 10 sprays splayed 30-40 deg, one side up to 1.35x longer than the other.

Rowan, bracken and tussock: colour shift and clump/scale only, meshes unchanged.

## Measured (rev2 engine test on the unit-test map)

The unit-test map is orange desert sand under a warm, low light with **moving cloud shadows**, not the Ore map's light,
so hue numbers are not comparable 1:1 with the review's targets (red lit ~ (160, 85, 56) H17, ochre lit ~ (185, 135, 72)
H33); the test sand itself reads H25-27. `bush_rev2_measure.py` repeats the reviewer's method (bg shot without the
bushes, shadow pixels dropped, brightest 30 % of the saturated pixels) inside each plant's own projected disc.
Reviewer's pose: 22 m back, 34 m up, FOV 90 (`rts34.png`, `bg_rts34.png`); 45 m up (`rts45.png`). Map-ground row:

| Piece | px at 34 m | bbox (px) | lit sRGB at 34 m | H / S / V | dE to ground | px at 45 m |
|---|---|---|---|---|---|---|
| RustHeath | 7596 | 110 x 116 | 167, 96, 42 | 26 / 0.75 / 0.65 | 16.7 | 3380 |
| RedDogwood | 9541 | 111 x 139 | 127, 42, 16 | 14 / 0.87 / 0.50 | 18.1 | 5900 |
| RedBilberryMat | 2452 | 87 x 77 | 142, 64, 25 | 20 / 0.82 / 0.56 | 14.7 | 1202 |
| RustRowan | 6242 | 94 x 128 | 130, 66, 26 | 23 / 0.80 / 0.51 | 13.8 | 3228 |
| BrambleThicket_Rust | 6290 | 156 x 91 | 147, 77, 29 | 25 / 0.80 / 0.58 | 14.2 | 3280 |
| BrambleThicket_Ochre | 6397 | 168 x 87 | 169, 107, 32 | 33 / 0.81 / 0.66 | 28.9 | 3132 |
| OchreBroom | 8227 | 143 x 125 | 119, 69, 21 | 29 / 0.82 / 0.47 | 12.0 | 4722 |
| OchreBracken | 4466 | 100 x 93 | 166, 103, 31 | 32 / 0.82 / 0.65 | 21.1 | 3010 |
| OchreTussock | 4972 | 125 x 83 | 123, 71, 23 | 29 / 0.81 / 0.48 | 5.5 (cloud shadow over it) | 2824 |

Full table incl. the IronOxide / Ochre swatch rows: `rev2/measure.txt`; masks in `rev2/masks/`. The swatch rows partly lie
under a cloud shadow in this run. Relative order holds: the reds sit 3-12 deg redder than the test ground, the ochres
4-8 deg yellower; under the warm test light both families read warmer than they will on the Ore map.

## Test (`bush_engine_test.py`, rev2_run1, 2026-11-22)

One throwaway editor on `UnitTests/RTS_ToCopySmall`, never saved, under the flora lock; `rev2/test/result.json`, log
`Saved/OreMountains/flora/bushes/test/rev2_run1.log`.

- Plant counts as in the table; all ISMs `NoCollision`, standing shadows on.
- **A** RustHeath clump + Luchs driving 1.15 m off-centre (over one column): 4 -> 2 standing, the two in the other column
  untouched.
- **B / B2** Rust and Ochre hedge + Luchs: both pathed round (4.3 m / 4.1 m to the side), 9 -> 9.
- **C / C2** Ochre and Rust hedge + Pz IV G through the middle: 9 -> 2 each (the two end clumps outside its lane stay).
- **D** OchreBroom clump + Pz IV G: 4 -> 0.
- **Paths** (12 m across, per agent with its own filter): clumps all agents 1200 straight before and after. Hedges before:
  LightTank 1558-1588 cm (415-437 to the side); Character, Medium, Heavy 1200 straight. **Gap probe** (five light-agent
  crossings at -230, -115, 0, 115, 230 cm along each hedge): no crossing goes through, all four hedges. After the Pz IV
  lane: the light crossing at 0 goes straight through the trampled lane, the others still detour.
- **Teleport** of the lane-B2 Luchs onto the Ochre hedge for 2.5 s: 9 -> 9 (crush gate refuses light). The lane-B Luchs
  (Rust hedge) was no longer alive at that point; the map has enemy bazooka infantry (`Rus_Inf_Hazmat_Bazooka_C`) and it
  was most likely killed. So the teleport check covers the Ochre hedge only; the Rust hedge has the identical crush type
  and refused the Luchs by pathing (9 -> 9).
- **Shells**: 45 line traces (every trace channel and object type) through 15 pieces at 30/70/120 cm: none hit a bush.

## Weak points (own pictures, rev2)
- The bilberry lobes read as six separate leafy pads with gaps, not one continuous carpet (`close_RedBilberryMat.png`);
  raising the mat's plant scale to about 1.35-1.5 would close them (not done after the single test run).
- The broom clump: four splayed brooms overlap into a dark-cored cluster; the vase reads only up close.
- The Rust hedge still reads orange-brown under the warm test light (lit H25); on the Ore map the same red cards (heath)
  measured H15-18 before the shift, so expect about H20 there. Not verified on the map.
- The swatch rows and part of the hedges sit under a moving cloud shadow in `rts34.png` / `topdown.png`.
- Crush effects are inherited stand-ins (vine debris / wheat chaff and cues); the white puff covers the spot for seconds.

## Rebuild

```
python -B bush_atlas.py                                                  # atlas (~30 s); palette + RED_SHIFT/OCH_SHIFT at the top
D:/Blender/BlenderSoftware/5/Blender5.0.exe -b --factory-startup -P bush_build.py     # Blender 4.0 crashed in the FBX export
D:/Blender/BlenderSoftware/5/Blender5.0.exe -b Bushes.blend -P render_preview.py
bash run_ue.sh ue_build.py <log> import bps     # Source/Tools/OreMineralFlora/bushes, takes Saved/OreMountains/flora/ue.lock
bash run_ue.sh ue_build.py <log> verify         # fresh process: outlines, plant counts/positions, recompile, vertex colours, LODs
sh run_engine_test.sh <log> 760                 # throwaway editor, ~4 min; refuses under 4 GB free video memory
python -B bush_rev2_measure.py                  # pixel footprint + lit colours
```

- FBX in centimetres (vertices x100, scene unit 0.01, `FBX_SCALE_NONE`); Blender +Y is Unreal -Y; `ue_build.py` checks
  every mesh's bounds against `bushes.json`.
- Blender 4.0 on this machine crashes in the FBX export (python310 access violation / heap corruption, TBBmalloc hook
  warning at start); Blender 5.2 runs the same scripts cleanly.
