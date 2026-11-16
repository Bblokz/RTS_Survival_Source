# Ger_OderWarthe: the map

`/Game/RTS_Survival/Maps/GerMissions/Ger_OderWarthe`, built from sketch C "Corner Ridge over the Lakes"
(`MapDesigner/OderWarthe/df2_c.layout.json`; mission doc [OderWartheMission.md](OderWartheMission.md), flow
[OderWartheMissionFlow.md](OderWartheMissionFlow.md)). Landscape, paint, water, nav and dressing only: **no units and no
mission scripting**. Every site the mission will arm carries an editor-only note (`OW_Note_*`).

Status: in progress (first build 2026-11-16, critic rounds running). This doc is updated as the rounds land.

## Rebuild

All tools are in `Source/Tools/OderWartheMap`. The map is built in a throwaway editor running `ow_editor_server.py`
(file-driven: `python ow_cmd.py <script.py>`; command folder `Saved/OderWarthe/cmd`).

```bash
python Source/Tools/OderWartheMap/ow_terrain_gen.py     # heights, 9 paint layers, masks, layout -> Saved/OderWarthe/terrain
python Source/Tools/OderWartheMap/ow_dress_gen.py       # every tree, prop, site, nav blocker, note -> OW_dress.json (+ werk_diff.txt)
python Source/Tools/OderWartheMap/ow_cmd.py Saved/OderWarthe/c_build1.py --timeout 1800   # map, landscape, water, lighting
python Source/Tools/OderWartheMap/ow_cmd.py Saved/OderWarthe/c_build2.py --timeout 1800   # dressing, nav rebuild, save, nav check
```

- Generated actors carry the label prefix `OW_` (`OW_D_*` Blueprints, `OW_I_*` instance holders, `OW_M_*` single
  mesh actors (dragon's teeth), `OW_NB_*` nav blockers, `OW_FX_*`, `OW_Note_*`) under the outliner folder `OderWarthe/`.
- The landscape uses its own instance `OderWarthe/Map/MI_RTS_Landscape_OderWarthe`, a child of the Pripyat landscape
  instance (same nine layers and layer infos, so no headless layer-info step). The lakes use
  `OderWarthe/Map/MI_OderWarthe_LakeWater` (child of the Pripyat algae water, retinted murky plum-brown).
- Checks: `ow_terrain_gen.py` writes `check.txt` (triangle flood fill: every site reachable, no lake leaks, the ridge top
  only through the ramps, Werk identity, tunnel distances); `ow_nav_check.run()` tests the editor navmesh with each
  agent's own query filter.
- In-game review shots: `Saved/OderWarthe/c_round_shots.py` (play session, fog of war off).

## Size, heights and water

- Play area 700 m (east-west) x 620 m (north-south); landscape 14 x 14 components (882 m).
- One water sheet at z = 0 under the whole landscape; the two **dammed ponds** behind the Packlitz and Lubow dams stand
  1.7 m higher on their own sheets (`OW_Water_Pond_*`).

| Level | z (cm) |
|---|---|
| Lake beds | about -280 |
| Waterline foot of every lake bank (the blocking 60 degree cut starts here) | -15 |
| Shore land | +35, easing up to the lowland over 18 m |
| Lowland (glacial sand, rolling) | +120 to +230 |
| Peat bogs (passable, never open water) | +80 |
| Dam / causeway crowns | +265 |
| Werk hills | 7 m over their surroundings |
| Ridge top | +1975 (18 m), with a 7.2 m 60 degree escarpment under the rim |

## How movement is gated (no nav volumes)

- **Lakes** block every agent: the bank drops 2.6 m at 60 degrees just under the waterline (units can wade the first
  metre of a shore, no more).
- **The ridge** is reached only by its three graded ramps (12-14 %); the escarpment band is continuous.
- **Boardwalks** are 2.5 m dykes: only the Character agent fits.
- **Dragon's teeth** (four staggered rows per Werk, one 12 degree vehicle gap, identical at all five): the tooth meshes
  are physical, and tank-only nav blockers (`RTS_BP_NavBlocker` with its obstacle set to `RTS_NavArea_BlockAllTanks`)
  cover the belt, so infantry walks through and tanks drive round to the gap. Geometry alone does not change the
  navmesh in this project (tested): the blocking is by nav area and the agents' query filters.

## The five Werke

One template (`WERK_TEMPLATE` in `ow_dress_gen.py`) and one hill stamp (`ow_terrain_gen.py`), placed by centre and yaw
only. The yaws were chosen so every tunnel mouth stands 108-172 m from every resource and station (the sketch's tunnel
points were 13-41 m from resources). `werk_diff.txt` compares the five in their own frames: 147 template pieces each,
0 differences.

## Deviations from the sketch

| Sketch | Map | Why |
|---|---|---|
| Tunnel mouths at the sketch points | 50 m behind each Werk, yaw per Werk | Clear of resources and stations (critic R0-1) |
| Werk pads with five different outlines | One identical 7 m hill stamp | Werke must be indistinguishable |
| Dragon's teeth as wire beside the tracks | A belt of four staggered rows round each Werk front, tank-only nav | A real mechanic; same at all five |
| One water level | Ponds 1.7 m higher behind the two dams | The dams hold something back (critic R0-6) |

## Open items

- Kit pieces still stand-ins until the modelling agent lands them (see `standins` in `OW_dress.json`).
- Critic rounds in progress (`Saved/OderWarthe/critic`).
