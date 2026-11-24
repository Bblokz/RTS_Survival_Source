# Pripyat Marshes (sketch v9) (pripyat_01, campaign)

**0 errors, 7 warnings, 10 notes.**

## Warnings
- `unreachable` feature "pilot_bog_a": it cannot be reached on the ground from "start"
- `unreachable` feature "pilot_bog_b": it cannot be reached on the ground from "start"
- `unreachable` feature "island_base": it cannot be reached on the ground from "start"
- `unreachable` feature "island_aa": it cannot be reached on the ground from "start"
- `unreachable` feature "island_vision": it cannot be reached on the ground from "start"
- `unreachable` feature "recon_zone": it cannot be reached on the ground from "start"
- `balance` resources: resource access differs between starts: {'start': 0.57, 'hq_site': 2.19} (distance-weighted)

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
- `coverage` features: 3 of 16 map sectors have no content: row 1 (from north), col 4 (from west); row 3 (from north), col 4 (from west); row 4 (from north), col 2 (from west)

## Metrics
- Size 670.0 x 700.0 m, analysis grid 150 cm, passable 23%, cliff bands 0%
- Landscape suggestion: 14x14 components of 63 quads at 100 cm (88200 cm, 9100 cm past the play edge)
- Start `start` on `start_rise`: build area 2153 m2, 100% passable, 100% flat
- Start `hq_site` on `ridge`: build area 15230 m2, 100% passable, 100% flat

### Enemy routes (spawns and bases)

| source | to | route m | straight m | detour | time s | through |
|---|---|---|---|---|---|---|
| spawn_n | start | 807 | 595 | 1.36 | infantry 269.0, vehicle 161.4, column 230.6 | bridge_n |
| spawn_n | hq_site | 423 | 322 | 1.31 | infantry 141.1, vehicle 84.6, column 120.9 | bridge_n |
| spawn_e | start | 789 | 682 | 1.16 | infantry 263.0, vehicle 157.8, column 225.4 | ford_e |
| spawn_e | hq_site | 446 | 349 | 1.28 | infantry 148.7, vehicle 89.2, column 127.5 | ford_e |
| spawn_s | start | 739 | 449 | 1.65 | infantry 246.5, vehicle 147.9, column 211.2 | bridge_s |
| spawn_s | hq_site | 463 | 361 | 1.28 | infantry 154.5, vehicle 92.7, column 132.4 | bridge_s |

### Openings

| id | kind | width cm | bearing | cuts barrier | enemy route share |
|---|---|---|---|---|---|
| bridge_n | bridge | 1000 | 17 | yes | 33% |
| ford_e | ford | 3000 | 80 | yes | 33% |
| bridge_s | bridge | 1000 | 164 | yes | 33% |

### Mission

| # | objective | type | leg from | leg m | time s |
|---|---|---|---|---|---|
| 1 | clear_checkpoint | kill_actors | start | 71 | infantry 23.7, vehicle 14.2, column 20.3 |
| 2 | clear_aa_1 | kill_actors | clear_checkpoint | 224 | infantry 74.8, vehicle 44.9, column 64.1 |
| 3 | clear_aa_2 | kill_actors | clear_aa_1 | 236 | infantry 78.7, vehicle 47.2, column 67.4 |
| 4 | clear_aa_3 | kill_actors | clear_aa_2 | 236 | infantry 78.7, vehicle 47.2, column 67.4 |
| 5 | clear_aa_4 | kill_actors | clear_aa_3 | 274 | infantry 91.4, vehicle 54.9, column 78.4 |
| 6 | clear_gate | kill_actors | clear_aa_4 | 179 | infantry 59.6, vehicle 35.8, column 51.1 |
| 7 | seize_base | destroy_base | clear_gate | 141 | infantry 47.1, vehicle 28.3, column 40.4 |
| 8 | deploy_hq | reach_area | seize_base | 18 | infantry 6.1, vehicle 3.7, column 5.2 |
| 9 | keep_hq | dont_lose_hq | - | - |  |
| 10 | repair_strip | defend_area | deploy_hq | 39 | infantry 13.0, vehicle 7.8, column 11.1 |
| 11 | first_sortie | kill_actors | repair_strip | 430 | infantry 143.3, vehicle 86.0, column 122.8 |
| 12 | hold_centre | survive_waves | - | - |  |
| 13 | recon_island | reach_area | - | - |  |
| 14 | destroy_island | destroy_base | - | - |  |
| 15 | opt_pilot_n | capture | deploy_hq | 388 | infantry 129.4, vehicle 77.7, column 110.9 |
| 16 | opt_pilot_e | capture | deploy_hq | 357 | infantry 119.0, vehicle 71.4, column 102.0 |
| 17 | opt_pilot_s | capture | deploy_hq | 434 | infantry 144.6, vehicle 86.8, column 123.9 |
| 18 | opt_pilots_bog | capture | - | - |  |
| 19 | opt_fuel | capture | deploy_hq | 492 | infantry 164.2, vehicle 98.5, column 140.7 |
| 20 | opt_pod | reach_area | start | 165 | infantry 55.0, vehicle 33.0, column 47.2 |

### Resources (path distance from `start`)

- radixite: 2 far
- metal: 1 far

### Causeway corridors

| id | style | length m | width m | min width m (where) | class | sealable | chokes | crossings | used by |
|---|---|---|---|---|---|---|---|---|---|
| ring_road | road | 905 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| road_west | road | 252 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| rail_north | rail | 333 | 14.0 | 10.0 (n_slump) | all vehicles | yes | n_slump 10.0 m x 32 m | bridge_n (bridge 10.0 m) | spawn_n |
| road_east | road | 348 | 35.0 | 30.0 (ford_e) | all vehicles | no | - | ford_e (ford 30.0 m) | spawn_e |
| causeway_south | logs | 334 | 14.0 | 10.0 (s_logs) | all vehicles | yes | s_logs 10.0 m x 30 m | bridge_s (bridge 10.0 m) | spawn_s |
| ramp_n | road | 89 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_n |
| ramp_e | road | 88 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e |
| ramp_s | road | 87 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_s |
| ramp_w | road | 81 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| dam_far_radixite | track | 91 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| fuel_siding | rail | 116 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| boardwalk_pod | boardwalk | 155 | 4.5 | 4.5 (base) | infantry only | yes | - | - | - |

### Enemy lanes: can wrecks seal them?

| spawn | narrowest on route | width m | class | sealable |
|---|---|---|---|---|
| spawn_n | n_slump | 10.0 | all vehicles | yes |
| spawn_e | ring_road | 14.0 | all vehicles | no |
| spawn_s | s_logs | 10.0 | all vehicles | yes |

### Airstrips

| id | length x width m | heading | apron m | region (runway / apron inside) | flat | apron blocked | owner | air only | ground reachable |
|---|---|---|---|---|---|---|---|---|---|
| centre_airstrip | 173 x 20.0 | 90 | 15.0 | ridge (100% / 100%) | 100% | 0% | enemy -> player (seize_base) | no | True |
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
- spawn_e: OPEN (start 789, hq_site 446)
- spawn_s: sealed (start -, hq_site -)

### Elevation (m above base)

Base 0 m; ground from 0.0 to 10.0 m. Contours: c_ridge_foot 0 m, c_bench_out 5 m (in c_ridge_foot), c_bench_in 5 m (in c_bench_out), c_ridge_top 10 m (in c_bench_in), c_island 3 m, c_start 2 m

| corridor | start m | end m | climb m | mean grade % | max grade % (10 m window) | at (m) |
|---|---|---|---|---|---|---|
| ring_road | 5.0 | 5.0 | 0.0 | 0.0 | 0.0 | (118, 85) |
| road_west | 2.0 | 5.0 | 5.0 | 2.8 | 14.7 | (-54, -179) |
| rail_north | 0.0 | 5.0 | 5.0 | 1.5 | 13.8 | (191, 34) |
| road_east | 0.0 | 5.0 | 5.0 | 1.5 | 9.3 | (-37, 197) |
| causeway_south | 0.0 | 5.0 | 5.0 | 1.5 | 13.0 | (-191, 33) |
| ramp_n | 5.0 | 10.0 | 5.0 | 5.6 | 13.6 | (128, 37) |
| ramp_e | 5.0 | 10.0 | 5.0 | 5.7 | 14.0 | (-72, 108) |
| ramp_s | 5.0 | 10.0 | 5.0 | 5.8 | 14.0 | (-126, 35) |
| ramp_w | 5.0 | 10.0 | 5.0 | 6.2 | 14.6 | (-46, -125) |
| dam_far_radixite | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | (-300, 84) |
| fuel_siding | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | (285, 69) |
| boardwalk_pod | 2.0 | 0.0 | 2.0 | 1.3 | 7.6 | (-157, -279) |

| airstrip | end heights m | end-to-end grade % | max slope under runway % |
|---|---|---|---|
| centre_airstrip | 10.0 / 10.0 | 0.0 | 0.0 |
| island_airfield | 3.0 / 3.0 | 0.0 | 0.0 |

Height at each object (m): start 2.0; hq_site 10.0; command_post 10.0; garrison 10.0; west_gate_bunkers 10.0; radixite_base 10.0; metal_base 10.0; hangars 10.0; spotter_post 10.0; pit_n 10.0; pit_e 10.0; pit_s 10.0; flight_lbl 5.0; checkpoint 0.0; spawn_n 0.0; spawn_e 0.0; spawn_s 0.0; first_column 0.0; lbl_n 0.0; lbl_e 0.0; lbl_s 0.0; pilot_n 0.0; pilot_e 0.0; pilot_s 0.0; pilot_bog_a 0.0; pilot_bog_b 0.0; demeter_pod 0.0; fuel_depot 0.0; fuel_guard 0.0; radixite_far 0.0; island_base 3.0; island_aa 3.0; island_vision 3.0; recon_zone 3.0; dz_checkpoint 0.0; dz_west_gate 4.6; spot_n 0.0; spot_e 0.0; spot_s 0.0; base_area 10.0; centre_airstrip 10.0; island_airfield 3.0; wb_n_slump 0.0; wb_n_bridge 0.0; wb_s_logs 0.0; wb_s_bridge 0.0; entry_n 0.0; entry_e 0.0; entry_s 0.0; aa_w 5.0; aa_n 5.0; aa_e 5.0; aa_s 5.0; bridge_n 0.0; ford_e 0.0; bridge_s 0.0

Symmetry (share of identical terrain cells): {'mirror_ns': 0.455, 'mirror_ew': 0.408, 'rotate_180': 0.378}

Content per sector (rows north to south): 5 3 4 0 / 1 3 7 3 / 4 2 3 0 / 1 0 4 1
