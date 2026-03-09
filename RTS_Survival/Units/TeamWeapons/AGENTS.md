# Team Weapon Controller Notes (AGENTS.md)

This AGENTS file applies to everything under `RTS_Survival/Units/TeamWeapons/`.

## Purpose

Use this folder when behavior depends on a **crew + mounted team weapon** operating as one unit.
`ATeamWeaponController` is not just a normal `ASquadController` with different stats; it is a state-driven controller that coordinates:
- squad units,
- a spawned or adopted `ATeamWeapon` actor,
- deploy/pack transitions,
- turret-driven range callbacks,
- and deferred command execution.

## How this differs from regular `ASquadController`

`ATeamWeaponController` derives from `ASquadController`, so it still participates in the `ICommands` queue flow, but command execution is specialized:

- **Movement is stateful** (`Ready_Deployed`, `Packing`, `Moving`, `Deploying`, `Ready_Packed`) and may defer actual command execution until pack/deploy transitions complete.
- **Patrol is intentionally unsupported** for team weapons (`ExecutePatrolCommand` immediately completes).
- **Attack/attack-ground are weapon-centered**, not pure per-unit infantry dispatch. The controller stages post-deploy actions and synchronizes operators/guards with the weapon.
- **Out-of-range handling differs**: regular squads move closer in `OnSquadUnitOutOfRange`, while team weapons prioritize weapon-driven reposition logic and can ignore per-soldier out-of-range flow when crewed.
- **Idle behavior differs**: on idle, team weapons can auto-deploy again depending on current state and queued/deferred actions.

In short: regular squads are “unit-first”; team weapon squads are “emplacement-first”.

## Team weapon actor lineage and why it matters

`ATeamWeapon` is derived from `AEmbeddedTurretsMaster` and implements `IEmbeddedTurretInterface`. This means the team weapon is treated as an **embedded turret owner/host context**, not a simple infantry weapon wrapper.

Practical implications:
- Rotation and pitch are driven through embedded turret mechanics and interface callbacks.
- Yaw/pitch arc constraints, base turning behavior, and animation hooks are turret-system responsibilities.
- Team weapon control therefore depends on both squad logic and embedded turret logic being consistent.

When changing aiming, rotation, fire events, or in-range/out-of-range behavior, inspect both:
- team weapon controller logic,
- and embedded turret behavior/interface expectations.

## ITurretOwner integration complications

`ATeamWeaponController` implements `ITurretOwner`, so turret events feed back into command/state flow:
- `OnTurretOutOfRange` may trigger controlled repositioning and deferred re-engagement.
- `OnTurretInRange` may restart deploy/engage steps.
- `OnMountedWeaponTargetDestroyed` must reconcile turret kill notifications with the currently active queued command target.

This creates a two-way dependency:
1. `ICommands` drives high-level intent.
2. Turret callbacks can force state transitions or command continuation logic.

Be careful not to treat turret callbacks as cosmetic notifications; they are part of core command progression.

## ICommands usage through squad controller inheritance

The controller still executes abilities through the inherited `ICommands` queue (`UCommandData` / `DoneExecutingCommand`).
However, for team weapons, many command handlers are wrappers around deferred actions:

- command requested,
- team weapon may need to pack/deploy/rotate,
- then deferred action is issued (`post-pack` / `post-deploy`) at the right state boundary.

So if you add/change abilities here, verify:
- queue progression still finishes (`DoneExecutingCommand` paths),
- deferred actions are reset when invalidated,
- and state transitions cannot strand commands.

## Ability implementation differences vs regular squads

Team weapon abilities are generally **state-gated** and **crew-role-aware**:
- operators and guards may receive different orders or restrictions,
- some abilities are delayed until deployment completes,
- dig-in/break-cover integrates with team weapon hull/rotation lock behavior,
- and unsupported abilities must complete safely without stalling queue flow.

Regular squad ability handlers often fan out directly to each `ASquadUnit`.
Team weapon handlers frequently coordinate crew assignment, movement mode, turret state, and deferred command payloads before/after unit-level execution.

## Working guidance for future changes

- Before adding/changing an ability here, read `Docs/Abilities.md` for queue/decoder/`ICommands` flow.
- Validate both command-queue behavior and turret callback behavior in the same change.
- Keep team weapon state transitions explicit; avoid hidden side effects.
- Preserve the distinction between:
  - direct command execution,
  - deferred post-pack/post-deploy execution,
  - internal (non-player) reposition/rotation requests.
