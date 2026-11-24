# Pripyat Marshes (sketch v6) (pripyat_01, campaign)

**0 errors, 7 warnings, 10 notes.**

## Warnings
- `unreachable` feature "pilot_bog_a": it cannot be reached on the ground from "start"
- `unreachable` feature "pilot_bog_b": it cannot be reached on the ground from "start"
- `unreachable` feature "island_base": it cannot be reached on the ground from "start"
- `unreachable` feature "island_aa": it cannot be reached on the ground from "start"
- `unreachable` feature "island_vision": it cannot be reached on the ground from "start"
- `unreachable` feature "recon_zone": it cannot be reached on the ground from "start"
- `balance` resources: resource access differs between starts: {'start': 0.6, 'hq_site': 2.17} (distance-weighted)

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
- Size 800.0 x 875.0 m, analysis grid 150 cm, passable 19%, cliff bands 0%
- Landscape suggestion: 17x17 components of 63 quads at 100 cm (107100 cm, 9800 cm past the play edge)
- Start `start` on `start_rise`: build area 2034 m2, 100% passable, 100% flat
- Start `hq_site` on `ridge`: build area 15230 m2, 100% passable, 100% flat

### Enemy routes (spawns and bases)

| source | to | route m | straight m | detour | time s | through |
|---|---|---|---|---|---|---|
| spawn_n | start | 926 | 700 | 1.32 | infantry 308.8, vehicle 185.3, column 264.7 | bridge_n |
| spawn_n | hq_site | 497 | 351 | 1.41 | infantry 165.7, vehicle 99.4, column 142.0 | bridge_n |
| spawn_e | start | 1045 | 871 | 1.2 | infantry 348.2, vehicle 208.9, column 298.4 | ford_e |
| spawn_e | hq_site | 622 | 463 | 1.34 | infantry 207.4, vehicle 124.4, column 177.8 | ford_e |
| spawn_s | start | 939 | 594 | 1.58 | infantry 313.0, vehicle 187.8, column 268.3 | bridge_s |
| spawn_s | hq_site | 593 | 444 | 1.34 | infantry 197.5, vehicle 118.5, column 169.3 | bridge_s |

### Openings

| id | kind | width cm | bearing | cuts barrier | enemy route share |
|---|---|---|---|---|---|
| bridge_n | bridge | 1000 | 23 | yes | 33% |
| ford_e | ford | 3000 | 67 | yes | 33% |
| bridge_s | bridge | 1000 | 153 | yes | 33% |

### Mission

| # | objective | type | leg from | leg m | time s |
|---|---|---|---|---|---|
| 1 | clear_checkpoint | kill_actors | start | 97 | infantry 32.4, vehicle 19.5, column 27.8 |
| 2 | clear_aa_1 | kill_actors | clear_checkpoint | 18 | infantry 6.0, vehicle 3.6, column 5.1 |
| 3 | clear_aa_2 | kill_actors | clear_aa_1 | 67 | infantry 22.2, vehicle 13.3, column 19.1 |
| 4 | clear_aa_3 | kill_actors | clear_aa_2 | 71 | infantry 23.5, vehicle 14.1, column 20.2 |
| 5 | clear_aa_4 | kill_actors | clear_aa_3 | 34 | infantry 11.3, vehicle 6.8, column 9.7 |
| 6 | clear_gate | kill_actors | clear_aa_4 | 137 | infantry 45.6, vehicle 27.4, column 39.1 |
| 7 | seize_base | destroy_base | clear_gate | 188 | infantry 62.8, vehicle 37.7, column 53.8 |
| 8 | deploy_hq | reach_area | seize_base | 50 | infantry 16.8, vehicle 10.1, column 14.4 |
| 9 | keep_hq | dont_lose_hq | - | - |  |
| 10 | repair_strip | defend_area | deploy_hq | 82 | infantry 27.3, vehicle 16.4, column 23.4 |
| 11 | first_sortie | kill_actors | repair_strip | 454 | infantry 151.3, vehicle 90.8, column 129.7 |
| 12 | hold_centre | survive_waves | - | - |  |
| 13 | recon_island | reach_area | - | - |  |
| 14 | destroy_island | destroy_base | - | - |  |
| 15 | opt_pilot_n | capture | deploy_hq | 363 | infantry 120.9, vehicle 72.6, column 103.7 |
| 16 | opt_pilot_e | capture | deploy_hq | 421 | infantry 140.2, vehicle 84.1, column 120.2 |
| 17 | opt_pilot_s | capture | deploy_hq | 461 | infantry 153.5, vehicle 92.1, column 131.6 |
| 18 | opt_pilots_bog | capture | - | - |  |
| 19 | opt_fuel | capture | deploy_hq | 441 | infantry 147.1, vehicle 88.2, column 126.1 |
| 20 | opt_pod | reach_area | start | 247 | infantry 82.3, vehicle 49.4, column 70.6 |

### Resources (path distance from `start`)

- radixite: 2 far
- metal: 1 far

### Causeway corridors

| id | style | length m | width m | min width m (where) | class | sealable | chokes | crossings | used by |
|---|---|---|---|---|---|---|---|---|---|
| road_west | road | 317 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| rail_north | rail | 480 | 14.0 | 10.0 (n_slump) | all vehicles | yes | n_slump 10.0 m x 32 m | bridge_n (bridge 10.0 m) | spawn_n |
| road_east | road | 451 | 35.0 | 30.0 (ford_e) | all vehicles | no | - | ford_e (ford 30.0 m) | spawn_e |
| causeway_south | logs | 486 | 14.0 | 10.0 (s_logs) | all vehicles | yes | s_logs 10.0 m x 30 m | bridge_s (bridge 10.0 m) | spawn_s |
| dam_far_radixite | track | 184 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| fuel_siding | rail | 151 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| boardwalk_pod | boardwalk | 212 | 4.5 | 4.5 (base) | infantry only | yes | - | - | - |

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
- spawn_e: OPEN (start 1045, hq_site 622)
- spawn_s: sealed (start -, hq_site -)

### Elevation (m above base)

Base 0 m; ground from 0.0 to 10.0 m. Contours: c_ridge_foot 0 m, c_bench_out 5 m (in c_ridge_foot), c_bench_in 5 m (in c_bench_out), c_ridge_top 10 m (in c_bench_in), c_island 3 m, c_start 2 m

| corridor | start m | end m | climb m | mean grade % | max grade % (10 m window) | at (m) |
|---|---|---|---|---|---|---|
| road_west | 2.0 | 10.0 | 10.0 | 3.8 | 9.2 | (-41, -214) |
| rail_north | 0.0 | 10.0 | 10.0 | 2.1 | 10.7 | (132, -57) |
| road_east | 0.0 | 10.0 | 10.0 | 2.2 | 8.0 | (-43, 205) |
| causeway_south | 0.0 | 10.0 | 10.0 | 2.1 | 10.2 | (-137, -45) |
| dam_far_radixite | 0.2 | 0.0 | 0.2 | 0.1 | 2.3 | (-208, 98) |
| fuel_siding | 0.2 | 0.0 | 0.3 | 0.3 | 1.9 | (212, 53) |
| boardwalk_pod | 2.0 | 0.0 | 2.0 | 0.9 | 6.6 | (-169, -325) |

| airstrip | end heights m | end-to-end grade % | max slope under runway % |
|---|---|---|---|
| centre_airstrip | 10.0 / 10.0 | 0.0 | 0.0 |
| island_airfield | 3.0 / 3.0 | 0.0 | 0.0 |

Height at each object (m): start 2.0; hq_site 10.0; command_post 10.0; garrison 10.0; west_gate_bunkers 10.0; radixite_base 10.0; metal_base 10.0; hangars 10.0; spotter_post 10.0; pit_n 10.0; pit_e 10.0; pit_s 10.0; flight_lbl 0.0; checkpoint 0.0; spawn_n 0.0; spawn_e 0.0; spawn_s 0.0; first_column 0.0; lbl_n 0.0; lbl_e 0.0; lbl_s 0.0; pilot_n 0.0; pilot_e 0.0; pilot_s 0.0; pilot_bog_a 0.0; pilot_bog_b 0.0; demeter_pod 0.0; fuel_depot 0.0; fuel_guard 0.0; radixite_far 0.0; island_base 3.0; island_aa 3.0; island_vision 3.0; recon_zone 3.0; dz_checkpoint 0.0; dz_west_gate 7.1; spot_n 0.0; spot_e 0.0; spot_s 0.0; base_area 10.0; centre_airstrip 10.0; island_airfield 3.0; wb_n_slump 0.0; wb_n_bridge 0.0; wb_s_logs 0.0; wb_s_bridge 0.0; aa_w1 0.3; aa_w2 0.0; aa_w3 5.0; aa_w4 5.0; bridge_n 0.0; ford_e 0.0; bridge_s 0.0

Symmetry (share of identical terrain cells): {'mirror_ns': 0.307, 'mirror_ew': 0.305, 'rotate_180': 0.298}

Content per sector (rows north to south): 5 3 4 0 / 1 3 4 3 / 7 4 1 0 / 1 1 4 0
