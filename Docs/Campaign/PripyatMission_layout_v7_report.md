# Pripyat Marshes (sketch v7) (pripyat_01, campaign)

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
- `coverage` features: 2 of 16 map sectors have no content: row 3 (from north), col 4 (from west); row 4 (from north), col 4 (from west)

## Metrics
- Size 800.0 x 875.0 m, analysis grid 150 cm, passable 22%, cliff bands 0%
- Landscape suggestion: 17x17 components of 63 quads at 100 cm (107100 cm, 9800 cm past the play edge)
- Start `start` on `start_rise`: build area 2034 m2, 100% passable, 100% flat
- Start `hq_site` on `ridge`: build area 15230 m2, 100% passable, 100% flat

### Enemy routes (spawns and bases)

| source | to | route m | straight m | detour | time s | through |
|---|---|---|---|---|---|---|
| spawn_n | start | 858 | 716 | 1.2 | infantry 286.1, vehicle 171.7, column 245.3 | bridge_n |
| spawn_n | hq_site | 523 | 377 | 1.39 | infantry 174.2, vehicle 104.5, column 149.4 | bridge_n |
| spawn_e | start | 1126 | 926 | 1.22 | infantry 375.4, vehicle 225.2, column 321.7 | ford_e |
| spawn_e | hq_site | 698 | 518 | 1.35 | infantry 232.6, vehicle 139.6, column 199.4 | ford_e |
| spawn_s | start | 860 | 611 | 1.41 | infantry 286.6, vehicle 171.9, column 245.6 | bridge_s |
| spawn_s | hq_site | 618 | 473 | 1.31 | infantry 205.9, vehicle 123.5, column 176.5 | bridge_s |

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
| 2 | clear_aa_1 | kill_actors | clear_checkpoint | 185 | infantry 61.8, vehicle 37.1, column 53.0 |
| 3 | clear_aa_2 | kill_actors | clear_aa_1 | 312 | infantry 104.0, vehicle 62.4, column 89.2 |
| 4 | clear_aa_3 | kill_actors | clear_aa_2 | 249 | infantry 83.1, vehicle 49.9, column 71.2 |
| 5 | clear_aa_4 | kill_actors | clear_aa_3 | 412 | infantry 137.3, vehicle 82.4, column 117.7 |
| 6 | clear_gate | kill_actors | clear_aa_4 | 422 | infantry 140.6, vehicle 84.3, column 120.5 |
| 7 | seize_base | destroy_base | clear_gate | 188 | infantry 62.8, vehicle 37.7, column 53.8 |
| 8 | deploy_hq | reach_area | seize_base | 50 | infantry 16.8, vehicle 10.1, column 14.4 |
| 9 | keep_hq | dont_lose_hq | - | - |  |
| 10 | repair_strip | defend_area | deploy_hq | 82 | infantry 27.3, vehicle 16.4, column 23.4 |
| 11 | first_sortie | kill_actors | repair_strip | 680 | infantry 226.7, vehicle 136.0, column 194.3 |
| 12 | hold_centre | survive_waves | - | - |  |
| 13 | recon_island | reach_area | - | - |  |
| 14 | destroy_island | destroy_base | - | - |  |
| 15 | opt_pilot_n | capture | deploy_hq | 356 | infantry 118.8, vehicle 71.3, column 101.8 |
| 16 | opt_pilot_e | capture | deploy_hq | 419 | infantry 139.8, vehicle 83.9, column 119.8 |
| 17 | opt_pilot_s | capture | deploy_hq | 453 | infantry 151.1, vehicle 90.6, column 129.5 |
| 18 | opt_pilots_bog | capture | - | - |  |
| 19 | opt_fuel | capture | deploy_hq | 435 | infantry 144.9, vehicle 86.9, column 124.2 |
| 20 | opt_pod | reach_area | start | 247 | infantry 82.3, vehicle 49.4, column 70.6 |

### Resources (path distance from `start`)

- radixite: 2 far
- metal: 1 far

### Causeway corridors

| id | style | length m | width m | min width m (where) | class | sealable | chokes | crossings | used by |
|---|---|---|---|---|---|---|---|---|---|
| ring_road | road | 1246 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| road_west | road | 317 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| rail_north | rail | 538 | 14.0 | 10.0 (n_slump) | all vehicles | yes | n_slump 10.0 m x 32 m | bridge_n (bridge 10.0 m) | spawn_n |
| road_east | road | 549 | 35.0 | 30.0 (ford_e) | all vehicles | no | - | ford_e (ford 30.0 m) | spawn_e |
| causeway_south | logs | 543 | 14.0 | 10.0 (s_logs) | all vehicles | yes | s_logs 10.0 m x 30 m | bridge_s (bridge 10.0 m) | spawn_s |
| dam_far_radixite | track | 184 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| fuel_siding | rail | 151 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| boardwalk_pod | boardwalk | 212 | 4.5 | 4.5 (base) | infantry only | yes | - | - | - |

### Enemy lanes: can wrecks seal them?

| spawn | narrowest on route | width m | class | sealable |
|---|---|---|---|---|
| spawn_n | n_slump | 10.0 | all vehicles | yes |
| spawn_e | ring_road | 14.0 | all vehicles | no |
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
- spawn_e: OPEN (start 1126, hq_site 698)
- spawn_s: sealed (start -, hq_site -)

### Elevation (m above base)

Base 0 m; ground from 0.0 to 10.0 m. Contours: c_ridge_foot 0 m, c_bench_out 5 m (in c_ridge_foot), c_bench_in 5 m (in c_bench_out), c_ridge_top 10 m (in c_bench_in), c_island 3 m, c_start 2 m

| corridor | start m | end m | climb m | mean grade % | max grade % (10 m window) | at (m) |
|---|---|---|---|---|---|---|
| ring_road | 5.0 | 5.0 | 0.0 | 0.0 | 0.0 | (106, 150) |
| road_west | 2.0 | 10.0 | 10.0 | 3.8 | 9.2 | (-41, -214) |
| rail_north | 0.0 | 10.0 | 10.0 | 1.9 | 10.7 | (131, -56) |
| road_east | 0.0 | 10.0 | 10.0 | 1.8 | 8.0 | (-43, 205) |
| causeway_south | 0.0 | 10.0 | 10.0 | 1.8 | 10.2 | (-137, -45) |
| dam_far_radixite | 0.2 | 0.0 | 0.2 | 0.1 | 2.3 | (-208, 98) |
| fuel_siding | 0.2 | 0.0 | 0.3 | 0.3 | 1.9 | (212, 53) |
| boardwalk_pod | 2.0 | 0.0 | 2.0 | 0.9 | 6.6 | (-169, -325) |

| airstrip | end heights m | end-to-end grade % | max slope under runway % |
|---|---|---|---|
| centre_airstrip | 10.0 / 10.0 | 0.0 | 0.0 |
| island_airfield | 3.0 / 3.0 | 0.0 | 0.0 |

Height at each object (m): start 2.0; hq_site 10.0; command_post 10.0; garrison 10.0; west_gate_bunkers 10.0; radixite_base 10.0; metal_base 10.0; hangars 10.0; spotter_post 10.0; pit_n 10.0; pit_e 10.0; pit_s 10.0; flight_lbl 0.0; checkpoint 0.0; spawn_n 0.0; spawn_e 0.0; spawn_s 0.0; first_column 0.0; lbl_n 0.0; lbl_e 0.0; lbl_s 0.0; pilot_n 0.0; pilot_e 0.0; pilot_s 0.0; pilot_bog_a 0.0; pilot_bog_b 0.0; demeter_pod 0.0; fuel_depot 0.0; fuel_guard 0.0; radixite_far 0.0; island_base 3.0; island_aa 3.0; island_vision 3.0; recon_zone 3.0; dz_checkpoint 0.0; dz_west_gate 7.1; spot_n 0.0; spot_e 0.0; spot_s 0.0; base_area 10.0; centre_airstrip 10.0; island_airfield 3.0; wb_n_slump 0.0; wb_n_bridge 0.0; wb_s_logs 0.0; wb_s_bridge 0.0; entry_n 0.0; entry_e 0.0; entry_s 0.0; aa_w 5.0; aa_n 5.0; aa_e 5.0; aa_s 5.0; bridge_n 0.0; ford_e 0.0; bridge_s 0.0

Symmetry (share of identical terrain cells): {'mirror_ns': 0.293, 'mirror_ew': 0.342, 'rotate_180': 0.296}

Content per sector (rows north to south): 5 3 4 2 / 1 4 5 1 / 4 4 2 0 / 1 1 4 0
