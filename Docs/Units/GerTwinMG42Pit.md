# German twin MG42 pit

`Ger_BXP_GerPit_TwinMG42` (`BTX_GerTwinMG42Pit`): two MG42 turrets in round sandbag nests, joined by a sandbag corridor with an ammunition bay. Each MG has a scavenger gunner and a scavenger loader who fetches belt boxes from the bay on every reload.

Blender sources: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Exports/TwinMG42Pit` (`generate_twinmg42_pit.py`, `twinmg42_pit_crew.py`, `validate_twinmg42_pit.py`, report `twinmg42_pit_report.txt`). Import: `Tools/GerPits/ue_twinmg42pit_import.py`.

## Layout (BXP space, cm)

| Part | Where |
|---|---|
| Left nest, turret `Howitzer` (`Ger_BP_EmbTurret_TwinMG42_Left`) | (40, -400) |
| Right nest, turret `MG42Right` (`Ger_BP_EmbTurret_TwinMG42`) | (40, 400) |
| Corridor lane | x 0..80, between the nest openings; front wall steps 3 → 4 → 5 courses towards the middle |
| Ammo bay | x -150..0, y -75..75, behind the corridor; a crate per loader at y ±36 |

Nests: two rings of stretcher-laid bags (inner radius 125, outer 195), inner ring 4 courses, outer 3, one opening facing the corridor. Mesh `Meshes/SM_GerPit_TwinMG42_Sandbags` (21k tris after the sandbag cut, Nanite off).

`PitDefense` `M_CollisionSettings.MaximumHorizontalExtent` is (300, 560), so the weapon-hit collider (300 × 560 × 175 in PIE) spans both nests. `MinimumExtent` stays (300, 375, 175).

## Turret crew

| Component | Socket | Role |
|---|---|---|
| `PitCrew` (`CrewTag` PitCrew) → `Crew1` | `PitOrigin` (Traverse) | DShK pit gunner: idle, burst shake, reload (cocks at the end). No gun reload montage. |
| `LoaderCrew` (`CrewTag` PitLoader) → `Crew2` | `PitStatic` (Root, static) | loader drill `Montage_GerTwinMG42Pit_Loader{L,R}_Reload`, idle `Anim_GerTwinMG42Pit_Loader{L,R}_Idle` |
| `LoaderBox` on `Crew2` / `ik_foot_root` | identity | the belt box he carries; the bake scales the bone to 0.001 while his hands are empty |

`PitStatic` was added to `SK_DShKPit_Gun_Skeleton` on `Root`, coincident with `PitOrigin` at rest. The turret graph calls `InitPitCrew` and `OnReloadStarted` on both crew components.

The loader drill is 8.9 s, stretched to the 6 s reload: nest opening → corridor → bay crate (takes a box by the handle) → back → lifts it to the gun's feed → steps back.

## Limits

- The gun turns 360°; the loader does not. With the gun swung far round, he hands the box up beside the gun's rest position.
- The shared DShK gun mesh still shows its own belt box and spare stack beside the MG42 visual.
- `MG42Right` is shown by `PitAdditionalTurretComponent` only once the BXP reaches Built, and the turrets only auto-engage after construction. A BXP spawned at runtime for tests shows only the left MG and does not fire until both happen.
