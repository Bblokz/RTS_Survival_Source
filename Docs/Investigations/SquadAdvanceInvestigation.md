# Squad advance investigation

Date: 2026-10-22

## Findings

The cover policy permits large forward moves without a preferred engagement distance. Aggressive stance feeds
ordinary attack orders into that policy. There is also a squad-wide range-closing path that can disturb soldiers
whose own weapons are already in range. These are separate mechanisms; treating every forward move as a failure
to stop at weapon range would miss the cover case.

### 1. Cover selection can take an already-in-range soldier almost onto the target

`URTSCoverFinderWorldSubsystem::GetAutomaticCoverSearchRadius` gives player 2 a 2500 cm initial cover search,
compared with 1200 cm for player 1. `TryReserveCombatCoverPoint` rejects points beyond the supplied maximum
weapon distance but imposes no minimum distance or limit on how much closer a candidate may bring the soldier.
`ASquadUnit::GetMaximumCoverDistanceToTarget` supplies 90% of weapon range as that maximum.

`FRTSCombatCoverScoring::TryScoreCoverPoint` rewards protection and facing, and subtracts travel cost. Travel cost
saturates at the reposition reference distance (900 cm by default), even during the larger initial search.
Consequently a 2400 cm walk receives the same maximum travel penalty as a 900 cm walk. There is no score term
for keeping an appropriate distance from the enemy.

The initial-selection test supplies a soldier at the origin, a target at X=2600 cm, sideways cover 400 cm away,
and well-facing cover at X=2400 cm. With controlled settings matching the current defaults, it asks the real
subsystem to select and reserve cover, including its real collision trace against the target. It checks that
player 1 selects nearby cover while player 2 selects the forward point, ending just 200 cm from the target.
A control gives the nearby point equally good protection: then the nearby point wins. The policy does not
unconditionally prefer forward movement; it allows substantial closing distance to obtain better protection.

### 2. Better-angle repositioning can also end at point-blank distance

`ASquadUnit::TryRepositionToBetterCombatCover` waits for its evaluation/minimum-hold timers, respects hold-position
stance, and checks that another squad mate is not already walking into cover. It then uses the same combat solver
within a 900 cm radius. Those checks limit frequency, not proximity to the target.

The occupied-cover test supplies a target 1000 cm away and a better-facing point 800 cm forward. The real solver
accepts leaving the current flanked point for a position just 200 cm from the target. The score gain clears the
existing threshold. Another test checks that equally aligned targets at 3000 cm and 200 cm receive identical
cover scores.

`ASquadUnit::OnSpecificTargetInRange` stops only `IdNoAbility_MoveCloserToTarget` movement. Cover is a subordinate
state under `IdAttack`, so this callback deliberately leaves a cover walk active. Reaching weapon range therefore
does not stop a soldier walking to the overly close cover point. Globally stopping cover walks in this callback
would break cover entry; the selected destination needs to be constrained instead.

### 3. One out-of-range member can restart movement for in-range members

`AInfantryWeaponMaster::SpecificEngage` checks each soldier's own range. An out-of-range soldier calls
`ASquadUnit::OnSpecificTargetOutOfRange`, which calls `ASquadController::OnSquadUnitOutOfRange` during an attack
or attack-ground command. The controller projects the enemy's position and calls `GeneralMoveToForAbility`
for the squad with `IdNoAbility_MoveCloserToTarget`.

That method's exemption is `GetCanHoldCoverWhileSquadClosesRange`. It protects an in-range soldier only when
already occupying cover, attacking the same actor as its weapon, and still in range. The tests check that a
soldier in the open or walking into cover is not exempt, even though its real weapon reports it is in range.
Changing to a short-range weapon also removes the occupied-cover exemption.

The range-stop test checks that an in-range notification clears the command path and restores both attack and
attack-ground correctly. The stop exists. The concern is repeated squad-wide reissuing by a different member,
especially with mixed weapon ranges or members at different distances. The guard's backoff resets when the squad
makes progress, so it is not a guarantee against this disturbance during a successful advance.

### 4. Aggressive acquisition inherits explicit-attack cover rules

`URTSTargetAcquisition::IssueAttackClosestVisibleTargetInAggroRange` calls `AttackActor(TargetActor, true)`.
Acquisition searches beyond weapon range: squad range plus 2500 cm for player 1 or 3500 cm for other players.
`ASquadController::GetSquadRange` uses the highest weapon range in the squad.

The ordinary attack command records no distinction here between an autonomous aggressive order and an explicit
player attack order. `GetCoverJudgementTarget` marks either as an attack-order target. If a cover point cannot
engage it, `DecideOnInvalidTarget(true, ...)` immediately leaves cover, bypassing the normal 2.5-second target
change tolerance. Aggressive stance also permits better-cover repositioning where hold position blocks it.
The stance/tolerance test exercises the real stance gate and invalid-target decision separately; the order
propagation between them is established by following the production call sites.

## Suggested fix

1. Add weapon-aware engagement-distance constraints to both initial combat cover selection and repositioning.
   Reject unwanted point-blank destinations and excessive forward displacement before scoring. Use weapon or
   squad role tuning so SMGs/flamethrowers may deliberately close farther than rifles. Preserve a sensible fallback
   when enemies themselves approach, rather than forbidding all cover at short range.
2. Charge for the full initial-search walk instead of saturating its cost at the smaller reposition radius.
   Prefer lateral moves or holding a usable firing position when the protection gain is marginal. Reducing the
   enemy's initial 25 m search is a useful temporary tuning change, but does not fix the missing distance policy.
3. Close range only for members that actually need it, to a valid firing position near their engagement distance.
   Keep in-range members firing whether in cover or in the open. Keep squad cohesion as a separate constraint;
   a short-range member should not repeatedly move the entire squad.
4. Preserve whether an attack was issued autonomously or explicitly. Autonomous orders should retain target-change
   grace and a bounded pursuit policy; explicit attack orders can retain their stronger intent.

Start with the cover destination constraints: they address the tested large advance and the tested 2 m endpoint
without weakening the working range-stop callback. Then address per-member range closing and order provenance.
No gameplay fix is included in this investigation.

## Implemented fix

Implemented on 2026-10-22, one change per suggestion:

1. **Engagement distance.** `TryReserveCombatCoverPoint` now takes the weapon range instead of a maximum distance
   and rejects candidates before scoring with `FRTSCombatCoverScoring::GetIsWithinDistanceLimits`: inside 90% of
   weapon range, at least `M_CombatCoverMinimumEngagementRangeRatio` (0.35) of the range from the target, and no
   more than `M_CombatCoverMaximumAdvanceDistance` (600 cm) closer to it than the soldier stands. A ratio of the
   range lets short-ranged weapons close further than rifles without a weapon-role enum. When the enemy walked
   up, cover no closer than the soldier's current distance stays allowed. Initial selection and repositioning
   share the limits. Zero weapon range still disables them (`CoverCombatTestScenario` relies on that).
2. **Travel cost.** The travel term is no longer capped at the reference distance, so a 2400 cm walk now costs
   about 1.07 instead of 0.4. Walks inside the 900 cm reposition radius score as before, so the
   `RTS.CoverFinder.Combat` tests are unaffected. On score alone the forward point in the initial-cover test
   still wins (0.43 against 0.06); the distance limits are what reject it.
3. **Per-member range closing.** `GetCanHoldCoverWhileSquadClosesRange` became
   `GetMayKeepFiringWhileSquadClosesRange`: every soldier whose own weapon has the ordered actor (attack) or
   ground location (attack ground) in range stays out of the squad's range-closing move, whether in cover, walking
   into cover or in the open. The move destination and range stop are unchanged: movers still stop at weapon
   range through `OnSpecificTargetInRange`. No separate squad-cohesion constraint was added.
4. **Order provenance.** `USquadTargetAcquisition` now calls `ASquadController::IssueAutonomousAttack`, which
   sets `bM_IsIssuingAutonomousAttack` around `AttackActor(Target, true)`. Replacing the queue starts the
   attack synchronously, so `ExecuteAttackCommand` passes the flag to every soldier
   (`ASquadUnit::bM_IsAttackOrderAutonomous`). `GetCoverJudgementTarget` treats an autonomous attack like a
   weapon target. It gets the 2.5 s tolerance, no forced step-out and no judging against an out-of-lane ordered
   enemy. If the attack only started later, it would count as a given order, matching the old behaviour. Team
   weapon guards pass "given": crews take no automatic cover. Pursuit for autonomous orders is bounded only by fixes
   1 and 3; no separate leash was added.

Verified on 2026-10-22 in an isolated copy (only these changes on top of `ad5bde69`), `RTS_SurvivalEditor Win64
Development`. `RTS.Squads.AdvanceDiagnostics` (7) and `RTS.CoverFinder.Combat` (5) all reported `Result={Success}`.
`-CoverFinderValidateCombatCover` on TestCover logged `RESULT PASS failed_checks=0` (12 directions, 9 with cover, 9
facing the enemy, 6 flank repositions). It was not checked in a live battle or with real Soviet squads.

## Tests and scope

Validation completed on 2026-10-22:

- `RTS_SurvivalEditor Win64 DebugGame` built successfully.
- All six tests completed with `Result={Success}`; the editor exited with status 0.
- The initial-cover case selected the 2400 cm forward destination, 200 cm from the target.
- The reposition case accepted an 800 cm forward move; score increased from 0.129 to 1.144.
- Shipping preprocessing produced an empty translation unit, with development automation deliberately enabled.
- Log: `D:\UE5Projects\RTS_Survival\Saved\Logs\SquadAdvanceDiagnostics.log`.

File: `RTS_Survival/Units/Squads/Tests/SquadAdvanceDiagnosticTests.cpp`

Automation prefix: `RTS.Squads.AdvanceDiagnostics`

These began as characterization tests (the list above records the pre-fix run). With the fix they are
regression tests: seven tests that check the enemy takes nearby cover over the 2 m forward point, and a flanked
occupant sidesteps instead of advancing. They also cover the distance limits for rifle and short-range ranges,
including an enemy that walked up and a soldier just beyond the maximum. Other checks: a 24 m walk costs more than
9 m, in-range soldiers in the open or walking to cover are exempt from range closing, and an autonomous attack is
not an attack-order target.

The fixture creates an isolated editor-preview world and native deferred actors. It supplies controlled cover
snapshots, runs the production selection/reservation/scoring functions and target collision traces, and exercises
the range/exemption callbacks. Settings overrides are scoped and restored on every exit. It does not move real
map squads, bake navigation, load Soviet Blueprints, or measure how frequently each path fires in a particular
battle. The findings establish reproducible decision-policy causes, not their relative frequency in a map.

Run after building `RTS_SurvivalEditor Win64 DebugGame`:

```powershell
& 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe' `
  'D:\UE5Projects\RTS_Survival\RTS_Survival.uproject' /Engine/Maps/Entry `
  '-ExecCmds=Automation RunTests RTS.Squads.AdvanceDiagnostics' '-TestExit=Automation Test Queue Empty' `
  -unattended -nullrhi -nosound -nosplash '-DisablePlugins=Fab,BpGeneratorUltimate' `
  '-abslog=D:\UE5Projects\RTS_Survival\Saved\Logs\SquadAdvanceDiagnostics.log'
```

The whole test translation unit and every new friend declaration use
`#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING`. No runtime logging, commands, timers, reflected classes,
test fields, module dependencies or gameplay changes were added. MSVC preprocessing with `UE_BUILD_SHIPPING=1`
and even `WITH_DEV_AUTOMATION_TESTS=1` produces an empty translation unit. The test code cannot enter a Shipping
binary, including a special Shipping target that happens to enable automation.
