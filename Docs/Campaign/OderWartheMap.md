# Ger_OderWarthe: the map

`/Game/RTS_Survival/Maps/GerMissions/Ger_OderWarthe`, built 2026-11-16 from sketch C "Corner Ridge over the Lakes"
(`MapDesigner/OderWarthe/df2_c.layout.json`; mission doc [OderWartheMission.md](OderWartheMission.md), flow
[OderWartheMissionFlow.md](OderWartheMissionFlow.md)). Landscape, paint, water, nav and dressing only: **no units and no
mission scripting**. Every site the mission will arm carries an editor-only note (`OW_Note_*`). Reviewed by an
adversarial critic agent each round (`Saved/OderWarthe/critic/critique_r0..`).

## Rebuild

All tools are in `Source/Tools/OderWartheMap`. The map is built in a throwaway editor running `ow_editor_server.py`
(file-driven: `python ow_cmd.py <script.py>`; command folder `Saved/OderWarthe/cmd`). After an editor restart, open the
map first (`ow_map_build.step_duplicate()` opens it when it exists).

```bash
python Source/Tools/OderWartheMap/ow_terrain_gen.py     # heights, 9 paint layers, masks, layout -> Saved/OderWarthe/terrain (+ check.txt)
python Source/Tools/OderWartheMap/ow_dress_gen.py       # every tree, prop, site, decal, nav blocker, note -> OW_dress.json (+ werk_diff.txt)
python Source/Tools/OderWartheMap/ow_cmd.py Saved/OderWarthe/c_build_full.py --timeout 580   # landscape, water, lighting, dressing, nav, save, nav check
python Source/Tools/OderWartheMap/ow_cmd.py Saved/OderWarthe/c_build2.py --timeout 580       # dressing only
python Source/Tools/OderWartheMap/ow_cmd.py Saved/OderWarthe/c_waves.py --timeout 580        # wave routes leg by leg
python Source/Tools/OderWartheMap/ow_cmd.py Saved/OderWarthe/c_nav.py --timeout 580          # the full per-agent nav table
python Source/Tools/OderWartheMap/ow_cmd.py Saved/OderWarthe/c_chokes.py --timeout 580       # choke clear widths
```

A paint or height change needs the full build (the landscape is re-imported); a dressing change needs only
`c_build2.py`.

- Generated actors carry the prefix `OW_` (`OW_D_*` Blueprints, `OW_I_*` instance holders, `OW_M_*` single mesh actors
  (dragon's teeth), `OW_NB_*` nav blockers, `OW_Decal_*`, `OW_FX_*`, `OW_Note_*`, `OW_Water_*`) under the outliner folder
  `OderWarthe/`. Each step clears and redoes its own actors.
- The instanced-mesh `abs_z` key is an absolute height; the item's dz is added to it.
- Materials: the landscape uses `OderWarthe/Map/MI_RTS_Landscape_OderWarthe` (child of the Pripyat landscape instance:
  same nine layers and layer infos). Lakes: `MI_OderWarthe_LakeWater` (murky plum-brown); the Mittelsee algae sheet:
  `MI_OderWarthe_AlgaeWater` (not used since round 5; the algae is landscape paint on the shelf). Kit tints are set by
`tune_kit_materials()` (`KIT_TUNE`). The kit earth (`MI_OW_Earth`) uses the dry-grass texture
`/Game/Brushify/Materials/Landscape/Textures/Grass/T_GrassDry_D` as sod. `PripEarth_Tint` makes the earth layer brown;
the Pripyat default reads olive.
- Checks: `check.txt` (triangle flood fill: every site reachable, no lake leaks, the ridge top only by its ramps, the
  basins isolated with every crossing removed, Werk identity of heights and paint in the Werk frame, tunnel distances),
  `werk_diff.txt` (every object within 60 m of each Werk in its own frame), `ow_nav_check.run()` (editor navmesh with
  each agent's own query filter), `c_waves.py` (each wave route proven leg by leg for infantry, medium and heavy tanks).
- In-game review shots: `Saved/OderWarthe/c_round_shots.py`; base-colour top views `Saved/OderWarthe/c_top.py`.

## The Oder-Warthe kit

Modelled for this map by a subagent (Blender, imported headless): `/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Kit/`
(manifest `Saved/OderWarthe/assets/ow_kit.json`, scripts `Source/Tools/OderWartheMap/assets`, sources
`D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe`): steel cupola, Werk block, tunnel portal, sluice gate,
Adcock and loop direction-finder stations, Werk mast, two dragon teeth, five dead pines (crushable `BP_OW_DeadPine_*`,
medium and heavy tanks) and three granite erratics.

## Size, heights and water

- Play area 700 m (east-west) x 620 m (north-south); landscape 14 x 14 components (882 m).
- One water sheet at z = 0 under the landscape; the ponds behind the Packlitz and Lubow dams stand 1.7 m higher on
  their own 4 m water tiles (`OW_Water_Pond_*`, only over the ponds); the Mittelsee has a purple algae sheet on top.

| Level | z (cm) |
|---|---|
| Lake beds | about -330 to -400 |
| Waterline | 0 (ponds 170) |
| Shore | rises 8.5 degrees for 6 m from the waterline |
| Lowland (glacial sand, rolling) | +120 to +230 |
| Peat bogs (passable, never open water) | +80 |
| Dam and causeway crowns | +265 |
| Werk drumlins | 7 m over their surroundings |
| Ridge top | +1975 (18 m), with a 5-9.4 m escarpment under the rim |

## How movement is gated

- **Chokes** (`c_chokes.py`, every prop in place, a vertical trace every 10 cm across the crossing):

| Choke | Design | Physical clear | Nav width + 2r (heavy tank) |
|---|---|---|---|
| Packlitz sluice | 10 m | 10.2 m | 11.5 m |
| Lubow sluice | 10 m | 10.3 m | 11.2 m |
| Kalau breach | 10 m | 10.6 m | 10.6 m |
| Island culvert breach | 11 m | 11.8 m | 11.5 m |

  The sluice gates and outlets carry all-agent nav blockers. The sluice nav lane is still about 1 m wider than the
  physical deck; the cause was not found (the outlet blocker did not change it).

- **Lakes:** the waterline is the zero contour of a smoothed signed distance; a 0.9-2.3 m shelf under it, then a steep
  cut to the bed. Units wade the shelf (under 1 m), never further.
- **The ridge:** only its three graded ramps (12-14 %) and the road from Berlin (past the play edge).
- **Boardwalks:** 2.5 m dykes, only the Character agent fits.
- **Dragon's teeth** (four staggered rows per Werk, one 12-degree vehicle gap, identical at all five): physical tooth
  meshes plus tank-only nav blockers (`RTS_BP_NavBlocker` with `RTS_NavArea_BlockAllTanks`): infantry walks through,
  tanks drive round to the gap. Geometry alone does not change the navmesh in this project; blocking is by nav area and
  the agents' query filters (`RTS_NavFilter_*`).

## The five Werke

One drumlin stamp (34 x 26 m semi-axes, plateau, stoss end, local lumps, four old craters, the dug-in portal ramp) and
one paint set, both in the Werk frame, and one dressing template (`WERK_TEMPLATE`): Werk block, three steel cupolas,
mast, two Soviet MG pits, rail hedgehogs, wire, crates, the teeth belt, the tunnel portal and three editor notes for the
hidden HQ targets. Placed by centre and yaw only; the yaws keep every tunnel mouth 108-172 m from every resource and
station. Identity: heights within 5 cm, paint within 1/255 (bilinear, in the Werk frame); `werk_diff.txt` 0 differences.

## Wave routes (proven leg by leg, round 8)

| Werk | Route | Length (infantry / medium / heavy) |
|---|---|---|
| W1 | Packlitz dam, north ramp | 678 / 688 / 689 m |
| W2 | island causeway, prow ramp | 556 / 571 / 574 m |
| W3 | Kalau causeway, east ramp | 618 / 619 / 626 m |
| W4 | Hochwald ford, Packlitz dam, north ramp | 824 / 821 / 822 m |
| W5 | Kalau causeway, east ramp | 693 / 699 / 702 m |

## Deviations from the sketch

| Sketch | Map | Why |
|---|---|---|
| Tunnel mouths at the sketch points | 50 m behind each Werk, yaw per Werk, dug in | Clear of resources and stations (critic R0-1) |
| Werk pads with five different outlines | One identical drumlin stamp | Werke must be indistinguishable |
| Dragon's teeth as wire beside the tracks | A belt round each Werk front, tank-only nav | A real mechanic; the same at all five |
| One water level | Ponds 1.7 m higher behind the two dams | The dams hold something back (critic R0-6) |
| Lake arms to the map edge | Meandering arms of varying width, closing 40-80 m past the play edge | Critic R1-1 |
| Outposts at the sketch points | Pushed to at least 75 m from every Werk centre | Critic R1-9 |
| Pump house at DF 1 | A walled 5 x 10 m concrete pump house with intake pipes, on the nearest Packlitz shore | Critic R0-9, R7-6 |
| Sluice: gate across a cut | Intake gate on the upstream face, culvert outlet headwall on the downstream face | The road keeps its 10 m deck; a deck mesh would not carry nav |
| Block buried in the drumlin | Block cover with grass sod, cupolas on level benches | The block fills the plateau; a mound walls off the hill top (round 8 nav) |
| (not in the sketch) | Rail line from the rail yard off the east edge | Critic R1-10 |

## Known items

- The Werk block's earth cover is part of the kit mesh: it reads as a sod-covered box, not a buried bunker.
- The Mittelsee algae is on the shallow shelf only; its deep middle reads as plain water.
- Some steep island cuts show stair steps at the waterline (landscape resolution at 60 degrees).
- Purple speckle strips beside the ford bar: not algae paint (masking it changed nothing). Most likely the water
  material's contact foam over the shallow shelf.
- Critic pointers left open (cosmetic, R8-3..R8-5 in `Saved/OderWarthe/critic/critique_r8.md`): ramp-head pads, the
  rail track mesh's pale ballast, the line behind each portal berm, the sod tiling grid on the block.
- Final grades (critic round 8): realism 6.5/10, fun/readability 7/10. Nothing blocks the design.
- The in-game shots use the fixed RTS camera pitch.
- Kit look checked in game (shots); the snapped pine top was re-imported after a file lock.
