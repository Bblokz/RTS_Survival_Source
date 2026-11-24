# Four-player skirmish playtest

Checks the four-player refactor (`FourPlayerRefactorPlan.md`, sections 5D and 6) with real fights: four players,
one group each, and an owner-by-owner matrix of the health their weapons really took off.

Code: `RTS_Survival/Utils/RTSMapTests/FourPlayerSkirmish/FourPlayerSkirmishSubsystem.*` (map tests only, nothing in
shipping). Runner: `Tools/FourPlayerSkirmish/run_four_player_skirmish.ps1`.

## How to run

Build the editor in DebugGame (or Development), then:

```powershell
cd D:\UE5Projects\RTS_Survival\Source\Tools\FourPlayerSkirmish
.\run_four_player_skirmish.ps1                 # FreeForAll, then Teams
.\run_four_player_skirmish.ps1 -Phase Teams    # one phase
.\run_four_player_skirmish.ps1 -Seconds 90 -Side 3000 -At 16796,-15808 -Configuration Development
```

Each phase runs in its own `-game -RenderOffscreen` process on `RTS_ToCopySmall`. Do not use `-nullrhi`, because
the fog of war needs a renderer. The runner prints the `RTS_FPS RESULT` line of each phase, and it exits with code 1
when any phase fails or prints no RESULT line. The output goes to
`Saved/FourPlayerSkirmish/<timestamp>/<phase>/`: `summary.md`, `hits.csv` (one line for each loss of health) and
`skirmish.log`.

Command-line switches (the runner sets them):

| Switch | Default |
| --- | --- |
| `-FourPlayerSkirmish` | turns the subsystem on |
| `-FourPlayerSkirmishPhase=FreeForAll\|Teams` | `FreeForAll` |
| `-FourPlayerSkirmishDir=<dir>` | `Saved/FourPlayerSkirmish/<timestamp>_<phase>` |
| `-FourPlayerSkirmishAt=X,Y` | `16796,-15808` (the duel bench's flat field) |
| `-FourPlayerSkirmishSide=<cm>` | `2600` |
| `-FourPlayerSkirmishSeconds=<game s>` | `120` |

## Setup

Each player gets the same three units, and no unit gets a move or attack order:

- Tank: `Ger_Vehicle_PanzerIV_H`
- Squad: `Rus_Squad_Mosin`
- Standalone turret: `BP_StandaloneTurret_RusBall_50MM` (`AStandaloneTurret`)

`FRTSUnitOwnerSpawning::SpawnUnitForPlayer` spawns every unit. The groups stand on the corners of a square around
the arena centre. Player 1 is at +X+Y, and the players continue counter-clockwise. In each group, the squad stands
6 m toward the centre, the turret on the corner and the tank 6 m outward.

With the default 26 m side, the closest members of two neighbouring groups are about 18 m apart, and the farthest
are about 34 m apart. Both are inside rifle range (about 37 m) and cannon range (about 38-40 m). On the diagonals,
the squads still reach each other but the tanks do not. In the Teams phase, the allies (1+3 and 2+4) are on the
diagonals, so every group has two hostile groups next to it.

The setup gate logs the real `UWeaponState::GetRange()` of every unit and the number of hostile groups that each
player reaches. If a player reaches fewer than two hostile groups, the summary shows a warning.

The human player's units (player 1) keep the `IsInEnemyVision` tag during the fight, as in the vehicle duel bench,
because the AI fog vision tags them late. AI players always see each other.

## What it checks

1. **Setup gate.** The gate runs after 3 s, and only when every squad is fully loaded and every unit has a weapon
   state. If that does not happen within 30 s, the gate times out and the phase fails. The gate checks that each of
   these reports the player that the unit was spawned for:
   - the unit
   - each of its soldiers
   - each turret and standalone turret
   - each weapon state

   Every mismatch is a FAIL reason. The weapon owners of the living units are checked again at the end, because a
   weapon can be created after the gate.
2. **Damage matrix.** `FDamageMapTestHooks` reports every loss of health, and the damage source names the attacking
   player. The matrix starts when the units spawn, because units fire on sight. Damage that no source claimed, such
   as a cook-off or an engine fire, goes into the "unclaimed" row and is not a failure.
3. **Held targets.** Four times a second, the harness reads the target of every turret, hull weapon, infantry weapon
   and standalone turret. A target owned by the same player or an ally is a FAIL reason. The held targets also
   decide which unit gets the credit for a hit. A unit gets the credit when it held the victim as target in the 3 s
   before the hit.
4. **End of the fight.** The fight ends at the first of these:
   - every player has met the damage requirement and it held for 10 s
   - at most one player has units left (the fight ends 3 s later)
   - the time limit is reached

5. **Enemy controllers (plan 5C, seams row C).** Before the groups spawn, players 3 and 4 each get an
   `AEnemyController`. If the map has none for that player, the harness spawns the plain C++ class with
   `SpawnActorDeferred`. Before `FinishSpawning`, it sets `ControlledPlayer` by reflection. It also makes the
   controller quiet: thinking waits for the mission trigger, training, building and direct control are off, and the
   controller gets an empty decision tree. The harness does not use `RTS_BP_EnemyController`, because its defaults
   (decision tree, global ability loadout) could act on the arena. The log names the map's player-2 controller. These
   are FAIL reasons:
   - `GetEnemyControllerForPlayer(3)` or `(4)` does not return that controller.
   - The controller's `GetControlledPlayer()` is wrong.
   - The controller is missing from `GetAllEnemyControllers()`.
   - The argument-less `GetEnemyController()` does not return player 2's controller. If the map has no player-2
     controller, the summary shows a warning instead.

   If the harness cannot spawn a controller, the FAIL reason is `controller_actor_spawn_failed_P<n>`.

   For each controller, the harness then spawns one Pz IV H the way the controller's waves do
   (`EnemyWaveController.cpp`): it calls `ARTSAsyncSpawner::AsyncSpawnOptionAtLocation` itself, with that
   controller's `GetSpawnOwnerOverride()`. No real wave runs, so wave supply, navigation and formations are not part
   of the check. If the spawner has no mapping for the option, the call is `AsyncSpawnClassAtLocation` with the same
   tank class. The tank stands 45 m beyond its
   group's corner, out of weapon range, so it is not in the fight or the unit tables. Within 20 s, the tank, its
   turrets and its weapon states must report the controller's player. The FAIL reasons are
   `controller_spawn_owner_P<n>`, `controller_spawn_timeout_P<n>` (nothing arrived), `controller_spawn_failed_P<n>`
   and `controller_spawn_no_weapons_P<n>`. An early end of the fight waits for these checks. Log lines start with
   `RTS_FPS controllers`.

### Verdict (`RTS_FPS RESULT PASS|FAIL reasons=...`)

| Phase | Pass needs |
| --- | --- |
| FreeForAll | Every player damaged at least **two** other players. **Zero** damage by a player to its own units. |
| Teams {1,3} vs {2,4} | Damage only crosses teams: **zero** damage to its own units or to an ally. Every player damaged at least **one** hostile player, so that a run with no fight cannot pass. |

These checks apply to both phases and are stricter than plan section 6. Each one is a FAIL reason:

- A setup gate mismatch, a setup timeout or a spawn failure.
- A weapon owner mismatch at the end of the fight.
- A weapon that held a non-hostile target.
- Damage claimed by an owner outside 1..4.
- A hit unit whose reported owner is not the player it was spawned for.
- Damage by a player to a non-spawned unit of its own or of an ally.
- A failed enemy controller check (item 5).

`summary.md` has these sections:

- The verdict.
- The teams and relations.
- The setup gate table and reach list.
- The enemy controllers: the registry results and one row for each probe tank.
- The damage matrix. A cell is marked **FRIENDLY** when a player damaged its own units or an ally.
- A table for each player.
- A table for each unit: damage dealt (inferred), damage taken, seconds on target for each player, and time of
  death.
- The warnings.

## Known limits

- The damage hook names the attacking **player**, not the unit. The damage that the table credits to each unit is
  inferred from held targets, but the matrix and the verdict use only the player that the hook names.
- AI units that the human player does not see stay hidden from the human's weapons, as in the real game. If player 1
  damages no one, check the fog first.
- The ability AI of each controller (`UEnemyUnitAbilityAIComponent`) runs on its own timer. So players 3 and 4 can
  now use grenades and behaviours with their groups, as player 2 already could. This happens only when the map has a
  mission manager.
- The runner starts one editor process for each phase. Other headless tests that run at the same time slow them down.

## Per-weapon attribution

Weapon-state traces and projectiles name their firing actor to the damage hook
(`RTS_MAPTEST_DAMAGE_ATTACKER_WEAPON_SCOPE`), so hits are credited to the exact unit ("Exact dmg dealt"). Every round a
weapon fires is counted by the owner of the target it held (`UWeaponState::OnMagConsumed`, a drop of the magazine count
only). The "Weapon kinds" section lists, per kind (tank, infantry, standalone turret), who fired at and hit whom. Free
for all FAILs with `<kind>_weapons_fired_at_only_<n>_players` when a kind's shots, over all four players, reach fewer
than three different players; the fight is kept going until that holds or the time limit.

## Controller attack waves

The controllers of players 3 and 4 each start one attack-move wave of two Pz IV H with `StartWaveFromRequest` (their own
spawning, formation and orders) from 25 m behind their group to the arena centre. FAIL reasons:
`controller_wave_not_started_P<n>`, `controller_wave_no_units_P<n>`, `controller_wave_owner_mismatches_P<n>`,
`controller_wave_never_fired_P<n>`, `controller_wave_never_hit_P<n>`, `controller_wave_fired_at_non_hostile_P<n>`, and
`controller_waves_engaged_only_<n>_players` when both waves together engaged fewer than two players. The fight waits
until each wave has fired at and hit a hostile player, its units died, or 90 s passed.

## Runtime owner change

Two tanks out of the fight change owner at the setup gate (P1 -> P3, P2 -> P1) via `SetOwningPlayerRuntime`. A weapon
trace of every player, with its channel and ignore mask, must stop on the tank exactly when that player is hostile to
its owner, before and after the change; the tank, its turret and weapon states must report the new owner, and no body
may stay on the old side's object channel. FAIL reasons start with `OwnerChange_`.

## Controller strategic AI

The controllers of players 3 and 4 start their own strategic AI think loop. Every 5 s the harness queues a
`FGetPlayerUnitCountsAndBase` on each; the loop sends it with the controller's requester. A result is conclusive when its
squad count equals the squads (with living soldiers) of the controller's hostile players and differs from the human-only
count (when the human is hostile) and, in the teams phase, from the all-other-players count. FAIL:
`strategic_ai_P<n>_no_conclusive_hostile_count_in_<k>_results` (the fight waits up to 60 s for one).

## Controller strategy (decision tree)

The harness gives the player 3 and 4 controllers a one-action decision tree (`USubAction_AttackMoveToPlayerUnits`) with
direct control on and `MinPickedUnitsForAction` 1. Each controller's probe tank is handed to it as an idle unit after its
owner check; the tree orders it to bulks of hostile units found by the controller's own queries. FAIL reasons start with
`controller_strategy_` (`unit_never_moved` below 15 m, `never_fired`, `never_hit`, `fired_at_non_hostile`).
