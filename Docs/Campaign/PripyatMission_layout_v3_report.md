# Pripyat Marshes (sketch v3) (pripyat_01, campaign)

**0 errors, 7 warnings, 10 notes.**

## Warnings
- `unreachable` feature "pilot_bog_a": it cannot be reached on the ground from "start"
- `unreachable` feature "pilot_bog_b": it cannot be reached on the ground from "start"
- `unreachable` feature "island_base": it cannot be reached on the ground from "start"
- `unreachable` feature "island_aa": it cannot be reached on the ground from "start"
- `unreachable` feature "island_vision": it cannot be reached on the ground from "start"
- `unreachable` feature "recon_zone": it cannot be reached on the ground from "start"
- `balance` resources: resource access differs between starts: {'start': 0.83, 'hq_site': 2.27} (distance-weighted)

## Infos
- `mission` objective "deploy_hq": type "reach_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "repair_strip": type "defend_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "recon_island": type "reach_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "opt_pilot_n": type "capture" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "opt_pilot_e": type "capture" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "opt_pilot_s": type "capture" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "opt_pilots_bog": type "capture" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "opt_fuel": type "capture" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "opt_pod": type "reach_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `coverage` features: 3 of 16 map sectors have no content: row 1 (from north), col 4 (from west); row 3 (from north), col 4 (from west); row 4 (from north), col 4 (from west)

## Metrics
- Size 640.0 x 700.0 m, analysis grid 150 cm, passable 20%, cliff bands 0%
- Landscape suggestion: 14x14 components of 63 quads at 100 cm (88200 cm, 9100 cm past the play edge)
- Start `start` on `start_rise`: build area 2153 m2, 100% passable, 100% flat
- Start `hq_site` on `ridge`: build area 14756 m2, 97% passable, 97% flat

### Enemy routes (spawns and bases)

| source | to | route m | straight m | detour | time s | through |
|---|---|---|---|---|---|---|
| spawn_n | start | 658 | 560 | 1.17 | infantry 219.3, vehicle 131.6, column 188.0 | bridge_n |
| spawn_n | hq_site | 314 | 281 | 1.12 | infantry 104.7, vehicle 62.8, column 89.8 | bridge_n |
| spawn_e | start | 770 | 697 | 1.11 | infantry 256.8, vehicle 154.1, column 220.1 | ford_e |
| spawn_e | hq_site | 418 | 371 | 1.13 | infantry 139.4, vehicle 83.6, column 119.5 | ford_e |
| spawn_s | start | 658 | 475 | 1.38 | infantry 219.3, vehicle 131.6, column 188.0 | bridge_s |
| spawn_s | hq_site | 383 | 355 | 1.08 | infantry 127.6, vehicle 76.6, column 109.4 | bridge_s |

### Openings

| id | kind | width cm | bearing | cuts barrier | enemy route share |
|---|---|---|---|---|---|
| bridge_n | bridge | 1000 | 23 | yes | 33% |
| ford_e | ford | 3000 | 67 | yes | 33% |
| bridge_s | bridge | 1000 | 153 | yes | 33% |

### Mission

| # | objective | type | leg from | leg m | time s |
|---|---|---|---|---|---|
| 1 | clear_checkpoint | kill_actors | start | 77 | infantry 25.7, vehicle 15.4, column 22.0 |
| 2 | clear_aa_1 | kill_actors | clear_checkpoint | 34 | infantry 11.3, vehicle 6.8, column 9.7 |
| 3 | clear_aa_2 | kill_actors | clear_aa_1 | 91 | infantry 30.4, vehicle 18.3, column 26.1 |
| 4 | clear_gate | kill_actors | clear_aa_2 | 107 | infantry 35.8, vehicle 21.5, column 30.7 |
| 5 | clear_aa_3 | kill_actors | clear_gate | 250 | infantry 83.4, vehicle 50.1, column 71.5 |
| 6 | clear_aa_4 | kill_actors | clear_aa_3 | 320 | infantry 106.7, vehicle 64.0, column 91.5 |
| 7 | seize_base | destroy_base | clear_aa_4 | 202 | infantry 67.5, vehicle 40.5, column 57.8 |
| 8 | deploy_hq | reach_area | seize_base | 50 | infantry 16.8, vehicle 10.1, column 14.4 |
| 9 | keep_hq | dont_lose_hq | - | - |  |
| 10 | repair_strip | defend_area | deploy_hq | 65 | infantry 21.8, vehicle 13.1, column 18.7 |
| 11 | first_sortie | kill_actors | repair_strip | 329 | infantry 109.6, vehicle 65.8, column 94.0 |
| 12 | hold_centre | survive_waves | - | - |  |
| 13 | recon_island | reach_area | - | - |  |
| 14 | destroy_island | destroy_base | - | - |  |
| 15 | opt_pilot_n | capture | deploy_hq | 203 | infantry 67.8, vehicle 40.7, column 58.1 |
| 16 | opt_pilot_e | capture | deploy_hq | 275 | infantry 91.6, vehicle 54.9, column 78.5 |
| 17 | opt_pilot_s | capture | deploy_hq | 269 | infantry 89.8, vehicle 53.9, column 77.0 |
| 18 | opt_pilots_bog | capture | - | - |  |
| 19 | opt_fuel | capture | deploy_hq | 250 | infantry 83.3, vehicle 50.0, column 71.4 |
| 20 | opt_pod | reach_area | start | 198 | infantry 65.9, vehicle 39.5, column 56.4 |

### Resources (path distance from `start`)

- radixite: 2 far
- metal: 1 far

### Causeway corridors

| id | style | length m | width m | min width m (where) | class | sealable | chokes | crossings | used by |
|---|---|---|---|---|---|---|---|---|---|
| road_west | road | 216 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| rail_north | rail | 272 | 14.0 | 10.0 (n_slump) | all vehicles | yes | n_slump 10.0 m x 32 m | bridge_n (bridge 10.0 m) | spawn_n |
| road_east | road | 304 | 35.0 | 30.0 (ford_e) | all vehicles | no | - | ford_e (ford 30.0 m) | spawn_e |
| causeway_south | logs | 272 | 14.0 | 10.0 (s_logs) | all vehicles | yes | s_logs 10.0 m x 30 m | bridge_s (bridge 10.0 m) | spawn_s |
| dam_far_radixite | track | 148 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| fuel_siding | rail | 102 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| boardwalk_pod | boardwalk | 170 | 4.5 | 4.5 (base) | infantry only | yes | - | - | - |

### Enemy lanes: can wrecks seal them?

| spawn | narrowest on route | width m | class | sealable |
|---|---|---|---|---|
| spawn_n | n_slump | 10.0 | all vehicles | yes |
| spawn_e | road_west | 14.0 | all vehicles | no |
| spawn_s | s_logs | 10.0 | all vehicles | yes |

### Airstrips

| id | length x width m | heading | apron m | region (runway / apron inside) | flat | apron blocked | owner | air only | ground reachable |
|---|---|---|---|---|---|---|---|---|---|
| centre_airstrip | 173 x 20.0 | 89 | 15.0 | ridge (100% / 100%) | 100% | 0% | enemy -> player (seize_base) | no | True |
| island_airfield | 81 x 15.0 | 60 | 8.0 | island (100% / 100%) | 100% | 0% | enemy | yes | False |

### Wreck markers

| id | corridor | choke / crossing | width m | class | seals |
|---|---|---|---|---|---|
| wb_n_slump | rail_north | n_slump | 10.0 | all vehicles | yes |
| wb_n_bridge | rail_north | bridge_n | 10.0 | all vehicles | yes |
| wb_s_logs | causeway_south | s_logs | 10.0 | all vehicles | yes |
| wb_s_bridge | causeway_south | bridge_s | 10.0 | all vehicles | yes |

Lanes with every sealing marker blocked (route m to each start, '-' = cut off):

- spawn_n: sealed (start -, hq_site -)
- spawn_e: OPEN (start 770, hq_site 418)
- spawn_s: sealed (start -, hq_site -)

Symmetry (share of identical terrain cells): {'mirror_ns': 0.286, 'mirror_ew': 0.262, 'rotate_180': 0.289}

Content per sector (rows north to south): 5 3 4 0 / 1 3 5 3 / 5 4 2 0 / 1 1 4 0
