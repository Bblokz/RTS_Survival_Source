# Mission fuel crane (`AMissionFuelCrane`)

Set dressing for mission bases: a small Soviet tower crane that lifts cargo pieces off a parked truck,
swings them over the wall, and stacks them inside the base, in a loop. It is not an RTS unit (you can't select it),
but it is an `ADestructableEnvActor`: the player can right-click attack it and it can be destroyed (see
[Damage and death](#damage-and-death)).

- C++: `Source/RTS_Survival/Missions/MissionProps/FuelCrane/MissionFuelCrane.h/.cpp`
- Blueprint: `/Game/RTS_Survival/Blueprints/Missions/MissionProps/FuelCrane/BP_MissionFuelCrane`
  (sets the meshes on the components; holds no logic)
- Meshes and materials: the same folder (`SM_FuelCrane_*`; `M_FuelCrane_PaintedSteel` + `MI_FuelCrane_Paint`,
  `M_FuelCrane_Concrete`, `M_FuelCrane_DarkSteel`, `M_FuelCrane_Hazard`)
- Headless test: `_AssetSource/FuelCrane/headless_crane_test.py` (throwaway editor on RTS_ToCopySmall; it
  covers a parallel-parked ZIL, a nose-in ZIL that falls back to the pile, and a truck destroyed mid-carry;
  pictures go to `Saved/FuelCrane/`)
- Mesh source: `_AssetSource/FuelCrane/build_crane.py` (Blender 4.0) and `ue_import.py` (editor import plus
  materials). The site surveys are in the same folder.

## API

| Function | Notes |
|---|---|
| `StartUnloading(AActor* Truck)` | Starts or restarts the loop. Call it once the truck has parked. `Truck` may be null. |
| `StartUnloadingTruckOnce(AActor* Truck)` | One-shot mode for the [fuel delivery depot](FuelDeliveryDepot.md): lifts the truck's own reachable cargo once, never refills it, then hoists and goes idle. `OnTruckCargoEmptied` (native) fires when the last piece lands. Returns false (no fallback pile) when no cargo is in reach. |
| `StopUnloading()` | Freezes the crane in place. A piece on the hook stays on the hook. |
| `GetIsUnloading()` | True while the loop runs. |
| `GetUnloadedCount()` | Pieces placed on the stack since BeginPlay. The count only goes up. |

## How it works

- **Cargo source.** `StartUnloading` gathers the truck's visible `UStaticMeshComponent`s whose name starts
  with `TruckCargoComponentPrefix` (default `Cargo`) and that lie within the trolley reach. On
  `Rus_ArmoredCar_ZilFuel` these are the six `Cargo_0..5` barrel pallets (`SM_Airport_Barrel_Stack_2`,
  scale 0.88). If the truck has no such components, the crane uses its own pallet pile instead:
  - on the truck bed (`TruckBedLocalOffset`) when the bed is in reach;
  - otherwise at `PickupPointLocation`;
  - also when `Truck` is null or dead.

  The pile uses `CargoMesh` and `CargoScale`.
- **Lift.** On pick-up, the hook copies the piece's mesh, materials and transform onto `CarriedCargoMesh`.
  It then hides the source piece with `SetVisibility(false)`. Fog of war uses actor-level hiding, so the
  two flags don't clash. A four-leg sling is scaled to the piece's footprint.
- **Placement.** Pieces go into a `StackColumns x StackRows x StackLayers` grid around `DropStackLocation`.
  The ground height of each cell comes from a WorldStatic trace at BeginPlay. A pool of `MaxStackCount`
  components is reused, so the crane never spawns more than that.
- **Restarts.**
  - When the stack is full, it is cleared while the hook is away at the truck.
  - When the truck is empty, its pieces reappear while the hook is away at the stack.
- **Slewing route.** Long swings pass through `PreferredSlewViaYaw` (crane-local, default -90) instead of
  taking the shortest way, so the jib keeps clear of buildings on the other side.
- **Truck lost.** If the truck dies or is destroyed mid-cycle, the crane finishes placing the piece on the
  hook (if any), hoists, and halts. It only uses weak pointers to the truck.
- **EndPlay.** Hidden truck pieces are shown again, because the stack leaves with the crane.
- **Cost.** Movement is eased per phase and driven from Tick, which is off while idle. Each frame sets a
  handful of relative transforms. After a slew stops, the cable and hook swing in a small decaying pendulum.
- **Collision and navigation.** Only `BaseMesh` collides and affects navigation (one 2.8 x 2.8 m box up the
  mast). Every moving part and every pooled piece has no collision and `SetCanEverAffectNavigation(false)`.
  Two invisible query-only boxes take the weapon hits (see below); they never affect navigation.

### Cycle

`SlewToPickup` > `LowerToPickup` > `AttachCargo` (pause) > `HoistFromPickup` > `SlewToDrop` > `LowerToDrop`
> `ReleaseCargo` (pause) > `HoistFromDrop` > repeat.

With the defaults and a ZIL parked with its bed about 10 m from the mast, one cycle takes about 14 s:

| Step | Time |
|---|---|
| Lower to the truck | 1.2 s |
| Attach | 0.5 s |
| Hoist | 1.2 s |
| Slew 180° | 3.6 s |
| Lower to the stack | 1.9 s |
| Release | 0.5 s |
| Hoist | 1.9 s |
| Slew back | 3.6 s |

`SpeedMultiplier` scales every phase.

## Damage and death

| Item | Value |
|---|---|
| Base class | `ADestructableEnvActor` (right-click on it issues an attack; it takes damage through `TakeDamage`) |
| `Health` | 3 Pz IV KwK 40 shots (`DeveloperSettings::GameBalance::SovietBuildingShotHealth`, set in the constructor) |
| `MastHitBox` | Box on the root, `MastHitBoxExtent` (default 2 x 2 m wide, 0 to 9.4 m high) |
| `JibHitBox` | Box on `SlewPivot` around the jib mesh bounds plus `JibHitBoxPadding`; it slews with the jib |
| `AimOffsetPoints` | 3.5, 4.5, 6.5 and 8.8 m up the mast, so shots from outside clear the 3 m base wall |

Both hit boxes block `COLLISION_OBJ_PROJECTILE` and both weapon trace channels. Only the mast box blocks `Visibility`
(the cursor). The jib swings over the parked truck and the wall gap, so a jib box that blocked the cursor would turn
clicks meant for those into attacks on the crane. The crane is not auto-targeted: the player has to right-click the
mast.

At zero health (`OnUnitDies`) the crane:

1. Halts the loop and stops ticking.
2. Turns its hit boxes off and stops `BaseMesh` from blocking weapons.
3. Runs `DeathSettings` through `FMissionPropSinkDeath`
   (`Source/RTS_Survival/Missions/MissionProps/PropSinkDeath/`):
   - one scorch decal of 6.5–7.5 m (`MI_Decal_Scorch_03` or `_04`) on the ground under the mast;
   - `NS_DestructionSmoke_Large` at that point, in three bursts at 0, 3 and 6 s (one burst fades after about 5 s);
   - the actor sinks 13 m over 8 s with a small random tilt, then is destroyed. While it sinks it counts as dead,
     takes no damage and lets shells and weapon traces pass through it.

The smoke and decal assets load at `BeginPlay`, so the death doesn't hitch.

The mission's ZIL fuel truck uses the same helper. See `USecureBaseMission` and `FSecureBaseFuelDelivery`:

- `TruckHullHitboxMesh` (`SM_Hull_Zil_Hitbox`, one padded box) replaces the hull mesh. The hull still serves as
  the armour mesh.
- `TruckWreckDeath` runs on the wreck through `ATankMaster::SetWreckSpawnedHook`. It plays `NS_DestructionSmoke_Large` at 0.75 scale,
  sinks the wreck 4 m over 8 s, and leaves one scorch decal plus two small ember decals.

Test: `Source/Tools/IntroMission/crane_truck_death_test.py`.

## Rig layout (must match the meshes)

| Property | Default | Meaning |
|---|---|---|
| `SlewPivotHeight` | 840 | Top of `SM_FuelCrane_Base` (slewing ring) |
| `TrolleyRailHeight` | 30 | Underside of the jib bottom chords above the ring |
| `CableOriginOffset` | 30 | Trolley pivot down to the hoist sheaves |
| `HookEyeOffset` | 78 | Hook pivot (cable end) down to the lift eye |
| `SlingHeight` | 70 | Lift eye down to the top of the cargo |
| `MinTrolleyRadius` / `MaxTrolleyRadius` | 160 / 1180 | Trolley travel; the jib mesh ends at 1235 |
| `TravelHookDepth` | 175 | Cable length while slewing; cargo bottom ~4.3 m above the crane's ground |
| `IdleJibYaw` / `IdleTrolleyRadius` | 0 / 700 | Pose before `StartUnloading` (also shown in the editor) |

Mesh pivots:

| Mesh | Pivot |
|---|---|
| `SM_FuelCrane_Base` | Ground centre |
| `SM_FuelCrane_Jib` | Slew axis; jib along +X |
| `SM_FuelCrane_Trolley` | Rail contact |
| `SM_FuelCrane_Cable` | Top; 1 m long down -Z, scaled in Z |
| `SM_FuelCrane_Hook` | Cable end |
| `SM_FuelCrane_Sling` | Apex; legs to (±50, ±50, -100) |

## Other properties

- **Cargo:**
  - `TruckCargoComponentPrefix`, `TruckBedLocalOffset`, `PickupPointLocation` (3D widget)
  - `PickupPileCount`, `CargoMesh`, `CargoScale`
  - `DropStackLocation` (3D widget), `StackColumns/Rows/Layers`, `StackSpacing`, `MaxStackCount`
  - `bTraceDropGround`
  - `bStartWithoutTruckOnBeginPlay`: runs the pick-up-point loop without a mission call.
- **Timing:**
  - `SlewSpeedDegreesPerSecond`, `TrolleySpeed`, `HoistSpeed`, `AttachPauseSeconds`, `ReleasePauseSeconds`
  - `MinMoveSeconds`, `SpeedMultiplier`
  - `bUsePreferredSlewSide`, `PreferredSlewViaYaw`, `PreferredSideMinSwing`
  - `SwayKickDegrees`, `SwayFrequencyHz`, `SwayDamping`

## Blueprint setup

Set these on `BP_MissionFuelCrane`'s components. Don't change the components' transforms; C++ drives them.

| Component | Mesh |
|---|---|
| `BaseMesh` | `SM_FuelCrane_Base` |
| `JibMesh` | `SM_FuelCrane_Jib` |
| `TrolleyMesh` | `SM_FuelCrane_Trolley` |
| `CableMesh` | `SM_FuelCrane_Cable` |
| `HookMesh` | `SM_FuelCrane_Hook` |
| `SlingMesh` | `SM_FuelCrane_Sling` |

Also set `CargoMesh` to `/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/meshes/SM_Airport_Barrel_Stack_2`.
The ZIL fuel truck uses the same mesh, so a fallback pile looks the same as the truck's cargo.

## Ger_IntroMission placement (east wall, long camo bunker; provisional until checked in game)

| Item | Location | Yaw | Notes |
|---|---|---|---|
| Crane | (3650, -1850, 532) | 0 | Inside the wall, just west of `Rus_BXP_LongCamoBunker`; pedestal x 3510..3790, y -1990..-1710 |
| Stack | about (3200, -2100) | n/a | Default `DropStackLocation` (-450, -250, 0) |
| Truck | (4640, -1717, ~531) | 90 | Outside the wall, parallel to it, facing +Y |

- **Truck.** The `Cargo_N` pivots sit at a mesh corner, so the pallet block centre is at truck-local
  (-133, 0). The bed centre is at about (4640, -1850), 9.9 m from the mast. The pallets are 9.4–10.5 m
  from the mast, within the 11.8 m reach.
- **Reach tolerance.** About +1 m outward and ±4 m along the wall, and only with the truck parked parallel
  (yaw ±90). A truck that parks nose-in (yaw 180) puts the bed out of reach, and the crane then lifts from a
  ground pile at `PickupPointLocation` instead.
- **Clearances.**
  - Crane and stack: out of both wall gaps (driving lines y -2340..-2570 and -830..-990) and the infantry
    lane (y ≈ -1500, x ≤ 3500).
  - Jib: swings only east to south-west, never north toward the 11 m cooling towers.
- **Swing.** Long swings go via the -Y side, over the open south gap at about 4.3 m.
