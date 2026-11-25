# Oder-Warthe direction-finder stations

The DF stations of Ger_OderWarthe are one shared, capture-only `ASupplyPoint` Blueprint, design F "Peilstelle": a loop
hut inside a small Adcock square. The user picked it from the six concepts in `Concepts/v01` on 2026-11-22.

| Step | Tool | Output |
|---|---|---|
| Concepts | `Concepts/run_round.sh <out> "<title>" specs/*.py` | `Concepts/v01/overview.png`, `sheet_*.png` |
| Model | `Blender5.0.exe -b --factory-startup -P Model/df_station_build.py -- [--preview]` | `D:/Blender/.../OderWarthe/Stations/` (FBX, .blend, `df_station_report.json`, previews) |
| Unreal | `sh run_ue_py.sh ue_build_df_station.py <log>` (headless, null RHI) | `/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Stations/` |
| Icon renders | `Icons/run_render.ps1` (headless GPU: check `nvidia-smi` first) | `Saved/OderWarthe/stations/icons/renders` |
| Icons | `python Icons/compose_df_icons.py`, then `sh run_ue_py.sh Icons/ue_import_df_minimap_icons.py <log>` | `Icons/Final`, `.../MiniMap/MiniMapIcons/DFStation`, rows in `DA_MinimapIcon` |
| Test | `run_station_test.ps1 [-Pictures] [-DebugGame]` | `Saved/SupplyPointTest/dfstation_*.log`, pictures |

## Blueprint `BP_OW_DFStation`

The Blueprint has no graph. C++ does the following:

- The flag (`Flag` under `FlagPole`, `SM_SupplyPoint_Flag`) rises 5 m. Its banner material follows the holder.
- `SupplyPointSettings`:
  - `bRunsSupplies` is off: no supply timer, no truck, no supply announcer lines.
  - `bEnemyCanCapture` is on, so the enemy can retake a station.
  - Initial holder 2 (enemy), capture radius 2500, bar at 9.5 m.
- Holder look: components tagged `EnemyHeld` (`SignEnemy`, a red star board) show while the player does not hold the
  station. Components tagged `PlayerHeld` (`SignPlayer`, a Balkenkreuz board, and `LampGlow`, the lit door lamp) show
  while the player does.
- Minimap: `EnemyMiniMapIcons` / `PlayerMiniMapIcons` map holders to `DFStation_*`. The icon is the loop aerial plus
  the holder's flag.

Layout: +X is the front (open yard, flag, sign), and the pivot is the capture centre. Blender +Y is Unreal -Y, so the
build script negates every Y and yaw from the report.

The loop turns as a child actor, `BP_OW_DFStation_Loop`, with a RotatingMovementComponent (16 deg/s). Python 5.5
cannot reach a material's World Position Offset pin.

Verified 2026-11-22 with `run_station_test.ps1 -Pictures` on Development: RESULT PASS, 0 failed checks. The icon was
DFStation_DirectorOfShield, then DFStation_GerStrikeDivision. No truck came.

Placed in `Ger_OderWarthe` on 2026-11-24 (all five, `OW_D_DF_s<N>_Station`, on flattened pads; see
`Source/Docs/Campaign/OderWartheMap.md`, "The Oder-Warthe kit").

Not done:
- The enemy retake is not exercised.
