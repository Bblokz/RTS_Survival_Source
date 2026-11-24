# Soviet vehicle assembly yard

A large concrete platform where four cranes work as one line on the vehicle being trained, with a 45 mm 53-K in a concrete-roofed casemate on its front corner and floodlights on three corner masts.

| | |
|---|---|
| Expansion type | `BTX_RusAssemblyYard` ("Assembly Yard") |
| Blueprint | `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/AssemblyYard/Rus_Building_AssemblyYard` |
| Health, abilities, build time, vision | the platform factory's (`BTX_RusPlatformFactory`): same role |
| Cost | 3500 radixite, 1750 metal (`RusAssemblyYard_*` in `DeveloperSettings.h`) |
| Footprint | 4800 × 3300 free placement; the mesh is 43.9 × 32.4 m, X −20.3..+23.6 m from the origin |

The Blueprint is a copy of `Rus_BXP_53KSectorPit`, so it keeps that pit's turret setup and `PitDefense` (inert crew):

- `Howitzer`: `RTS_BP_EmbTurret_Rus53KSectorPit` at (1920, 1085, 90), facing +X; the casemate keeps its ±45° arc clear (roof underside 2.85 m over the floor).
- `YardMachinery` holds the four crane skeletal meshes and six floodlight spotlights. They are hidden with the gun during construction and pack-up.
  - The cranes are `SK_AssemblyYard_{GantryFront,GantryRear,JibFront,JibRear}`. Each is authored in building space and plays its 30 s loop `AN_AssemblyYard_*_Loop` as a single-node animation, with always-tick pose and no update-rate optimisation.
  - The part hand-offs between the cranes are coincident visibility swaps, so the four loops must stay on the same frame.
- `VehicleSpot` at (−75, 0, 93): where the vehicle being trained stands. The gantries hide each part 1.5 m above it.

## Enemy AI training

The yard trains tanks for the enemy strategic AI (`Docs/EnemyAI/EnemyBuildingTraining.md`):

- `EnemyBuildingTrainer`: `TrainerKind` Vehicles, `IdleAnimation` FinishCycle, `IdleRestSeconds` 2.0.
- The four cranes are tagged `AITrainingActivity` and do not autoplay (`saved_playing` off, `saved_position` 2.0). The trainer starts all four from 2.0 s on the same call, so the hand-offs stay in step. When the queue is empty, they play on to 2.0 s in the loop and rest there.
  - Why 2.0 s: parts hang on a hook at 3.0–15.0 s (front jib), 22.56–31.2 s (front gantry, hidden over the bay at 1.2 s), 18–30 s (rear jib) and 7.56–16.2 s (rear gantry). The only part-free windows are 1.2–3.0 s and 16.2–18.0 s (`assembly_yard_kit.schedules`, front phase 0, rear phase 15). At 2.0 s (frame 48), the transmission is already hidden in the bay and the engine sits on the rear table. The front gantry's empty hook is still low over the bay, the front jib's empty hook is going down into its housing, and the rear gantry is parked.
  - Frame 0 is not a rest: the front gantry is lowering the transmission over the bay.
  - The cranes use `AlwaysTickPoseAndRefreshBones`. With `AlwaysTickPose`, off-screen bones lag behind the animation (the trainer test now refreshes bones before each capture as well).
- `Floodlight_1..6` are tagged too: on only while training. The lamp lenses are an emissive slot of the yard mesh and keep glowing.
- `BayVehicle` (`SM_CargoTank_T3476`, a baked T-34/76) on `VehicleSpot`, tagged and hidden by default. It shows while any tank trains and gives the parts a hull to disappear into. It is not under `YardMachinery`, so the construction and pack-up `SetVisibility` nodes never show it.
- `AITrainingExit` at (3000, 0, 0), +X: on the ground 7 m in front of the exit ramp, clear of the footprint's `NavArea_Obstacle` modifier and selection box (X up to 2360). There, the test's BT-7s spawn with `exit_dist=0` and drive 24 m. Without an exit, the fallback point (bounds + 4 m, X 2760, on another grid slot with neighbours) left a BT-7 stuck. Why is not established.
- Construction and pack-up still hide `YardMachinery` (propagate). At construction finish, the floodlights show for up to 1 s, until the trainer's idle check hides them.
- Script: `Saved/AgentScripts/AssemblyYard/ue_assembly_yard_trainer.py` (idempotent, headless). Test: `run_enemy_building_trainer_test.ps1 -Buildings AssemblyYard -Capture` (passed 2026-11-19, `Saved/EnemyBuildingTrainer/yard_run3`).

Source and generators: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/VehicleAssemblyYard` (report `SovietAssemblyYard_Report.txt`).
Editor scripts: `Saved/AgentScripts/AssemblyYard/`
- `ue_assembly_yard_import.py`: `STEP` is `static`, `cranes` or `anims`. `anims` reimports the loops in place.
- `ue_assembly_yard_bp.py`
- `ue_assembly_yard_set_type.py`
- `ue_assembly_yard_trainer.py` (enemy AI training setup) and `ue_assembly_yard_trainer_inspect.py` (read-only component dump + graph T3D)

Like the platform factory, the yard is meant to be placed in levels (the platform factory is in no async-spawner map or BXP option list). Only if players should build it does it need entries in the async spawner's `BuildingExpansionMap` / `BxpPreviewMeshMap` and a nomadic vehicle's option list.

Not yet done: damage meshes, a wreck, and a separate collision mesh.
