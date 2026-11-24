# Pripyat Marshes (sketch v1) (pripyat_01, campaign)

**0 errors, 7 warnings, 10 notes.**

## Warnings
- `unreachable` feature "pilot_bog_a": it cannot be reached on the ground from "start"
- `unreachable` feature "pilot_bog_b": it cannot be reached on the ground from "start"
- `unreachable` feature "island_base": it cannot be reached on the ground from "start"
- `unreachable` feature "island_aa": it cannot be reached on the ground from "start"
- `unreachable` feature "island_vision": it cannot be reached on the ground from "start"
- `unreachable` feature "recon_zone": it cannot be reached on the ground from "start"
- `balance` resources: resource access differs between starts: {'start': 0.67, 'hq_site': 2.21} (distance-weighted)

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
- `coverage` features: 3 of 16 map sectors have no content: row 1 (from north), col 4 (from west); row 4 (from north), col 2 (from west); row 4 (from north), col 4 (from west)

## Metrics
- Size 1000.0 x 1100.0 m, analysis grid 150 cm, passable 11%, cliff bands 0%
- Landscape suggestion: 20x20 components of 63 quads at 100 cm (126000 cm, 8000 cm past the play edge)
- Start `start` on `start_rise`: build area 3969 m2, 100% passable, 100% flat
- Start `hq_site` on `ridge`: build area 14774 m2, 89% passable, 89% flat

### Enemy routes (spawns and bases)

| source | to | route m | straight m | detour | time s | through |
|---|---|---|---|---|---|---|
| spawn_n | start | 1047 | 855 | 1.22 | infantry 349.0, vehicle 209.4, column 299.2 | bridge_n |
| spawn_n | hq_site | 509 | 463 | 1.1 | infantry 169.5, vehicle 101.7, column 145.3 | bridge_n |
| spawn_e | start | 1103 | 1035 | 1.07 | infantry 367.8, vehicle 220.7, column 315.2 | ford_e |
| spawn_e | hq_site | 577 | 535 | 1.08 | infantry 192.5, vehicle 115.5, column 165.0 | ford_e |
| spawn_s | start | 1098 | 834 | 1.32 | infantry 366.0, vehicle 219.6, column 313.7 | bridge_s |
| spawn_s | hq_site | 616 | 572 | 1.08 | infantry 205.3, vehicle 123.2, column 176.0 | bridge_s |

### Openings

| id | kind | width cm | bearing | cuts barrier | enemy route share |
|---|---|---|---|---|---|
| bridge_n | bridge | 1000 | 21 | yes | 33% |
| ford_e | ford | 3500 | 92 | yes | 33% |
| bridge_s | bridge | 1000 | 148 | yes | 33% |

### Mission

| # | objective | type | leg from | leg m | time s |
|---|---|---|---|---|---|
| 1 | clear_checkpoint | kill_actors | start | 132 | infantry 44.1, vehicle 26.5, column 37.8 |
| 2 | clear_aa_1 | kill_actors | clear_checkpoint | 51 | infantry 17.0, vehicle 10.2, column 14.6 |
| 3 | clear_aa_2 | kill_actors | clear_aa_1 | 193 | infantry 64.5, vehicle 38.7, column 55.3 |
| 4 | clear_gate | kill_actors | clear_aa_2 | 100 | infantry 33.3, vehicle 20.0, column 28.6 |
| 5 | clear_aa_3 | kill_actors | clear_gate | 242 | infantry 80.8, vehicle 48.5, column 69.2 |
| 6 | clear_aa_4 | kill_actors | clear_aa_3 | 312 | infantry 103.9, vehicle 62.3, column 89.0 |
| 7 | seize_base | destroy_base | clear_aa_4 | 188 | infantry 62.7, vehicle 37.6, column 53.7 |
| 8 | deploy_hq | reach_area | seize_base | 59 | infantry 19.6, vehicle 11.8, column 16.8 |
| 9 | keep_hq | dont_lose_hq | - | - |  |
| 10 | repair_strip | defend_area | deploy_hq | 71 | infantry 23.7, vehicle 14.2, column 20.3 |
| 11 | first_sortie | kill_actors | repair_strip | 425 | infantry 141.6, vehicle 85.0, column 121.4 |
| 12 | hold_centre | survive_waves | - | - |  |
| 13 | recon_island | reach_area | - | - |  |
| 14 | destroy_island | destroy_base | - | - |  |
| 15 | opt_pilot_n | capture | deploy_hq | 392 | infantry 130.6, vehicle 78.4, column 112.0 |
| 16 | opt_pilot_e | capture | deploy_hq | 405 | infantry 134.9, vehicle 81.0, column 115.7 |
| 17 | opt_pilot_s | capture | deploy_hq | 484 | infantry 161.4, vehicle 96.8, column 138.3 |
| 18 | opt_pilots_bog | capture | - | - |  |
| 19 | opt_fuel | capture | deploy_hq | 286 | infantry 95.2, vehicle 57.1, column 81.6 |
| 20 | opt_pod | reach_area | start | 343 | infantry 114.5, vehicle 68.7, column 98.1 |

### Resources (path distance from `start`)

- radixite: 2 far
- metal: 1 far

Symmetry (share of identical terrain cells): {'mirror_ns': 0.231, 'mirror_ew': 0.161, 'rotate_180': 0.167}

Content per sector (rows north to south): 4 1 2 0 / 2 7 5 1 / 4 2 3 2 / 1 0 2 0
