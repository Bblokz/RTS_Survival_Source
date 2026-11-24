# Berlin raid (sketch v6) (berlin_01, campaign)

**0 errors, 1 warnings, 22 notes.**

## Warnings
- `spawn_close` feature "response_road": 74 m from start "start" (minimum 120 m)

## Infos
- `opening_load` feature "gate_passage": no shortest enemy route uses it
- `opening_load` feature "sink_gap_small": no shortest enemy route uses it
- `opening_load` feature "sink_gap_all": no shortest enemy route uses it
- `opening_load` feature "gap_nce_small": no shortest enemy route uses it
- `opening_load` feature "gap_nce_inf": no shortest enemy route uses it
- `opening_load` feature "gap_ncw_small": no shortest enemy route uses it
- `opening_load` feature "gap_ncw_inf": no shortest enemy route uses it
- `opening_load` feature "gap_cse_small": no shortest enemy route uses it
- `opening_load` feature "gap_cse_inf": no shortest enemy route uses it
- `opening_load` feature "gap_csw_small": no shortest enemy route uses it
- `opening_load` feature "gap_csw_inf": no shortest enemy route uses it
- `opening_load` feature "moltke_bridge": no shortest enemy route uses it
- `opening_load` feature "gap_flank_small": no shortest enemy route uses it
- `opening_load` feature "gap_flank_inf": no shortest enemy route uses it
- `opening_load` feature "hbf_gap_south": no shortest enemy route uses it
- `mission` objective "survey_data_obj": type "capture" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "reach_zoo": type "reach_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "broadcast": type "defend_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "escape_west": type "reach_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "escape_north": type "reach_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "escape_south": type "reach_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `coverage` features: 6 of 16 map sectors have no content: row 1 (from north), col 1 (from west); row 1 (from north), col 3 (from west); row 3 (from north), col 2 (from west); row 3 (from north), col 4 (from west); row 4 (from north), col 2 (from west); row 4 (from north), col 4 (from west)

## Metrics
- Size 520.0 x 1000.0 m, analysis grid 150 cm, passable 92%, cliff bands 0%
- Landscape suggestion: 19x19 components of 63 quads at 100 cm (119700 cm, 9850 cm past the play edge)
- Start `start` on `start_ruins`: build area 5234 m2, 100% passable, 100% flat

### Enemy routes (spawns and bases)

| source | to | route m | straight m | detour | time s | through |
|---|---|---|---|---|---|---|
| response_rail | start | 262 | 248 | 1.06 | infantry 86.4, vehicle 51.8 | - |
| response_road | start | 77 | 74 | 1.04 | infantry 25.0, vehicle 15.0 | - |
| shield_base | start | 598 | 536 | 1.12 | infantry 178.9, vehicle 107.4 | - |

### Openings

| id | kind | width cm | bearing | cuts barrier | enemy route share |
|---|---|---|---|---|---|
| gate_passage | pass | 1000 | 270 | yes | 0% |
| sink_gap_small | pass | 600 | 210 | yes | 0% |
| sink_gap_all | pass | 1000 | 201 | yes | 0% |
| gap_nce_small | pass | 600 | 52 | yes | 0% |
| gap_nce_inf | pass | 400 | 56 | yes | 0% |
| gap_ncw_small | pass | 600 | 26 | yes | 0% |
| gap_ncw_inf | pass | 400 | 0 | yes | 0% |
| gap_cse_small | pass | 600 | 77 | yes | 0% |
| gap_cse_inf | pass | 400 | 76 | yes | 0% |
| gap_csw_small | pass | 600 | 115 | yes | 0% |
| gap_csw_inf | pass | 400 | 163 | yes | 0% |
| moltke_bridge | bridge | 600 | 324 | yes | 0% |
| gap_flank_small | pass | 600 | 19 | yes | 0% |
| gap_flank_inf | pass | 400 | 22 | yes | 0% |
| hbf_gap_south | gate | 600 | 324 | yes | 0% |

Gate spacing [7, 2, 1, 4, 2, 4, 11, 1, 4, 1, 5, 5, 3, 310] deg (cv 3.072), width cv 0.328.

### Mission

| # | objective | type | leg from | leg m | time s |
|---|---|---|---|---|---|
| 1 | sabotage_yard | kill_actors | start | 373 | infantry 124.4, vehicle 74.6 |
| 2 | survey_data_obj | capture | start | 444 | infantry 148.0, vehicle 88.8 |
| 3 | reach_zoo | reach_area | survey_data_obj | 440 | infantry 146.7, vehicle 88.0 |
| 4 | broadcast | defend_area | reach_zoo | 0 | infantry 0.0, vehicle 0.0 |
| 5 | escape_west | reach_area | broadcast | 143 | infantry 47.6, vehicle 28.6 |
| 6 | escape_north | reach_area | broadcast | 630 | infantry 210.0, vehicle 126.0 |
| 7 | escape_south | reach_area | broadcast | 195 | infantry 65.2, vehicle 39.1 |

### Resources (path distance from `start`)


Symmetry (share of identical terrain cells): {'mirror_ns': 0.264, 'mirror_ew': 0.205, 'rotate_180': 0.056}

Content per sector (rows north to south): 0 3 0 1 / 1 3 6 1 / 1 0 3 0 / 4 0 1 0
