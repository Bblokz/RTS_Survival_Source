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

Source and generators: `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/SovietsStuff/VehicleAssemblyYard` (report `SovietAssemblyYard_Report.txt`).
Editor scripts: `Saved/AgentScripts/AssemblyYard/`
- `ue_assembly_yard_import.py`: `STEP` is `static`, `cranes` or `anims`. `anims` reimports the loops in place.
- `ue_assembly_yard_bp.py`
- `ue_assembly_yard_set_type.py`

Like the platform factory, the yard is meant to be placed in levels (the platform factory is in no async-spawner map or BXP option list). Only if players should build it does it need entries in the async spawner's `BuildingExpansionMap` / `BxpPreviewMeshMap` and a nomadic vehicle's option list.

Not yet done: damage meshes, a wreck, and a separate collision mesh.
