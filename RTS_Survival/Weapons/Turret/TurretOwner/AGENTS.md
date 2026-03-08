# TurretOwner Interface Notes (ITurretOwner)

This directory contains the `ITurretOwner` interface used by turrets (`ACPPTurretsMaster`) to talk back to the actor that owns/mounts them.

## What this interface is for
`ITurretOwner` lets a turret delegate owner-level decisions and feedback:
- ownership/team (`GetOwningPlayer`),
- movement reactions to range transitions (`OnTurretOutOfRange`, `OnTurretInRange`),
- kill attribution and post-kill reactions (`OnMountedWeaponTargetDestroyed`),
- combat feedback (`OnFireWeapon`, `OnProjectileHit`),
- and owner orientation context (`GetOwnerRotation`).

The interface is intentionally protected and used through friend classes (`ACPPTurretsMaster`, `UHullWeaponComponent`) so public owner APIs stay clean.

## Concrete behavior reference: `ATankMaster`
Use `RTS_Survival/Units/Tanks/TankMaster.*` as the canonical implementation example:

- `GetOwningPlayer` reads from `RTSComponent`; logs and falls back if unavailable.
- `OnTurretOutOfRange` asks AI controller to move toward target location (with re-issue timing guard), and updates nav collision settings while repositioning.
- `OnTurretInRange` clears the pending out-of-range move state, optionally stops movement for attack/idle commands, and restores nav collision behavior.
- `OnMountedWeaponTargetDestroyed` routes turret-vs-hull kill notifications to specialized handlers and triggers shared “tank killed actor” response when appropriate.
- `OnFireWeapon` marks unit as in combat and plays fire voice feedback.
- `OnProjectileHit` chooses bounced vs connected voice line.
- `GetOwnerRotation` returns tank mesh rotation (or zero rotator fallback).

## Implementation expectation for future turret owners
If a new mounted platform implements `ITurretOwner`, it should:
1. Provide authoritative owner/player identity.
2. Decide how movement/pathing reacts to turret range state changes.
3. Handle kill/reporting attribution between mounted weapons and platform systems.
4. Provide audio/feedback hooks for fire + impact events.
5. Return stable owner rotation context for turret idle/aim behaviors.
