# Enemy artillery battery (`AEnemyArtilleryBattery`)

A placed actor that turns existing weapon pits into an artillery battery that ammo trucks supply. Supplied, the pits
reach much further and shell the player's buildings and units in that reach; unsupplied they are ordinary pits. First
used for the Soviet battery of the Ore Mountains mission (see `Docs/Campaign/OreMountainsMission.md`), but nothing in it
is map-specific.

C++: `Source/RTS_Survival/Enemy/ArtilleryBattery/EnemyArtilleryBattery.h/.cpp`. It derives from
`AEnemyPlacementActor`, so it has the editor label ("BATTERY <BatteryId>", yellow when misconfigured) and the
editor-only drawings: the supplied reach and the truck arrival ring always, the auto-collect ring and links to the
listed pits while selected. It needs no enemy controller.

## Behaviour

- **Pits.** 2 s after BeginPlay the battery collects its pits: every actor in `Pits.PitActors`, plus (with
  `bAutoCollectPits`) every building expansion and team weapon within `AutoCollectRadiusCm` (2D) of the battery. A pit
  only joins while it is alive and owned by the battery's `EnemyPlayer` or an ally. Pits are collected again whenever
  a supply starts, and `AddPitActor` adds one later. The turrets of a pit are read on every update: a building
  expansion's mounted turrets, a team weapon itself, or turrets attached to any other actor.
- **Supply.** Each ammo truck adds `SecondsPerAmmoTruck`; `AddSuppliedSeconds` adds any amount. Supplied time stacks
  up to `MaxSuppliedSeconds`. It counts down on world time, so it pauses with the game. Each truck counts once: a
  truck seen by auto-detection and also reported through `NotifyAmmoTruckArrived` adds the time once.
- **Auto-detected trucks** (`TruckDetection.bAutoDetectAmmoTrucks`). Once a second, tanks of the battery's player
  within `ArrivalRadiusCm` (2D) are checked. A tank counts when it is a living, active **truck** member of a running
  `AConvoy` (`AConvoy::FindConvoyOfActiveMember` plus the member's role). With `bRequireAmmoCargo` the truck must
  carry Ammo: its own cargo from `AConvoy::GetTruckCargoType` (the spec's `CargoTypeOverride` in a mixed column),
  or the definition's `Cargo.CargoType` when the convoy does not know the truck. A truck that has surrendered or been released from its convoy is no
  longer an active member, so it does not count; report such trucks with `NotifyAmmoTruckArrived` instead.
- **Range while supplied.** Every weapon on a barrage-capable turret (largest calibre >= `MinimumCalibreMm`) shorter
  than `SuppliedRangeCm` gets the difference added as a behaviour range upgrade (`UWeaponState::Upgrade` with
  `FBehaviourWeaponAttributes::Range`). The turret rebuilds its range from that. The battery remembers the exact
  amount per weapon and takes it off when:
  - the supply ends;
  - the pit dies, is captured (owner no longer allied) or is abandoned (team weapon without crew);
  - the battery is destroyed or removed.

  A re-crewed team weapon, or a new turret on a pit, gets the range again on the next update.
- **Barrage.** While supplied, every `BarrageIntervalSeconds` (the first 1 s after the supply starts):
  1. The battery lists the hostile targets within `SuppliedRangeCm` (2D) of the battery actor: building expansions
     and deployed base vehicles (non-truck `ANomadicVehicle`) are **base** targets; other tanks and soldiers are
     **unit** targets. Aircraft are ignored. With `bOnlyVisibleTargets` only targets the battery's player can see
     count.
  2. For each active turret:
     - A turret with a hostile within `SelfDefenceRadiusCm` is handed back to its own targeting, so it can fight
       what is attacking it.
     - Otherwise the turret picks a base target with `BaseTargetPreference` chance (a unit target otherwise; it falls
       back to the other list when one is empty). Picks rotate through the list so the pits spread over different
       targets.
     - The aim point is scattered up to `AimSpreadRadiusCm` around the target, and the turret fires at that ground
       location (`ACPPTurretsMaster::SetEngageGroundLocation`) until the next interval.
     - Only aim points within 95 % of the turret's range and inside its traverse arc are used. A turret with no
       reachable target goes back to its own targeting.
  3. With no targets at all, every barraging turret goes back to its own targeting.
- **Supply ends.** Ranges go back to normal and the barrage turrets return to auto-engage (only the ones the battery
  took over, and never those of a dead, captured or abandoned pit). `OnSupplyEnded` fires.
- **Destroyed.** When every collected pit is dead or captured, the supply ends (with `OnSupplyEnded` if it was
  supplied), the timers stop and `OnBatteryDestroyed` fires once. An abandoned team weapon still counts as a pit,
  because it can be crewed again. After this `AddSuppliedSeconds` and `NotifyAmmoTruckArrived` do nothing. A battery
  with no pits never counts as destroyed (it reports an error at start).
- **Battery removed** (`Destroy` during play): ranges are restored and the turrets return to their own targeting. On
  a world teardown it only clears its timers.

## Settings (defaults)

| Group | Setting | Default | Notes |
|---|---|---|---|
| | `BatteryId` | None | Name for `FindEnemyArtilleryBattery`; the label goes yellow without it. |
| | `EnemyPlayer` | 2 | From `AEnemyPlacementActor`: the battery's player. Pits must be this player's or an ally's. |
| Pits | `PitActors` | empty | Placed pits (instance only). |
| Pits | `bAutoCollectPits` | true | |
| Pits | `AutoCollectRadiusCm` | 6000 | 2D from the battery. |
| Pits | `MinimumCalibreMm` | 0 | Set 20 to keep machine-gun pits (DShK) at normal range and out of the barrage. |
| Supply | `SecondsPerAmmoTruck` | 180 | |
| Supply | `MaxSuppliedSeconds` | 540 | Cap for stacked time. |
| Supply | `SuppliedRangeCm` | 40000 | Minimum weapon range while supplied, and the target search radius from the battery actor. Tune against the map. |
| Barrage | `BarrageIntervalSeconds` | 10 | |
| Barrage | `AimSpreadRadiusCm` | 900 | |
| Barrage | `BaseTargetPreference` | 0.75 | 0 = units only when any are in range, 1 = base first. |
| Barrage | `SelfDefenceRadiusCm` | 5000 | 0 turns self-defence off. |
| Barrage | `bOnlyVisibleTargets` | false | |
| Ammo Trucks | `bAutoDetectAmmoTrucks` | true | |
| Ammo Trucks | `ArrivalRadiusCm` | 2500 | |
| Ammo Trucks | `bRequireAmmoCargo` | true | Off: any convoy truck that arrives counts. |

## Blueprint API

| Kind | Name | Notes |
|---|---|---|
| Callable | `AddSuppliedSeconds(float Seconds)` | Escaped ammo cargo, scripted bonuses. |
| Callable | `NotifyAmmoTruckArrived(AActor* Truck) -> bool` | Once per truck; a null truck adds the time without the guard. False for a truck that already counted or a destroyed battery. |
| Callable | `AddPitActor(AActor* Pit)` | Pits placed or spawned later. |
| Callable | `EndSupplyNow()` | |
| Pure | `GetIsSupplied`, `GetSuppliedSecondsLeft`, `GetIsBatteryDestroyed`, `GetRemainingPitCount`, `GetBatteryId` | |
| Static | `FindEnemyArtilleryBattery(WorldContext, BatteryId)` | Mission Blueprints cannot reference level actors; find it by ID. Reports an error when there is none. |
| Event | `OnSupplied(Battery)` | Unsupplied to supplied. |
| Event | `OnSuppliedTimeAdded(Battery, SecondsAdded, SuppliedSecondsLeft)` | Every addition, the first included; `SecondsAdded` is after the cap. |
| Event | `OnSupplyEnded(Battery)` | |
| Event | `OnBatteryDestroyed(Battery)` | All pits dead or captured. |

## Setting it up in a map

1. Place the pits (e.g. `Rus_BXP_53KPit`, `Rus_BXP_BS3Pit`, `Rus_BXP_120MortarPit`, `Rus_BXP_DShKPit`,
   `Rus_BXP_AmpulometPit`) as the enemy player's.
2. Place `AEnemyArtilleryBattery` in the middle of them. Set `BatteryId` (e.g. `OreBattery`), and either list the pits
   in `PitActors` or leave auto-collect on with a radius that covers them (and no other base's pits).
3. Check that the always-drawn reach circle (`SuppliedRangeCm`) covers the player's base and that the arrival ring sits
   where the ammo trucks stop.

## Wiring it from a mission Blueprint

- On the stage that starts the ammo run: `FindEnemyArtilleryBattery(OreBattery)`, keep the reference and bind
  `OnSupplied` / `OnSupplyEnded` for Advisor lines or a HUD timer (`GetSuppliedSecondsLeft`).
- Convoy route event `OnBranchTruckArrived` (convoy behaviour) -> `NotifyAmmoTruckArrived(Truck)`. Leaving
  auto-detection on as well is safe: each truck counts once.
- Escaped ammo cargo (convoy pressure director) -> `AddSuppliedSeconds(seconds)`.
- Bonus objective: a `UKillActorsMission` on the pits, or bind `OnBatteryDestroyed`.

## Not verified

The code compiles but has not been play-tested. Not checked yet:
- whether direct-fire pits (53-K, BS-3) actually hit the ridge from the battery (terrain may block the shot line);
- how the mortar arc behaves at 400 m;
- whether a team weapon's controller overrides the ground order;
- the range numbers against the 570 m map.
