# B-4 and ML-20 concrete pits

A second emplacement for the two turntable howitzer pits. It is meant to stand on the airfield ground of the player start regions (`SM_Ground_1`, `M_RTSGround_WithSand`): the gun on an airfield concrete apron behind thick Soviet sandbag walls.

| Blueprint | Copy of | Changes |
|---|---|---|
| `B4_203MM/Pit/Rus_BXP_B4HowitzerConcretePit` | `Rus_BXP_B4HowitzerPit` | `BuildingMeshComponent`, `Sandbags` component and `BP_InitBuildingExp`'s building and construction mesh |
| `ML20_152MM/Pit/Rus_BXP_ML20ConcretePit` | `Rus_BXP_ML20Pit` | the same |

Unchanged from the earth pits:
- the expansion types (`BTX_RusB4HowitzerPit`, `BTX_RusML20Pit`);
- the turrets (`RTS_BP_EmbTurret_B4Pit`, `RTS_BP_EmbTurret_ML20Pit`) with the guns, weapons, crew, crane, cart, turntable and every animation and drill;
- the sizes (2000 × 2000 footprint).

The spawner still maps each type to the earth pit, so the build menu and the AI keep placing the earth pits. Place the concrete pits in a level, or point the spawner at them.

## The emplacement

`Meshes/SM_B4ConcretePit` is the building mesh; both pits use it. It has 2,992 triangles and three slots:

| Slot | Material | |
|---|---|---|
| `Ground` | `M_RTSGround_WithSand` | the 20 m square apron, flush with the ground (top at z 0), its edges bevelled into the ground |
| `Concrete` | `MI_Rus_BunkerConcrete_05` (the earth pit's concrete) | the ammo niche's floor and plinth |
| `Ammo` | `MI_B4_Trailer` (the B-4 cartridge's) | twelve 203 mm cartridge cases (the cartridge's LOD1) in a 5-4-3 pyramid, bases towards the gun |

The apron is laid like the airfield ground. Each 8 m panel is one UV unit, turned a random quarter turn and maybe flipped, and the panel grid starts on the gun's pivot.

`Meshes/SM_B4ConcretePit_Sandbags` is in the `Sandbags` component: Soviet pack sandbags (`MI_Rus_SandBag_01`).
- It is laid like the German Flak 88 pit: a square ring three bags thick, stretchers in running bond, 6 courses (87 cm).
- The middle row is laid only on top; below it the inner and outer rows hide it.
- The corners step down to 4 courses, so no part of the wall is under 4 high.
- The upper courses step back towards the gun.
- The inner face is 7 m from the pivot. It never comes within 6.5 m, where the earth pit's revetment stands, so everything that turns with the gun clears it.
- The crew gap is at the rear. The ammo niche goes through the left wall beside the shell rack, 1.7 m wide and 1.35 m deep, with walls two bags thick.

The bags are cut for non-Nanite rendering at 0.25 (see [GunPitMeshes.md](../GunPitMeshes.md)): from 237k to 59k triangles, with LargeProp LODs at 50, 25 and 12.5 %.

Both meshes have Nanite off and complex collision.

## Rebuilding

1. Run `blender -b --factory-startup -P Marketplace/Scritps/AnimTeamWeapons/B4_203mm/b4_concrete_pit_build.py -- "<(KADO-3D)/(EXPORTS)/B4_203mm>"`.
   - It writes `PitProps/SM_B4ConcretePit.fbx` and `SM_B4ConcretePit_Sandbags.fbx`.
   - It fails if a bag comes within the 6.5 m clear radius.
2. Run `pit_mesh_optimize.py -- <...>/PitProps/SM_B4ConcretePit_Sandbags.fbx Sandbag=0.25`.
3. Run `python Tools/B4Howitzer/build_b4_concrete_pit.py [mesh,bxp]`.

## Testing

- `-B4HowitzerValidate -B4HowitzerConcretePit` runs the B-4 test on the concrete pit (31 checks pass).
- Add `-B4HowitzerPit=ML20` for the ML-20 concrete pit (30 checks pass).
- The flag switches the pit class and the expected building mesh (`FB4HowitzerPitVariant::EmplacementMeshName`). Pictures come with `-B4HowitzerCapture=<dir>`.
