# Enemy (AI) harvesting (2026-11-24)

The harvest loop (harvester, drop-off, game resource manager) works for every player, not only the human (player 1).
This is the harvest-loop part of the Soviet refinery contract ([../EnemyAI/SovietRefinery.md](../EnemyAI/SovietRefinery.md)
§5); economy decisions in [../EnemyAI/EnemyHarvestEconomy.md](../EnemyAI/EnemyHarvestEconomy.md).

## Rules

| Part | Behaviour |
|---|---|
| `UGameResourceManager` | Drop-offs of any owner register (`FRegisteredResourceDropOff` = drop-off + owner at registration). The async drop-off data carries that owner, so a harvester only finds drop-offs of **its own player**. Only player-1 drop-offs are registered with the human's `UPlayerResourceManager`. `GetCopyOfResourceDropOffsOfPlayer(Player)` lists one player's drop-offs. |
| `UResourceDropOff`, owner 1 | Unchanged: deliveries are credited to the human, expansions change the human's storage, the "+ amount" pop-up shows. |
| `UResourceDropOff`, owner not 1 | Its cargo slots are that player's **stockpile**. `DropOffResources`, `DropOffResourcesFromCrane`, `AddExpansionStorage`, `RemoveExpansionStorage` never touch the human; `GetDropOffAmountStored` / `RemoveResources` / `GetDropOffCapacity` read and spend the stock. No pop-up text. No errors for a missing player resource manager or a missing `IResourceStorageOwner` (the Soviet refinery is an `ABuildingExpansion`). |
| Owner change | The drop-off listens to its owner's `URTSComponent::OnOwningPlayerChanged` and re-registers under the new owner. |
| `UHarvester` | Asks for drop-offs of `GetOwningPlayer()` (its unit's RTS component). The "No Drop-Offs Available!" text and the player-controller-0 lookup are player 1 only. |
| Enemy unit pools | Harvesters (a `UHarvester` component, or subtype `Tank_PzI_Harvester` / `Tank_PzIV_Harvester` / `Tank_T34_Harvester`) never join the AI's pools: `FStrategicAIHelpers::GetIsExcludedFromEnemyUnitPools(Actor)` and `GetIsHarvester(Snapshot)`. Applied in `ICommands::ExecuteRegisterUnitAsBlackboardIdleCommand`, `FBlackboardIdleUnitsResult::SetupResultForPickedEntries` (every pick drops them), the `BlackboardQueries` idle counts, and the damaged-tank retreat gather. |

## Autonomous harvest (AI harvesters)

```cpp
UHarvester::StartAutonomousHarvest(ERTSResourceType ResourceType); // BlueprintCallable
UHarvester::StopAutonomousHarvest();
UHarvester::GetIsHarvestingAutonomously();
```

- No player order: 0.5 s after the call, then every 10 s while the unit is idle, the harvester returns its cargo
  (`ReturnCargo`) or asks the async thread for the nearest non-empty field of the type and issues the normal
  `ICommands::HarvestResource` command on it. The unit needs `IdHarvestResource` on its command card.
- The normal harvest state machine does the rest; when a field runs dry it moves on to the next nearest field of the
  same type. With no field (or no own drop-off) the command ends and the 10 s tick tries again.
- Safe to call before `BeginPlay`. The resource type is kept apart from the state machine's target type; the old 5 s
  idle re-harvest check is skipped for autonomous harvesters.

## Test

`Resources/Harvester/Tests/EnemyHarvestTestSubsystem` (`-EnemyHarvestValidate`, script
`Resources/Harvester/Tests/run_enemy_harvest_test.ps1`):

```
powershell -File Source\RTS_Survival\Resources\Harvester\Tests\run_enemy_harvest_test.ps1 [-At X,Y] [-Configuration Development]
```

On `RTS_ToCopySmall` it spawns, around the radixite field nearest the origin (or `-At`), a plain actor with a
player-2 `UResourceDropOff` and one with a player-1 drop-off, crossed so each harvester starts next to the other
player's drop-off; a player-2 `Ger_Harvester_Panzer1` running `StartAutonomousHarvest(Radixite)` and a player-1 one
under a harvest order. Checks: the player-2 drop-off is not in the human's storage, both deliver to their own
drop-offs only, the player-2 stock rises, the human's radixite changes exactly by the player-1 deliveries, spending
the player-2 stock leaves the human alone, the pool filter drops the harvester; then (with a fresh field spawned
first) its field is drained and it must move on to another field by itself; then all radixite is drained, it must
go idle, and a field spawned afterwards must be found by its 10 s retry. The Pz I's own harvest animation ends after about 30 s (also under
`-nullrhi`); one still running after 60 s is finished by the test (`forced_anim_finish` lines, expected 0).
The player-2 drop-off component is not named `ResourceDropOff`, so its quietness comes from the owner rule. The script also fails when the old drop-off errors appear in the
log. Result line: `RTS_ENEMY_HARVEST_TEST RESULT PASS|FAIL`.
