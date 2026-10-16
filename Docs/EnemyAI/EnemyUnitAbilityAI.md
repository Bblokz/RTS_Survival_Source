# Enemy unit ability AI

`UEnemyUnitAbilityAIComponent` (on `AEnemyController`) lets enemy squads and tanks use their own command card
abilities: behaviour abilities (squads and tanks) and grenades (squads). It runs on its own timer, so it also works on
maps where the strategic AI never starts (`StartAfterMissionTrigger`).

Source: `RTS_Survival/Enemy/EnemyUnitAbilityAI/`.

## Settings

Project Settings > Game > **RTS Enemy Unit Ability AI** (`UEnemyUnitAbilityAISettings`, `DefaultGame.ini`).

| Setting | Default | Meaning |
|---|---|---|
| `bEnabled` | true | Turns the system off on every map. |
| `MinDifficultySquadsUseAbilities` | Normal | Squads use abilities from this mission difficulty up. |
| `MinDifficultyTanksUseAbilities` | Hard | Tanks use abilities from this mission difficulty up. |
| `EvaluationInterval` | 0.5 s | Timer interval. |
| `UnitsEvaluatedPerTick` | 6 | Units looked at per tick, round robin. |
| `RosterRefreshInterval` | 3 s | How often new or dead enemy units are picked up. |
| `MinSecondsBetweenAbilityUsesPerUnit` | 4 s | A unit uses at most one ability per this interval. |
| `MinSoldiersInGrenadeArea` | 2 | Anti-infantry grenades need this many player soldiers in the blast. |
| `GrenadeWalkRangeMultiplier` | 1.5 | Off hold position a squad may walk to a target this many throw ranges away. |
| `MinAliveSoldiersToThrow` | 2 | Smaller squads keep shooting instead. |
| `SkipTargetsBelowHealthRatio` | 0.2 | No grenade at a nearly dead vehicle or building. |
| `TargetClaimSeconds` | 5 s | Other squads leave a just-grenaded area alone. |
| `RepairBelowHealthRatio` | 0.6 | Repair behaviours below this health. |
| `CloseRangeFraction` | 0.4 | Close range behaviours need the target within this fraction of weapon range. |

The difficulty is read from the mission manager (`AMissionManager::GetIsGameDifficultyInitialized` /
`GetCurrentGameDifficulty`). Until it is set, or on a map without a mission manager, the system does nothing.

## How a unit is evaluated

1. **Roster.** Every `RosterRefreshInterval` the component collects enemy (player 2) squad controllers and tanks whose
   command card holds a behaviour or grenade ability. Cards fill in after spawning, so units are re-checked every time.
2. **Gates.** A unit is skipped when it used an ability in the last `MinSecondsBetweenAbilityUsesPerUnit`, when it is
   retreating (`UEnemyRetreatController::GetIsUnitRetreating`, a direct-control retreat group, or an `IdRetreat` /
   `IdReverseMove` command), or when it runs anything other than idle, attack, or a movement command in combat.
3. **Repair first**, then a **grenade**, then the other **behaviours**. One ability per evaluation.

### Behaviours (`FEnemyAbilityUsageProfiles::GetBehaviourUsage`)

Behaviours start through `ActivateBehaviourAbility(type, true)`, which does not touch the command queue, so they are
also allowed for units that move, but only while those units are in combat.

| Usage | Behaviours | Used when |
|---|---|---|
| Mobility | DefaultSprint, EngineOverclocking | In combat while moving, or attacking a target the weapons cannot reach yet. |
| Anti-armor | WeakSpotAim, SuperCharge, HighVoltage, ArmorPiercingBullets, APCR_Rounds | Weapons engage a vehicle. |
| Anti-infantry | FlakShrapnelRounds, FlameNapalmAbility, SuppressiveShockline | Weapons engage infantry. |
| Close range | AdrenalineShot | Target within `CloseRangeFraction` of the weapon range. |
| Stationary | OverwatchFireSupport | In combat, not moving and not in a formation (it locks the vehicle in place). |
| Repair | PulseRepair | Health below `RepairBelowHealthRatio`. |
| Any target | everything else | Weapons engage something. |

New behaviour types fall back to "any target"; add them to the switch when they need another situation.

### Grenades (`FEnemyAbilityUsageProfiles::GetGrenadeUsage`)

Only squads that are idle or attacking, in combat, and not walking to a formation waypoint throw.

- **Anti-armor** (`GerAtGrenade`): only at vehicles above `SkipTargetsBelowHealthRatio`.
- **Any target** (bundle charges): vehicles and buildings above that health, or a group of soldiers.
- **Anti-infantry** (everything else, including the molotov): at least `MinSoldiersInGrenadeArea` soldiers in the
  blast; the aim point moves to the middle of the soldiers around the target when that hits more.

The landing spot must be within the throw range when the squad is on hold position, or within
`GrenadeWalkRangeMultiplier` throw ranges otherwise, and outside every area claimed in the last `TargetClaimSeconds`.

The target is the attack command's target, otherwise what the soldiers' weapons are shooting at.

## Keeping attack moves and retreats intact

- **Grenades are inserted before the running command** (`ICommands::ThrowGrenadeBeforeActiveCommand`): the running
  command (for example the attack) is terminated, the grenade runs, and then the attack and anything queued behind it
  (such as the decision tree's `RegisterAsBlackboardIdle`) resume. An idle squad simply goes idle again.
- **Formations read "unit went idle" as "waypoint reached".** Grenades are never used by a unit that is walking to its
  formation waypoint (`UEnemyFormationController::GetIsUnitMovingToFormationWaypoint`).
- **New AI orders wait for a running grenade or aim ability.** The decision tree's `Issue...Order` helpers and
  `UEnemyFormationController::MoveUnitToWayPoint` call `FEnemyOrderQueueHelpers::PrepareOrderBehindRunningAbility`:
  it drops what was queued behind the ability (`ICommands::ClearQueuedCommandsAfterActive`) and appends the new order
  instead of clearing the queue. Retreat orders still interrupt.
- **Retreating units are never given abilities.**

## Map test

`-EnemyAbilityAIValidate` (`Tests/EnemyAbilityAITestSubsystem`, not in shipping) plays a mission map with the
difficulty forced to Ironman so squads and tanks both use abilities. It watches the ability AI, the formation
controller and the retreat controller through non-shipping test delegates. Halfway through it orders
`RetreatAllFormations`. It checks that:

- enemy units used abilities, and at least one grenade was thrown by a formation unit;
- no retreating unit used an ability, and no grenade was thrown while walking to a formation waypoint;
- no unit counted as arrived at its waypoint more than 15 m away from it after a grenade;
- every formation with a grenade-throwing unit advanced, ended, or was still fighting 150 s later;
- the ordered retreat arrived.

```
UnrealEditor-Cmd.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/GerMissions/Ger_MarderStugMission -game
  -unattended -nosound -RenderOffscreen -windowed -ResX=640 -ResY=360 -benchmark -fps=30 -DisablePlugins=Fab
  -EnemyAbilityAIValidate -abslog=<file> -log
```

Optional: `-EnemyAbilityAITestSeconds=<game s>` (default 900) and `-EnemyAbilityAIRetreatAt=<game s>` (default 600).
Grep the log for `RTS_ENEMY_ABILITY_AI_TEST`. The run must be rendered (`-RenderOffscreen`): without fog of war enemy
units never pick targets, so nothing fights.
