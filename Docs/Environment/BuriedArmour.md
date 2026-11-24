# Buried 1945 armour (recoverable vehicles)

Five Wehrmacht vehicles abandoned in spring 1945, half-buried in sand (style C3 "Eckversunken"; see
`Docs/Campaign/OreMountainsMission.md`, "Buried 1945 armour"). A squad scavenges one, the squad is consumed (it takes
command), and the real vehicle spawns for that squad's player. Built 2026-11-23 for the Ore Mountains mission, not
placed on any map yet.

## Assets

Folder: `/Game/RTS_Survival/Blueprints/Environment/OreMountains/BuriedArmour/`

| Blueprint | Recovered vehicle | Scavenge time | Parts under `Pose` |
|---|---|---|---|
| `BP_BuriedArmour_StuGIII` | `Ger_Vehicle_StugIII` | 30 s | hull, casemate gun, top MG34 |
| `BP_BuriedArmour_Wespe` | `Ger_Vehicle_Wespe` | 25 s | hull, gun |
| `BP_BuriedArmour_FlakLuchs` | `Ger_Vehicle_PanzerII_Luchs_FlakLuchs` | 25 s | hull, twin weapon, casing, drum |
| `BP_BuriedArmour_PzIV_F1_Assault` | `Ger_Vehicle_PanzerIV_F1_Assault` | 30 s | hull, hull MG, troop compartment |
| `BP_BuriedArmour_Sdkfz251_MortarTW` | `Ger_ArmoredCar_251_MortarTW` | 20 s | hull, GrW 34, two MG34 |

- `BP_BuriedArmour_Base` is the parent (C++ `ABuriedVehicleSite`). It holds the progress bar widget class
  (`W_BP_TimeProgressBarWidget`, copied from `BP_ScavengeableObject`) and the reward widget class.
- Each child has the sand mound `Meshes/SM_BuriedSand_<Vehicle>` (`MI_BuriedArmour_Sand`) on the root `Sand`, and
  the vehicle's own hull/turret static meshes and weapon skeletal meshes under `Pose`. Each part keeps its
  Blueprint-relative transform and material overrides from `BuriedArmour_vehicle_dump.json`.
  - `Pose` = `pose_component` of `BuriedArmour_poses.json`: nose up 4 deg, 5 deg onto the right side, sunk 8 cm.
  - Left out: tracks, wheels, crew, the Wespe's loose ready rounds.
  - Scavenge times are starter values for the owner to tune.
- Sand meshes since 2026-11-23: complex-as-simple collision (query only) and eight `Scav_01`-`Scav_08` sockets.
  The sockets sit side by side, 1.2 m apart, 1.1 m outside the nav blocker, around its shallow front-left corner
  (nose and left flank, away from the dune). Squad unit i walks to socket i and is teleported there after 2 s.

## C++: `ABuriedVehicleSite`

`Source/RTS_Survival/Environment/BuriedVehicle/BuriedVehicleSite.h/.cpp`, derived from `AScavengeableObject`.

| Setting | Default | Meaning |
|---|---|---|
| `RecoveredVehicleClass` | set per child | Vehicle Blueprint spawned for the scavenging player |
| `VehicleSpawnOffset` | (0, 0, 60) | Actor-space offset (yaw only); Z lifts the vehicle onto the ground, as supply points do |
| `bCanBeDestroyedByDamage` | false | Off: shells hit the site but do nothing, so the reward can't be lost. On: dies as a plain wreck |
| `ScavengeTime` | 30 (child overrides) | Base scavenging setting |
| `bConsumeSquadWhenFinished` | true | Base scavenging setting: the squad takes command |
| `bGivePlayerResourceRewards` | false | No resources; the vehicle is the reward |

| Event | When |
|---|---|
| `OnVehicleRecovered(AActor* Vehicle)` (BlueprintAssignable) | Once, right after the vehicle spawned |
| `BP_OnVehicleRecovered(AActor* Vehicle)` (BlueprintImplementableEvent) | Same moment, for the child Blueprint |
| `GetRecoveredVehicle()` (BlueprintPure) | The vehicle, or null before recovery |

The base scavenge hooks still work: `MissionBase::RegisterCallbackOnScavengableObjectScavenged` (the mission's
`BP_OnCallBackScavengableObjectScavenged`) fires on completion, *before* the vehicle exists. To get the vehicle, bind
`OnVehicleRecovered` instead.

Base class change: `AScavengeableObject` has a new protected virtual `OnScavengeCompletedBySquad(ASquadController&)`.
It is called at the start of `OnScavengingComplete`, before the squad is consumed. The site reads the squad's owner
there.

## How the recovery works

1. **Before:**
   - `Sand` is the scavenge mesh (set in the constructor). It gets the base scavengeable collision: Destructible,
     query only, blocks weapons and Visibility. It then ignores pawns, vehicles and the player/enemy unit object
     channels, so units wade through the low drift ring.
   - The parts under `Pose` get query-only Visibility + projectile + weapon collision at runtime. The hull and gun
     that stick out of the sand take the scavenge click and stop shells.
   - In the editor the parts and the sand never affect navigation.
   - `NavBlocker` is a hidden box, a dynamic nav obstacle with `NavArea_Null`. It covers the hull footprint (+40 cm)
     and all sand above 1.1 m, so nothing paths through the vehicle.
   - Infantry walk to the `Scav_` sockets at the mound foot.
2. **Scavenging:** this is the ordinary mechanic: right-click with a squad, progress bar, a pause when the squad
   leaves.
3. **On completion** (`OnUnitDies` with `ERTSDeathType::Scavenging`):
   1. The squad is consumed.
   2. Every part under `Pose` loses collision and is hidden.
   3. `Sand` collision is switched off and `NavBlocker` is destroyed, which removes the obstacle from the navmesh.
   4. `RecoveredVehicleClass` spawns through `FRTSUnitOwnerSpawning::SpawnUnitForPlayer` for the squad's owner (the
      human if that can't be read). It spawns at the actor location + offset, with the actor's yaw only, so the nose
      points along the actor's +X (the mound's nose).
   5. The two events fire.
4. **After:** the sand mound stays as decoration, a low plateau where the hull was, with no collision. The vehicle
   lands on the landscape inside it and can be driven out. The empty reward widget shows briefly; there is no
   resource line.

Precedent: `BP_SpawnerScavObjectsMaster` / `BP_Scav_Marder` spawn a tank in a Blueprint `BP_OnUnitDies` and then
collapse. The buried sites do this in C++ instead, keep the mound, and set the owner properly.

## How to place

- Drop a `BP_BuriedArmour_<Vehicle>` on open, fairly flat ground. The root is the ground point. The 4 m sand skirt
  hides slopes, but keep the ground under the hull footprint within about 30 cm.
- Yaw it freely: the recovered vehicle faces the actor's +X. Put a clear exit in front of the nose, since the rear
  right is the deep dune.
- The navmesh must allow runtime modifiers (dynamic obstacles). The sanded houses on Ger_IntroMission rely on
  `DynamicModifiersOnly` too. Check this on the Ore Mountains map before trusting the blocker.
- Placement picked 2026-11-22: StuG III near the ridge, Pz IV carrier in the north foothills, 251 mortar by the
  viaduct, Flak-Luchs near the radio outpost, Wespe near the battery. Exact spots are still open.

## Tools

`Source/Tools/BuriedArmour/ue_buried_armour_bps.py` rebuilds everything from scratch (sand collision, sockets, base
and child Blueprints): `BA_STEP=build`. `BA_STEP=check` fresh-loads each child, spawns it in an unsaved `/Temp` level
and compares every part's world transform with `xf * pose * actor`, its mesh and its materials. Result
2026-11-23: all 5 load and spawn, every part within 0.01 cm, 8 sockets each, nav blocker and classes set. An editor
world never runs `PostInitializeComponents` or refreshes physics queries, so the runtime collision and the visibility
traces it records are info only. Run both with the DebugGame commandlet (the class
needs a DLL that has it). Reports go to `Saved/BuriedArmour/bps_<step>.json`.

## Not verified

- No play test: the scavenge-to-vehicle path (squad consumed, owner read, vehicle spawned, nav blocker gone,
  vehicle driving out) has not run in a game world.
- The Development DLL (2026-11-22) has no `ABuriedVehicleSite`. Until Development is rebuilt, these Blueprints only
  load in DebugGame editors.
- The runtime collision setup (sand query collision, part Visibility/weapon collision) and clicking the site have not
  been seen working.
- The vehicle spawns level (the burial tilt is dropped) inside collision-free sand, so it pops from the tilted pose.
- The progress bar widget is initialised in C++ `BeginPlay` (the scavenge template does it in its Blueprint graph).
- Whether spawned vehicles are added to the player's selection or control groups is up to the normal unit
  registration on BeginPlay.
