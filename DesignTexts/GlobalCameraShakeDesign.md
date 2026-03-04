# Global Camera Shake Design (Projectile Weapons + Explosions)

## Scope
- Applies to projectile weapon fire and projectile/explosive impact events.
- Explicitly excludes trace weapons.

## Why this fits current architecture
- Weapon fire events already flow through `UWeaponState::FireSingleShot/FireSingleBurst -> IWeaponOwner::PlayWeaponAnimation`.
- Projectile impacts already converge in `AProjectile::SpawnExplosionHandleAOE` and bounce/hit notifications.
- Optimization already computes `InFOV / OutFOVClose / OutFOVFar` state via `URTSOptimizer` and derived classes.

## Proposed system

### 1) Global runtime singleton
Create a world subsystem:
- `URTSCameraShakeSubsystem : UWorldSubsystem`

Responsibilities:
- Receive fire/explosion shake requests.
- Apply global throttling and aggregation per frame/tick window.
- Start camera shakes on local player camera manager.

### 2) Event input APIs
Provide two lightweight request methods:
- `RequestWeaponFireShake(const FRTSWeaponShakeRequest& Request)`
- `RequestExplosionShake(const FRTSExplosionShakeRequest& Request)`

Each request should minimally carry:
- World location
- Source calibre (mm) and/or TNT grams
- Event type (`WeaponFire`, `Explosion`)
- Optional source actor weak ptr
- Optional precomputed visibility hint (`ERTSOptimizationDistance`)

### 3) Visibility gating strategy (performance)
Use two-stage gating:
1. **Producer-side cheap gate**
   - If owner has `URTSOptimizer`, only enqueue when `InFOV` or when inside an always-shake near radius.
   - If no optimizer exists, enqueue only for high-calibre thresholds and let subsystem decide.
2. **Subsystem-side final gate**
   - Frustum cone + distance check against camera once per queued request.
   - Aggregate nearby events in a short window (e.g. 50 ms) to avoid N shakes from salvo spam.

This avoids expensive per-shot projection checks across all weapons while still being robust.

### 4) Aggregation model
Use a fixed short accumulation window:
- Collect all events in current window.
- Compute weighted intensity from calibre/TNT and distance falloff.
- Fire at most 1-2 shakes per window (e.g. heavy + residual).

This keeps camera readable and cost bounded with many simultaneous projectiles.

### 5) Data-driven project settings
Add a real project settings object (designer editable in Project Settings):
- `URTSCameraShakeDeveloperSettings : UDeveloperSettings`
- `UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="RTS Camera Shake"))`

Suggested settings:
- Global toggles
  - `bEnableWeaponFireShake`
  - `bEnableExplosionShake`
- Thresholds
  - `MinWeaponCalibreForShake`
  - `MinExplosionTNTGramsForShake`
- Distances
  - `MaxShakeDistanceCm`
  - `AlwaysShakeDistanceCm`
- Visibility rules
  - `bRequireInFOVForWeaponShake`
  - `bRequireInFOVForExplosionShake`
- Curves / response
  - `UCurveFloat* WeaponCalibreToAmplitude`
  - `UCurveFloat* ExplosionTNTToAmplitude`
  - `UCurveFloat* DistanceFalloff`
- Frequency / anti-spam
  - `AggregationWindowSeconds`
  - `MaxShakeRequestsPerSecond`
  - `MinTimeBetweenHeavyShakes`
- Shake assets
  - `TSubclassOf<UCameraShakeBase> WeaponShakeClass`
  - `TSubclassOf<UCameraShakeBase> ExplosionShakeClass`
  - Optional high-calibre override classes.

### 6) Integration points
- **Projectile fire (non-trace only):**
  - call subsystem from projectile weapon path (not trace weapon path).
  - ideal place: projectile-specific fire code after successful launch.
- **Explosion / impact:**
  - call subsystem in/near `AProjectile::SpawnExplosionHandleAOE` after impact location is resolved.

### 7) Optimizer integration detail
Add a read accessor to `URTSOptimizer`:
- `ERTSOptimizationDistance GetCurrentOptimizationDistance() const;`

Then producers can provide this state in request payload, so subsystem can skip additional checks for obviously far out-of-view sources.

### 8) Multiplayer ownership
- Only local player camera should shake.
- If this is single-player RTS today, keep local-only path; if multiplayer is added, ensure requests only execute for owning local client camera.

## Benefits
- Scales with many projectile weapons because expensive decisions are centralized and rate-limited.
- Leverages existing optimizer visibility buckets for near-zero extra per-weapon overhead.
- Gives designers full global control in Project Settings without touching code.
