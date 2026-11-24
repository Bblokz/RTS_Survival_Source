# Surrendering convoy trucks and the armoured case truck

Capturable convoy trucks for the Ore Mountains mission (campaign mission 2). Only trucks of these classes surrender;
every other convoy, survival convoy and supply-point truck keeps its old behaviour. Code:
`RTS_Survival/Enemy/Convoy/SurrenderingCargoTruck/`. Design: `Docs/Campaign/OreMountainsMission.md`, "Capturing convoy
trucks" and "Mission structure" (convoy 5).

| Class | Use |
|---|---|
| `ASurrenderingCargoTruck` | Ordinary capturable truck. Point `FConvoyTruckSpec::TruckClass` at a Blueprint child. |
| `ASurrenderingCaseTruck` | The armoured case truck of convoy 5 (child of the class above). It carries the Berlin cases. |
| `USurrenderingTruckSubsystem` | World subsystem. The mission binds its events once, because the case truck is gone before most of its outcomes resolve. |
| `UW_SurrenderingTruckCountdown` | The "SCUTTLING n" plate. It needs no asset. |

Both truck Blueprints derive from `ATrackedTankMaster` but not from `RTS_BP_TrackedTankMaster`. Copy its AI controller
and setup, and call `InitTrackedTank` and `SetupTurret` as for any tracked tank.

## Rules

1. **Trigger.** The last escort of the convoy dies while the column drives. A convoy that never had an escort starts
   once a player unit comes within `UnescortedAlertRadius` or the truck is hit.
2. **Scuttle countdown.**
   - The truck stops and shows the countdown plate. The plate is hidden by the fog of war and by the "hide all game UI"
     toggle.
   - A player squad within `CaptorSearchRadius` takes the truck before the countdown ends.
   - A truck below `MinHealthFractionToSurrender` only scuttles.
   - If nobody comes, the crew blows the truck up through the normal damage path, so the convoy counts it as destroyed.
3. **Surrender.**
   - **Owner.** The truck changes owner to `CapturingPlayer` and leaves the convoy (`AConvoy::ReleaseCapturedTruck`),
     which counts it as captured.
   - **Health bar.** It switches to the captor's colours and visibility rules.
   - **Minimap.** The unit dot turns to the captor's colour (`UFowComp` follows the owner change), and the supply-truck
     icon is added for player 1.
   - **White flag.** Shown if `WhiteFlagMesh` is set.
   - **Advisor line.** The first truck to surrender in the world plays `FirstSurrenderVoiceLine` once, on the portrait
     system. It also fires `BP_OnFirstSurrenderInWorld` and the subsystem's `OnFirstTruckSurrendered`.
4. **Delivery.**
   - After `SurrenderStandSeconds`, the truck drives itself (`USupplyTruckComponent`) to the nearest actor tagged
     `CapturedTruckDropOff` (or `SetDropOffTarget`). It pays only on arrival, and nothing chases it.
   - **Payout:** the whole per-truck cargo value is multiplied by `CapturedCargoMultiplier`. "Whole value" means every
     resource in one truck's cache, summed.
   - The payout is paid in the resource that `PayoutMapping` gives for the truck's cargo type. That is the truck spec's
     `CargoTypeOverride`, or the definition's `Cargo.CargoType`.
   - **Example:** a fuel truck whose cache would hold 40 metal and 20 parts pays 2.5 x 60 = 150 Radixite. Author the
     convoy definitions' `ResourcesPerGuardPoint` with this in mind. The simplest choice is one resource per
     definition, so the cache and the capture pay the same kind.
   - **Set the line everywhere.** The first truck to surrender uses up the once-per-world line, so set
     `FirstSurrenderVoiceLine` on every truck Blueprint, the case truck included.
5. **Cargo caches of destroyed convoy trucks** (`ConvoyCargo.cpp`).
   - **Where:** the cache lands beside the actual wreck, `CacheSideOffset` to the side away from the road centre. That
     includes branch trucks off the road.
   - **What it pays:** a truck whose cargo type differs from the definition's (`CargoTypeOverride`) pays its whole
     share in its own type's resource, with the same mapping as a capture. The default mapping applies to trucks that
     cannot surrender. Other trucks pay the definition's resources as before.
6. **Lost on the way.** A captured truck destroyed on the way fires `OnCapturedTruckLost`, and its cargo is gone.
   Captured trucks never drop a cargo cache.

### The case truck

| Outcome | What happens | Events |
|---|---|---|
| Captured and reaches the drop-off | The cases are recovered. It pays its cargo as well. | Truck `OnCasesDelivered`, subsystem `OnCasesDelivered` |
| Destroyed: shot, scuttled, killed mid-countdown or after capture | The convoy drops **no** cargo cache. After `SealedCacheSpawnDelaySeconds`, a **sealed cache** spawns `SealedCacheSideOffset` to the right of the wreck, on the navmesh. It never expires. | Truck `OnCasesDropped(Transform)`, then subsystem `OnSealedCacheSpawned(Cache)`; on a finished scavenge, `OnSealedCacheScavenged(Cache)` |
| Removed without dying, but not by an escape (a stuck captured run abandoned, `EndConvoy` with despawn, a script `Destroy`) | Same as destroyed: a sealed cache where it stood, so the mission stays winnable. | As above |
| Leaves the map with its convoy (convoy outcome `Escaped`) | The cases are gone. They come back when the mission calls `SpawnEscapedCasesSealedCache` (Base B falling). | Truck `OnCasesEscaped`, subsystem `OnCasesEscaped`; `GetHaveCasesEscaped` becomes true |

- **Sealed cache shot to pieces before it was scavenged.** With `bRespawnWhenDestroyed` (default on), it respawns at the
  same spot and fires `OnSealedCacheSpawned` again. With it off, the subsystem fires `OnSealedCacheDestroyed`.
- **The `Cache` pin.** `OnSealedCacheScavenged` and `OnSealedCacheDestroyed` pass the cache actor, which may already
  be none if the cache removed itself. Rely on the event, not on the pin.
- **Who scavenges.** Scavenging always rewards the human player, since `AScavengeableObject` pays its controller.
  Any finished scavenge counts.
- **Hard to destroy.** Every hit is multiplied by `DamageTakenMultiplier` (0.25) after armour has decided the
  penetration. Set health and armour in the Blueprint as well. The scuttle still kills it.

## Settings (defaults)

**`SurrenderSettings`:**
- `bCanSurrender` true
- `Trigger` AllEscortsDestroyed
- `EscortProtectionRadius` 3500
- `bRequireCaptorNearby` true
- `Captors` SquadsOnly
- `CaptorSearchRadius` 2000 cm
- `MinHealthFractionToSurrender` 0.6
- `CapturingPlayer` 1
- `SurrenderStandSeconds` 3
- `CheckIntervalSeconds` 0.5

**`ScuttleSettings`:**
- `bScuttleWhenNotCaptured` true
- `CountdownSeconds` 12
- `bStopDuringCountdown` true
- `UnescortedAlertRadius` 3000
- `ScuttleDamageType` Kinetic
- Optional explosion effect and sound
- Countdown widget class, offset (0, 0, 450), text "SCUTTLING {0}", refresh 0.1 s

**`DeliverySettings`:**
- `DropOffTag` `CapturedTruckDropOff`
- `CapturedCargoMultiplier` 2.5
- `PayoutMapping`:
  - Fuel → Radixite
  - VehicleParts → VehicleParts
  - Ammo → Metal
  - Concrete → Metal
- `bUseFixedCargo` false, with `FixedCargo`

**`DisplaySettings`:**
- `WhiteFlagMesh` none. The project has no white flag mesh yet; `SM_FlagPole` (FactionFlags) or `SM_SupplyPoint_Flag`
  could be re-skinned.
- `WhiteFlagSocket` none (the root)
- `WhiteFlagRelativeTransform` z 300
- `FirstSurrenderVoiceLine` none
- `FirstSurrenderPortrait` GermanAnnouncer (the Advisor)

**`CaseTruckSettings`** (case truck only):
- `DamageTakenMultiplier` 0.25
- `SealedCache`:
  - `SealedCacheClass`: **must be set**; the truck reports an error at BeginPlay without it
  - `ScavengeSeconds` 30 (0 keeps the Blueprint's time)
  - `bOverrideRewards` false, with `Rewards`
  - `MinimapIcon` Objective
  - `bRespawnWhenDestroyed` true
- `SealedCacheSpawnDelaySeconds` 1.5
- `SealedCacheSideOffset` 650 cm

When `bOverrideRewards` is off and `ScavengeSeconds` is above 0, the Blueprint's rewards are kept at their expected
value (random amounts become fixed).

## Blueprint API

**`ASurrenderingCargoTruck`:**
- **Calls:** `SetDropOffTarget(Actor)`, `SetCanSurrender(bool)`, `SurrenderNow()`.
- **Getters:** `GetSurrenderState`, `GetHasSurrendered`, `GetScuttleSecondsLeft`, `GetCapturedCargo`.
- **Events:**
  - `OnScuttleCountdownStarted(Truck, Seconds)`
  - `OnScuttled(Truck)`
  - `OnSurrendered(Truck)`
  - `OnCapturedCargoDelivered(Truck, Cargo)`
  - `OnCapturedTruckLost(Truck)`
- **BP events:**
  - `BP_OnScuttleCountdownStarted`
  - `BP_OnScuttled`
  - `BP_OnSurrendered(NewOwningPlayer)`
  - `BP_OnFirstSurrenderInWorld`
  - `BP_OnDriveToDropOffStarted(DropOff)`
  - `BP_OnCapturedCargoDelivered(Cargo)`
  - `BP_OnCapturedTruckLost`

**`ASurrenderingCaseTruck`** (adds to the above):
- **Static calls:**
  - `GetActiveCaseTruck(WorldContext)`: the case truck that still carries the cases, or none.
  - `SpawnSealedCacheAt(WorldContext, Class, Transform, ScavengeSeconds = 30)`.
- **Getters:** `GetCasesState` (OnTruck / Delivered / Escaped / InSealedCache), `GetIsCarryingCases`.
- **Events:** `OnCasesDelivered(CaseTruck)`, `OnCasesEscaped(CaseTruck)`, `OnCasesDropped(CaseTruck, SealedCacheTransform)`.
- **BP events:** `BP_OnCasesDelivered`, `BP_OnCasesEscaped`, `BP_OnCasesDropped(Transform)`.

**`USurrenderingTruckSubsystem`** (the "Get SurrenderingTruckSubsystem" node):
- **Getters:**
  - `GetActiveCaseTruck`
  - `GetHaveCasesEscaped`
  - `GetOpenSealedCacheCount`
  - `GetHasAnyTruckSurrendered`
  - `GetIsGameUIHidden`
- **Calls:**
  - `SpawnSealedCache(Class, Transform, ScavengeSeconds = 30)`
  - `SpawnEscapedCasesSealedCache(Transform)`: uses the escaped case truck's sealed-cache settings.
- **Events:**
  - `OnFirstTruckSurrendered(Truck)`
  - `OnCaseTruckSpawned(CaseTruck)`
  - `OnCasesDelivered(CaseTruck)`
  - `OnCasesEscaped(CaseTruck)`
  - `OnSealedCacheSpawned(Cache)`
  - `OnSealedCacheScavenged(Cache)`
  - `OnSealedCacheDestroyed(Cache)`

### Hooking up the mission

1. **Bind the events.** In the `Convoy5_Cases` or `RecoverCases` stage's start, call Get SurrenderingTruckSubsystem
   and bind:
   - `OnCasesDelivered` → the cases are recovered: complete `RecoverCases`.
   - `OnSealedCacheSpawned` → put the objective marker on the cache. It can fire again after a respawn.
   - `OnSealedCacheScavenged` → the cases are recovered: complete `RecoverCases`.
   - `OnCasesEscaped` → note "cases at Base B".
2. **Base B falls.** When `DestroyBaseB` completes and `GetHaveCasesEscaped` is true, call
   `SpawnEscapedCasesSealedCache` at the command post. `OnSealedCacheScavenged` then completes `RecoverCases`.
3. **Mark the truck.** To mark the truck itself, use `OnCaseTruckSpawned`, or `GetActiveCaseTruck` after convoy 5
   starts.

## Known gaps

- **Stuck/Repath log spam.** The convoy may log `Stuck`/`Repath` for a truck held during its countdown.
  - **Cause:** `AConvoy::UpdateStuck` (`ConvoyColumn.cpp`) still gives the held truck a `WantedSpeed` above zero while
    the truck's own mobility restriction keeps it standing. The truck then also stops holding the column after
    `MaxWaitForStuckTruckSeconds`.
  - **Fix (convoy code):** skip stuck checks and orders for members whose actor is an `ASurrenderingCargoTruck` in
    `ScuttleCountdown`, or add a public `AConvoy::SetMemberHeld(Actor, bool)` that the truck calls in `HoldInPlace` /
    `ReleaseHoldInPlace`.
- **Not tested in play.** All of this compiles, but nothing has been play-tested.
