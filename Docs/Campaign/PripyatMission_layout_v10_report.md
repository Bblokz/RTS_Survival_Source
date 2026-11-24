# Pripyat Marshes (v10, final) (pripyat_01, campaign)

**0 errors, 7 warnings, 10 notes.**

## Warnings
- `unreachable` feature "pilot_bog_a": it cannot be reached on the ground from "start"
- `unreachable` feature "pilot_bog_b": it cannot be reached on the ground from "start"
- `unreachable` feature "island_base": it cannot be reached on the ground from "start"
- `unreachable` feature "island_aa": it cannot be reached on the ground from "start"
- `unreachable` feature "island_vision": it cannot be reached on the ground from "start"
- `unreachable` feature "recon_zone": it cannot be reached on the ground from "start"
- `balance` resources: resource access differs between starts: {'start': 0.48, 'hq_site': 2.18} (distance-weighted)

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
- `coverage` features: 3 of 16 map sectors have no content: row 1 (from north), col 4 (from west); row 2 (from north), col 3 (from west); row 3 (from north), col 4 (from west)

## Metrics
- Size 670.0 x 700.0 m, analysis grid 150 cm, passable 25%, cliff bands 0%
- Landscape suggestion: 14x14 components of 63 quads at 100 cm (88200 cm, 9100 cm past the play edge)
- Start `start` on `start_rise`: build area 2153 m2, 100% passable, 100% flat
- Start `hq_site` on `ridge`: build area 15230 m2, 100% passable, 100% flat

### Enemy routes (spawns and bases)

| source | to | route m | straight m | detour | time s | through |
|---|---|---|---|---|---|---|
| spawn_n | start | 836 | 595 | 1.41 | infantry 278.7, vehicle 167.2, column 238.9 | bridge_n |
| spawn_n | hq_site | 521 | 355 | 1.47 | infantry 173.6, vehicle 104.2, column 148.8 | bridge_n |
| spawn_e | start | 848 | 682 | 1.24 | infantry 282.6, vehicle 169.6, column 242.3 | ford_e |
| spawn_e | hq_site | 556 | 335 | 1.66 | infantry 185.4, vehicle 111.3, column 158.9 | ford_e |
| spawn_s | start | 745 | 449 | 1.66 | infantry 248.2, vehicle 148.9, column 212.8 | bridge_s |
| spawn_s | hq_site | 467 | 321 | 1.45 | infantry 155.6, vehicle 93.3, column 133.3 | bridge_s |

### Openings

| id | kind | width cm | bearing | cuts barrier | enemy route share |
|---|---|---|---|---|---|
| bridge_n | bridge | 1000 | 16 | yes | 33% |
| ford_e | ford | 3000 | 80 | yes | 33% |
| bridge_s | bridge | 1000 | 164 | yes | 33% |

### Mission

| # | objective | type | leg from | leg m | time s |
|---|---|---|---|---|---|
| 1 | clear_checkpoint | kill_actors | start | 71 | infantry 23.7, vehicle 14.2, column 20.3 |
| 2 | clear_aa_1 | kill_actors | clear_checkpoint | 201 | infantry 67.0, vehicle 40.2, column 57.5 |
| 3 | clear_aa_2 | kill_actors | clear_aa_1 | 280 | infantry 93.5, vehicle 56.1, column 80.1 |
| 4 | clear_aa_3 | kill_actors | clear_aa_2 | 267 | infantry 88.8, vehicle 53.3, column 76.2 |
| 5 | clear_aa_4 | kill_actors | clear_aa_3 | 247 | infantry 82.3, vehicle 49.4, column 70.6 |
| 6 | clear_gate | kill_actors | clear_aa_4 | 310 | infantry 103.5, vehicle 62.1, column 88.7 |
| 7 | seize_base | destroy_base | clear_gate | 174 | infantry 57.9, vehicle 34.7, column 49.6 |
| 8 | deploy_hq | reach_area | seize_base | 30 | infantry 10.0, vehicle 6.0, column 8.6 |
| 9 | keep_hq | dont_lose_hq | - | - |  |
| 10 | repair_strip | defend_area | deploy_hq | 40 | infantry 13.4, vehicle 8.1, column 11.5 |
| 11 | first_sortie | kill_actors | repair_strip | 574 | infantry 191.4, vehicle 114.8, column 164.1 |
| 12 | hold_centre | survive_waves | - | - |  |
| 13 | recon_island | reach_area | - | - |  |
| 14 | destroy_island | destroy_base | - | - |  |
| 15 | opt_pilot_n | capture | deploy_hq | 380 | infantry 126.8, vehicle 76.1, column 108.7 |
| 16 | opt_pilot_e | capture | deploy_hq | 470 | infantry 156.7, vehicle 94.0, column 134.3 |
| 17 | opt_pilot_s | capture | deploy_hq | 325 | infantry 108.3, vehicle 65.0, column 92.9 |
| 18 | opt_pilots_bog | capture | - | - |  |
| 19 | opt_fuel | capture | deploy_hq | 604 | infantry 201.4, vehicle 120.8, column 172.6 |
| 20 | opt_pod | reach_area | start | 165 | infantry 55.0, vehicle 33.0, column 47.2 |

### Resources (path distance from `start`)

- radixite: 2 far
- metal: 1 far

### Causeway corridors

| id | style | length m | width m | min width m (where) | class | sealable | chokes | crossings | used by |
|---|---|---|---|---|---|---|---|---|---|
| ring_road | road | 962 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| road_west | road | 247 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_n, spawn_s |
| rail_north | rail | 337 | 14.0 | 10.0 (n_slump) | all vehicles | yes | n_slump 10.0 m x 32 m | bridge_n (bridge 10.0 m) | spawn_n |
| road_east | road | 350 | 35.0 | 30.0 (ford_e) | all vehicles | no | - | ford_e (ford 30.0 m) | spawn_e |
| causeway_south | logs | 338 | 14.0 | 10.0 (s_logs) | all vehicles | yes | s_logs 10.0 m x 30 m | bridge_s (bridge 10.0 m) | spawn_s |
| ramp_n | road | 180 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_n |
| ramp_s | road | 177 | 14.0 | 14.0 (base) | all vehicles | no | - | - | spawn_e, spawn_s |
| ramp_w | road | 179 | 14.0 | 14.0 (base) | all vehicles | no | - | - | - |
| dam_far_radixite | track | 91 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
| fuel_siding | rail | 119 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |
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
| centre_airstrip | 173 x 20.0 | 45 | 12.0 | ridge (100% / 100%) | 100% | 0% | enemy -> player (seize_base) | no | True |
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
- spawn_e: OPEN (start 848, hq_site 556)
- spawn_s: sealed (start -, hq_site -)

### Elevation (m above base)

Base 0 m; ground from 0.0 to 10.0 m. Contours: c_ridge_foot 0 m, c_bench_out 5 m (in c_ridge_foot), c_bench_in 5 m (in c_bench_out), c_ridge_top 10 m (in c_bench_in), c_island 3 m, c_start 2 m

| corridor | start m | end m | climb m | mean grade % | max grade % (10 m window) | at (m) |
|---|---|---|---|---|---|---|
| ring_road | 5.0 | 5.0 | 0.0 | 0.0 | 0.0 | (-83, 128) |
| road_west | 2.0 | 5.0 | 5.0 | 2.8 | 13.1 | (-30, -204) |
| rail_north | 0.0 | 5.0 | 5.0 | 1.5 | 13.9 | (199, 34) |
| road_east | 0.0 | 5.0 | 5.0 | 1.4 | 8.5 | (-42, 203) |
| causeway_south | 0.0 | 5.0 | 5.0 | 1.5 | 13.1 | (-199, 33) |
| ramp_n | 5.0 | 10.0 | 5.0 | 2.8 | 5.9 | (34, 119) |
| ramp_s | 5.0 | 10.0 | 5.0 | 3.0 | 5.9 | (-120, -34) |
| ramp_w | 5.0 | 10.0 | 5.0 | 2.8 | 6.7 | (69, -103) |
| dam_far_radixite | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | (-300, 84) |
| fuel_siding | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | (300, 69) |
| boardwalk_pod | 2.0 | 0.0 | 2.0 | 1.3 | 7.6 | (-157, -279) |

| airstrip | end heights m | end-to-end grade % | max slope under runway % |
|---|---|---|---|
| centre_airstrip | 10.0 / 10.0 | 0.0 | 0.0 |
| island_airfield | 3.0 / 3.0 | 0.0 | 0.0 |

Height at each object (m): start 2.0; hq_site 10.0; command_post 10.0; garrison 10.0; west_gate_bunkers 10.0; radixite_base 10.0; metal_base 10.0; hangars 10.0; spotter_post 10.0; pit_n 10.0; pit_e 10.0; pit_s 10.0; flight_lbl 5.0; checkpoint 0.0; spawn_n 0.0; spawn_e 0.0; spawn_s 0.0; first_column 0.0; lbl_n 0.0; lbl_e 0.0; lbl_s 0.0; pilot_n 0.0; pilot_e 0.0; pilot_s 0.0; pilot_bog_a 0.0; pilot_bog_b 0.0; demeter_pod 0.0; fuel_depot 0.0; fuel_guard 0.0; radixite_far 0.0; island_base 3.0; island_aa 3.0; island_vision 3.0; recon_zone 3.0; dz_checkpoint 0.0; dz_west_gate 5.0; spot_n 0.0; spot_e 0.0; spot_s 0.0; base_area 10.0; centre_airstrip 10.0; island_airfield 3.0; wb_n_slump 0.0; wb_n_bridge 0.0; wb_s_logs 0.0; wb_s_bridge 0.0; entry_n 0.0; entry_e 0.0; entry_s 0.0; aa_w 5.0; aa_n 5.0; aa_e 5.0; aa_s 5.0; bridge_n 0.0; ford_e 0.0; bridge_s 0.0

Symmetry (share of identical terrain cells): {'mirror_ns': 0.482, 'mirror_ew': 0.416, 'rotate_180': 0.393}

Content per sector (rows north to south): 5 4 4 0 / 1 2 0 3 / 3 4 8 0 / 1 1 4 1
