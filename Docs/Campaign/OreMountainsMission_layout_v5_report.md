# Ore Mountains raid (sketch v5) (ore_01, campaign)

**0 errors, 1 warnings, 6 notes.**

## Warnings
- `opening_load` feature "ramp_main": 75% of enemy routes go through it; the other openings see little use

## Infos
- `slope` feature "ramp_sally": max grade 23% over 10 m, 12 m wide: not usable by heavy vehicles
- `opening_load` feature "cp_gate": no shortest enemy route uses it
- `mission` objective "recover_cargo": type "capture" has no mission class yet (needs a new UMissionBase child or a BP)
- `mission` objective "return_cargo": type "reach_area" has no mission class yet (needs a new UMissionBase child or a BP)
- `coverage` features: 5 of 16 map sectors have no content: row 1 (from north), col 1 (from west); row 1 (from north), col 2 (from west); row 3 (from north), col 1 (from west); row 4 (from north), col 1 (from west); row 4 (from north), col 4 (from west)
- `corridor` path "ammo_route": crosses no impassable ground; a plain road would do

## Metrics
- Size 850.0 x 850.0 m, analysis grid 200 cm, passable 85%, cliff bands 0%
- Landscape suggestion: 17x17 components of 63 quads at 100 cm (107100 cm, 11050 cm past the play edge)
- Start `hq` on `ridge_bench`: build area 22888 m2, 99% passable, 100% flat

### Slope passability (main analysis: heavy vehicles)

| unit class | max grade % | min width cm | speed cm/s | open ground too steep |
|---|---|---|---|---|
| infantry | 60 | 400 | 300 | 3% |
| small vehicles | 40 | 600 | 600 | 5% |
| medium vehicles | 30 | 800 | 500 | 5% |
| heavy vehicles | 20 | 1000 | 400 | 6% |

### Enemy routes (spawns and bases)

| source | to | route m | straight m | detour | time s | through |
|---|---|---|---|---|---|---|
| convoy_entry | hq | 412 | 375 | 1.1 | infantry 137.2, vehicle 82.3, convoy 117.6 | ramp_east |
| base_a | hq | 396 | 373 | 1.06 | infantry 135.4, vehicle 81.2, convoy 116.0 | ramp_main |
| base_b | hq | 825 | 492 | 1.68 | infantry 273.2, vehicle 163.9, convoy 234.2 | knoll_ramp_ne > ramp_main |
| artillery_battery | hq | 636 | 541 | 1.18 | infantry 212.0, vehicle 127.2, convoy 181.7 | ramp_main |

### Routes per unit class (route m / time s at the class speed; - = no route)

| source | to | infantry | small vehicles | medium vehicles | heavy vehicles |
|---|---|---|---|---|---|
| convoy_entry | hq | 407 m / 135.7 s | 410 m / 68.4 s | 410 m / 82.1 s | 412 m / 102.9 s |
| base_a | hq | 388 m / 132.7 s | 392 m / 67.1 s | 394 m / 80.8 s | 396 m / 101.5 s |
| base_b | hq | 665 m / 222.7 s | 672 m / 112.5 s | 703 m / 141.2 s | 825 m / 204.9 s |
| artillery_battery | hq | 621 m / 207.1 s | 631 m / 105.1 s | 632 m / 126.4 s | 636 m / 159.0 s |

Not reachable from `hq` per unit class: infantry: none; small vehicles: none; medium vehicles: none; heavy vehicles: none

### Openings

| id | kind | width cm | bearing | cuts barrier | enemy route share | climb m | max grade % (10 m) | usable by |
|---|---|---|---|---|---|---|---|---|
| ramp_east | ramp | 1600 | 113 | yes | 25% | 17.9 | 10.8 | infantry, small vehicles, medium vehicles, heavy vehicles |
| ramp_main | ramp | 2600 | 123 | yes | 75% | 17.9 | 8.2 | infantry, small vehicles, medium vehicles, heavy vehicles |
| ramp_sally | ramp | 1200 | 191 | yes | 0% | 17.8 | 22.8 | infantry, small vehicles, medium vehicles |
| ramp_west | ramp | 2200 | 240 | yes | 0% | 16.0 | 9.6 | infantry, small vehicles, medium vehicles, heavy vehicles |
| knoll_ramp_w | ramp | 1400 | 324 | NO | 0% | 11.5 | 12.5 | infantry, small vehicles, medium vehicles, heavy vehicles |
| knoll_ramp_ne | ramp | 1200 | 348 | NO | 25% | 6.2 | 7.0 | infantry, small vehicles, medium vehicles, heavy vehicles |
| cp_gate | gate | 1200 | 89 | yes | 0% | 0.0 | 0.0 | infantry, small vehicles, medium vehicles, heavy vehicles |

### Graded ramps (length along the centre line)

| id | length m (straight) | width m | heights m | ramp grade % | max grade % (10 m) | tightest turn m | usable by |
|---|---|---|---|---|---|---|---|
| ramp_east | 167 (157) | 16 -> 22 | 18.0 -> 0.0 | 10.8 | 10.8 | 35 | infantry, small vehicles, medium vehicles, heavy vehicles |
| ramp_main | 219 (212) | 26 -> 36 | 18.0 -> 0.0 | 8.2 | 8.2 | 44 | infantry, small vehicles, medium vehicles, heavy vehicles |
| ramp_sally | 79 (73) | 12 | 18.0 -> 0.0 | 22.8 | 22.8 | 33 | infantry, small vehicles, medium vehicles |
| ramp_west | 167 (157) | 22 | 18.0 -> 1.9 | 9.6 | 9.6 | 35 | infantry, small vehicles, medium vehicles, heavy vehicles |
| knoll_ramp_w | 93 (92) | 14 | 16.0 -> 4.4 | 12.5 | 12.5 | 54 | infantry, small vehicles, medium vehicles, heavy vehicles |
| knoll_ramp_ne | 91 (90) | 12 | 16.0 -> 9.7 | 7.0 | 7.0 | 49 | infantry, small vehicles, medium vehicles, heavy vehicles |

### Mission

| # | objective | type | leg from | leg m | time s |
|---|---|---|---|---|---|
| 1 | hold_ridge | dont_lose_hq | - | - |  |
| 2 | ambush_convoys | kill_actors | hq | 542 | infantry 180.6, vehicle 108.3, convoy 154.8 |
| 3 | raid_base_a | destroy_base | hq | 406 | infantry 135.4, vehicle 81.2, convoy 116.0 |
| 4 | raid_base_b | destroy_base | hq | 820 | infantry 273.2, vehicle 163.9, convoy 234.2 |
| 5 | stop_heavy | kill_actors | ambush_convoys | 156 | infantry 51.9, vehicle 31.1, convoy 44.5 |
| 6 | recover_cargo | capture | stop_heavy | 6 | infantry 1.9, vehicle 1.1, convoy 1.6 |
| 7 | return_cargo | reach_area | recover_cargo | 645 | infantry 214.9, vehicle 128.9, convoy 184.2 |
| 8 | destroy_battery | destroy_base | hq | 636 | infantry 212.0, vehicle 127.2, convoy 181.7 |

### Resources (path distance from `hq`)

- radixite: 8 far
- metal: 2 near, 3 far

### Causeway corridors

| id | style | length m | width m | min width m (where) | class | sealable | chokes | crossings | used by |
|---|---|---|---|---|---|---|---|---|---|
| valley_road | road | 868 | 10.0 | 10.0 (base) | all vehicles | yes | viaduct_gap 10.0 m x 24 m | - | - |
| ammo_route | track | 310 | 10.0 | 10.0 (base) | all vehicles | yes | - | - | - |

### Enemy lanes: can wrecks seal them?

| spawn | narrowest on route | width m | class | sealable |
|---|---|---|---|---|
| convoy_entry | - | - | - | no |

### Wreck markers

| id | corridor | choke / crossing | width m | class | seals |
|---|---|---|---|---|---|
| wb_viaduct | valley_road | viaduct_gap | 10.0 | all vehicles | yes |

Lanes with every sealing marker blocked (route m to each start, '-' = cut off):

- convoy_entry: OPEN (hq 412)

### Elevation (m above base)

Base 0 m; ground from 0.0 to 35.0 m. Contours: c_ridge_foot 0 m, c_ridge_6 6 m (in c_ridge_foot), c_ridge_12 12 m (in c_ridge_6), c_ridge_rim 18 m (in c_ridge_12), c_bench_back 18 m (in c_ridge_rim), c_crest 35 m (in c_bench_back), c_fh_foot 0 m, c_fh_4 4 m (in c_fh_foot), c_fh_8 8 m (in c_fh_4), c_fh_12 10 m (in c_fh_8), c_knoll_foot 10 m (in c_fh_8), c_knoll_top 16 m (in c_knoll_foot); level pads: pad_heap_1 5 m, pad_heap_2 5 m, pad_heap_3 5 m, pad_battery 9.5 m

| corridor | start m | end m | climb m | mean grade % | max grade % (10 m window) | at (m) |
|---|---|---|---|---|---|---|
| valley_road | 0.0 | 0.0 | 0.0 | 0.0 | 0.0 | (-28, 420) |
| ammo_route | 0.0 | 9.5 | 9.5 | 3.1 | 10.7 | (70, 351) |

Height at each object (m): hq 18.0; b_gamma 18.0; b_refinery 18.0; b_barracks 18.0; adit_1 31.6; adit_2 31.7; adit_3 31.6; lbl_border 35.0; overlook 18.0; ramp_east 9.0; ramp_main 9.0; ramp_sally 9.0; ramp_west 9.9; knoll_ramp_w 10.2; knoll_ramp_ne 12.8; base_a 0.0; base_a_tanks 0.3; base_b 16.0; lbl_metal_bench 18.0; lbl_metal_valley 0.0; convoy_entry 0.0; convoy_exit 0.0; base_b_overwatch 8.6; base_a_alarm 0.0; heavy_intercept 0.0; survey_cargo 0.0; cargo_return 18.0; lbl_overwatch 10.0; lbl_exit 1.3; lbl_convoy 0.0; lbl_flank_w 3.3; lbl_flank_e 9.4; artillery_battery 9.5; pit_bs3_a 9.5; pit_bs3_b 9.5; pit_53k 9.5; pit_mortar_a 9.5; pit_mortar_b 9.5; pit_dshk_a 9.5; pit_dshk_b 9.5; pit_ampulomet 9.5; battery_guard 9.5; ring_normal 9.5; ring_supplied 9.5; lbl_ring_normal 0.0; lbl_ring_supplied 18.0; lbl_ammo 7.8; checkpoint 0.0; cp_weighbridge 0.0; cp_booth 0.0; cp_tower_n 1.6; cp_tower_s 0.0; cp_guards 0.0; cp_gate 0.0; viaduct 2.9; wb_viaduct 0.0; lbl_ambush 0.0; survey_post 0.0; sp_cryo 0.0; sp_generator 0.0; sp_tent 0.0; sp_guard 0.0; fossil_roots_1 3.2; fossil_roots_2 3.2; fossil_roots_3 4.6; fossil_roots_4 4.4; fossil_roots_5 6.9; smoulder_1 1.5; smoulder_2 0.0

Symmetry (share of identical terrain cells): {'mirror_ns': 0.092, 'mirror_ew': 0.449, 'rotate_180': 0.096, 'rotate_90': 0.087}

Content per sector (rows north to south): 0 0 8 4 / 1 7 2 4 / 0 4 5 7 / 0 1 3 0
