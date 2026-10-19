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

- Before adding/changing an ability here, read `Docs/Abilities/Abilities.md` for queue/decoder/`ICommands` flow.
- Validate both command-queue behavior and turret callback behavior in the same change.
- Keep team weapon state transitions explicit; avoid hidden side effects.
- Preserve the distinction between:
  - direct command execution,
  - deferred post-pack/post-deploy execution,
  - internal (non-player) reposition/rotation requests.

## Crew animations (operators on a deployed weapon)

Design: `Source/DesignTexts/Teamweapons/AnimationsAndTWPositionIntegration.md`.

- Designer data lives on `USquadUnitAnimInstance::TeamWeaponCrewMontages` (`FTeamWeaponCrewMontages`): one entry per
  `ECrewPositionType` plus subtype overrides keyed by the **team weapon squad's** `ESquadSubtype`. Montages must use the
  `FullBody` slot. A null montage means "play nothing" and is not an error.
- The controller owns *when*: `FTeamWeaponCrewAnimationState` maps operator index → sorted crew position → role
  (`TryGetCrewPositionForOperatorIndex` is the single source of truth, also used by crew moves and rotation snapping).
- Arming requires `Ready_Deployed` **and** a settled operator: no active path following
  (`ASquadUnit::GetIsPathFollowingActive`), braking finished, and not stuck more than a few metres away. Crew moves
  allow partial paths without goal projection, so operators routinely stop short of a spot next to the weapon
  footprint; arming therefore **snaps** the operator onto its crew position (same landscape-trace teleport the
  rotation flow uses). Do not gate arming on a tight distance or a near-zero velocity: that is exactly what broke
  re-arming after pack → move → deploy.
  Triggers: `IdMove` completion, `SnapOperatorsToCrewPositions`, and `TickCrewAnimationArming` (needed because a unit
  already at its goal never reports move completion). `SetTeamWeaponState` disarms on every state other than
  `Ready_Deployed`; `AssignCrewToTeamWeapon` rebuilds the slots. Armed slots are disarmed again when the operator
  gets a path-following move from anywhere, and the anim instance drops the montage itself when the unit walks, so
  movement that bypasses the state machine (retreat, cargo, evasion) can never leave a crew loop running.
- Looping montages loop by per-instance section chaining (`Montage_SetNextSection`), not by asset settings; the
  loop-ended delegate is only a fallback.
- Reacting roles (loader by default) play once per reload start of weapon index 0 via
  `ITurretOwner::OnTurretWeaponReloadStart`, stretched to the flux-adjusted reload time. `OnFireWeapon` is not used.
- Debug prints: `DeveloperSettings::Debugging::GTeamWeapon_CrewAnimations_Compile_DebugSymbols`.

## Ground contact, pushed crew and tow sway

Doc and test: `Source/Docs/Units/TeamWeaponQuality.md`, `-TeamWeaponQualityValidate` (`Tests/TeamWeaponQualityTestSubsystem`).
Run it after any change to movement, deploy, crew placement or towing.

- The weapon mesh is the actor root and blocks the landscape trace channel. Ground traces for the weapon or its crew
  go through `ATeamWeapon::TryGetGroundHeightAt` (steps through units and the gun); a plain landscape trace at a crew
  position lands on the gun.
- `ATeamWeapon::ConformToGround` tilts the actor to the footprint plane. Gameplay (mover, rotation requests, yaw arc,
  controller copy) must keep reading only the yaw; never copy the tilted rotation to soldiers or the controller.
- Pushed moves do not give soldiers paths. `FTeamWeaponPushedCrewFormation` steers operators onto their crew positions
  and guards onto an arc behind the gun with `RequestDirectMove`; the mover's speed scale makes the gun wait for its
  crew. `SetTeamWeaponState` stops the formation on every state but `Moving`. A soldier with path following active
  (any other order) is left alone until that ends.
- The mover skips path point 0 (the squad's path origin) and turns on the spot above `MaxTurnWhileMovingDegrees`.
  A rotation request still running when a push starts is cancelled; both would turn the gun against each other.
- Towing: `FTeamWeaponTowSwaySimulation` moves the attached actor about the hook each controller tick (visual only;
  the attachment stays). The hitch frame uses the truck actor's rotation, not the socket's. Hooking cancels deploy and
  pack timers, post actions, turns and pushes (`CancelPendingDeployForTow`); deploy/pack timer callbacks return when
  towed. `EndTowSway` after detaching keeps the swung position upright.

## Dig-in (sandbag pits)

Doc and test: `Source/Docs/Units/TeamWeaponDigIn.md`, `-TeamWeaponDigInValidate` (`Tests/TeamWeaponDigInTestSubsystem`).

- `UTeamWeaponDigInComponent` (on `ATeamWeapon`) owns the pit and the protection (+33 % max health, damage multiplier
  x0.8, both taken off by the same factors). The pit mesh is found by gun Blueprint name
  (`SM_TWDigIn_<Gun>`, `_Soviet` copies and Blueprint children share it); there is no per-subtype table because some
  squad Blueprints carry the wrong subtype.
- `ATeamWeaponDigInWall` has a scene root: the mesh rises relative to it while building. Hits on it forward to the gun.
  Its wall runs block navigation through nav modifiers from the mesh's `NavBox_<n>` sockets, not through the mesh.
- The controller owns the command card (Break Cover instead of Dig In; Move, Rotate Towards and Tow off the card) and
  must call `RemoveTeamWeaponDigIn` wherever the gun leaves its pit without Break Cover (tow hook, abandon). Anything
  that would move a dug-in gun (out-of-range reposition) checks `GetIsTeamWeaponDigInActive` first.

## Unit variants

Doc: `Source/Docs/Units/UnitVariants.md` (section 5.2.1).

- The controller resolves its variant in `InitSquadData` (as a squad); the gun does not exist yet at that point.
  `ApplyUnitVariantToTeamWeapon` runs at the end of `OnAllSquadUnitsLoaded`, after `SpawnTeamWeapon`, and fires
  `BP_ApplyUnitVariant` on `ATeamWeapon` directly or once the controller's `URTSComponent::OnUnitVariantApplied` fires.
- An adopted gun receives the event again with its new crew's variant: gun Blueprints must set state, not accumulate.
