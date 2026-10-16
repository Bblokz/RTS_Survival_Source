# B-4 and ML-20 concrete pits

A second emplacement for the two turntable howitzer pits. It is meant to stand on the airfield ground of the player start regions (`SM_Ground_1`, `M_RTSGround_WithSand`): the gun on an airfield concrete apron behind thick Soviet sandbag walls.

| Blueprint | Copy of | Changes |
|---|---|---|
| `B4_203MM/Pit/Rus_BXP_B4HowitzerConcretePit` | `Rus_BXP_B4HowitzerPit` | `BuildingMeshComponent`, `Sandbags` component and `BP_InitBuildingExp`'s building and construction mesh; `Howitzer` child actor is the concrete turret below |
| `ML20_152MM/Pit/Rus_BXP_ML20ConcretePit` | `Rus_BXP_ML20Pit` | the same |
| `B4_203MM/Pit/RTS_BP_EmbTurret_B4Pit_Concrete` | `RTS_BP_EmbTurret_B4Pit` | the ammo runner (see below) |
| `ML20_152MM/Pit/RTS_BP_EmbTurret_ML20Pit_Concrete` | `RTS_BP_EmbTurret_ML20Pit` | the same |

Unchanged from the earth pits:
- the expansion types (`BTX_RusB4HowitzerPit`, `BTX_RusML20Pit`);
- the guns, weapons, the four crew, crane, cart, turntable and every animation and drill of the turrets;
- the sizes (2000 × 2000 footprint).

The earth pits and their turrets have no runner.

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

## The ammo runner

A fifth crewman keeps the shell rack stocked. Like the other crew he rides the turntable (`PitOrigin`), so his walk turns with the gun. He loops for ever, independent of the loading drill:
1. He takes the top shell from a ready rack at the turntable's rim, on the shell rack's side.
2. He carries it to the cart pusher's (Crew2's) shell rack and lays it on the top place.
3. He walks back.

The loop is 8.5 s on the B-4 and 10.2 s on the ML-20, where he goes round behind the left trail's spade.

The turret copies add these components:

| Component | |
|---|---|
| `ReadyRack`, `ReadyShell1`, `ReadyShell2` | a turned copy of `ShellRack` and its two lower shells, tagged `B4PitSocket:PitOrigin`. The ready rack has no top shell of its own: the runner's shell lies there until he picks it up. |
| `Runner` | `SK_RussianSoldier` with `AnimBp_B4Pit_Crew`, tag `PitRunner` |
| `RunnerShell` | the gun's shell mesh on the runner's spare bone `ik_foot_root` (unskinned on the soldier) |
| `PitRunnerCrew` | a second `UGunPitCrewComponent`: `CrewTag` `PitRunner`, `CrewAttachSocket` `PitOrigin`, his loop (`Pit/Crew/Anim_B4PitRunner_Loop` or `Anim_ML20PitRunner_Loop`) as the only idle, no rounds and no flinch. The turret's BeginPlay calls its `InitPitCrew` after the gun crew's. |

His shell never hides. He lays it exactly on `RackShell3`, so the two become one. The moment he lets go, his shell is back on the ready rack at the far end of his walk. The pop happens away from where the eye is.

The runner is built like the Flak 88 pit's runner ([GerFlak88Pit.md](../GerFlak88Pit.md)).

## Rebuilding

1. Run `blender -b --factory-startup -P Marketplace/Scritps/AnimTeamWeapons/B4_203mm/b4_concrete_pit_build.py -- "<(KADO-3D)/(EXPORTS)/B4_203mm>"`.
   - It writes `PitProps/SM_B4ConcretePit.fbx` and `SM_B4ConcretePit_Sandbags.fbx`.
   - It fails if a bag comes within the 6.5 m clear radius.
2. Run `pit_mesh_optimize.py -- <...>/PitProps/SM_B4ConcretePit_Sandbags.fbx Sandbag=0.25`.
3. Run `python Tools/B4Howitzer/build_b4_concrete_pit.py [mesh,bxp]`.
4. For the runner, run `blender -b -P Marketplace/Scritps/AnimTeamWeapons/B4_203mm/b4_pit_runner.py -- [B4,ML20] [preview]` with Blender 5.
   - It writes `Crew/Anims/Anim_<Pit>PitRunner_Loop.fbx` and `Crew/<pit>_pit_runner.json` in each gun's export folder.
   - The json holds the ready rack's place and the shell poses.
   - `preview` renders key moments to `PitRunnerPreview/`.
   - The ready rack's place, his stand-off and the corners of his walk are at the top of the script (Unreal pit space).
5. Run `python Tools/B4Howitzer/build_b4_concrete_runner.py [anims,turret,bxp] [B4,ML20]`.
   - It imports the loop and remakes the concrete turret copies from the earth turrets.
   - It solves `RunnerShell`'s offset on the bone from the loop and points the concrete BXPs at the copies.
   - Run step 3's `bxp` before this step: it copies the earth BXPs, which point at the earth turrets.

## Testing

- `-B4HowitzerValidate -B4HowitzerConcretePit` runs the B-4 test on the concrete pit (33 checks pass).
- Add `-B4HowitzerPit=ML20` for the ML-20 concrete pit (34 checks pass).
- The flag switches the pit class and the expected building mesh (`FB4HowitzerPitVariant::EmplacementMeshName`). Pictures come with `-B4HowitzerCapture=<dir>`.
- It also checks the runner with `FGunPitRunnerWatch` (`FB4HowitzerPitVariant::Runner`):
  - he is on `PitOrigin`;
  - over 24 s, his shell reaches the ready rack's top and `RackShell3`'s place (within 25 cm; both came within 0.2 cm).
  - The ready and rack shell pivots in `B4HowitzerTestScenario.cpp` come from `b4_pit_runner.py`'s layout. Update them if the layout moves.
