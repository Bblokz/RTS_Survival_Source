# WeaponOwner Interface Notes (IWeaponOwner)

This directory contains the `IWeaponOwner` interface used by weapon state classes.

## What this interface is for
`IWeaponOwner` is the bridge between generic weapon-state logic and the concrete actor that owns those weapons.
Weapon states call into this interface to:
- read targeting context (`GetFireDirection`, `GetTargetLocation`),
- ask policy questions (`AllowWeaponToReload`),
- notify gameplay events (`OnWeaponKilledActor`, `OnReloadStart`, `OnReloadFinished`, `OnProjectileHit`),
- and let the owner create and register weapon states (`Setup*Weapon` functions, `OnWeaponAdded`).

Most methods are intentionally `protected` and accessed through friend declarations so implementors do not need to expose these details as part of their public API.

## Concrete behavior reference: `ACPPTurretsMaster`
Use `RTS_Survival/Weapons/Turret/CPPTurretsMaster.*` as the canonical example of how to implement this interface in gameplay code:

- `GetFireDirection` returns cached aim direction state (`SteeringState.M_TargetDirection`).
- `GetTargetLocation` returns targeting data (`TargetingData.GetActiveTargetLocation()`).
- `AllowWeaponToReload` currently returns `true` for turret weapons.
- `OnWeaponKilledActor` verifies whether the killed actor was the current target, then informs turret owner (`ITurretOwner`) or falls back to auto-engage logic.
- `PlayWeaponAnimation` triggers BP animation hook and forwards fire notifications (vehicle feedback + turret owner callback).
- `OnReloadStart` delegates to `ReloadWeapon`, and `ForceSetAllWeaponsFullyReloaded` iterates weapons to force instant reload.
- `OnProjectileHit` forwards bounce/hit feedback to the turret owner.
- Each `Setup*Weapon` function constructs a specific `UWeaponState*`, initializes it, stores it in `M_TWeapons`, and updates turret range.
- Projectile-based setups also connect to projectile manager when available.

## Implementation expectation for future owners
If a new class implements `IWeaponOwner`, mirror the same responsibilities demonstrated by `ACPPTurretsMaster`:
1. Keep ownership of weapon state objects.
2. Keep target/fire-direction data available to weapon states.
3. Forward combat callbacks to the higher-level owner/controller gameplay layer.
4. Keep setup functions responsible for constructing and registering the correct weapon state type.
