# RTS_SurvivalShippingGlobalAbilityTests

A Shipping game target for testing the global abilities in a packaged build. Target file:
`Source/RTS_SurvivalShippingGlobalAbilityTests.Target.cs`. Staged package: `E:\RR_test\FInalWindows`.

## What the target changes

Everything sits behind `RTS_WITH_ALL_GLOBAL_ABILITIES` (`GlobalAbilitySystem/Tests/AllGlobalAbilities/AllGlobalAbilitiesUnlock.h`),
which only this target defines. The plain `RTS_Survival` Shipping build keeps every gate.

- The player gets every `EGlobalAbility` that has a template in `DA_GlobalAbilitySystem`, on every map, instead of the
  commander's kit (`ACPPController::SetupGlobalAbilityManager`).
- Nothing blocks a player ability: no mission tier cap, no technology or required unit, no cost (nothing is paid), no
  cooldown (also not a template's starting cooldown) (`UGlobalAbilitiesManager`).
- Missions that hide the global ability panel keep it (`AMissionManager::HidePlayerGlobalAbilityPanelIfNeeded`, the
  controller).
- The panel has 8 slots, so it shows a page of 8 abilities. Console (enabled in this target):
  `RTS.GlobalAbilities.Page` (next), `RTS.GlobalAbilities.Page prev`, `RTS.GlobalAbilities.Page 3`.
- Enemy ability managers are unchanged.

The target also defines `RTS_WITH_MAP_TESTS=1` (the other command-line validators) and
`UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING=1` (a map on the command line is opened). Logging and the console are on.
`bUseChecksInShipping` is deliberately off: UE 5.5 asserts in `RHIInit` with it (no graphics context on the immediate
command list in Shipping), so the target runs the same check-free code as the real game.

## The test runner (-RTSGlobalAbilityTest)

`GlobalAbilitySystem/Tests/AllGlobalAbilities/AllGlobalAbilitiesTestRunner.cpp` (header-less, no UCLASS, compiled
only in this target). It uses the player's own loaded abilities and the player's path: page the panel, broadcast the
enabled item button's `OnClicked`, then `ACPPController::TryHandleActiveGlobalAbilityPrimaryClick` with the ground
location (twice for strip abilities: anchor and end).

1. **Sweep**: waves of `-RTSGATestWave` (default 3) abilities on separate navigable spots around the camera. Each must
   arm on the button, execute on the click, end exactly once within 320 s (spies infiltration is instant and has no
   end broadcast), and: drops must put a player-owned unit/building (or a resource crate) within 45 m of the spot,
   on the ground; strikes get an enemy T-34 at the spot and warn when it takes no damage. Error/ensure lines logged
   during a wave are reported with it (WARN). Payload and targets are killed through the health path afterwards.
2. **Stress** (`-RTSGATestStressSeconds`, default 600 real seconds): random abilities at enemies, own units, random
   ground, the landscape border, outside the landscape and the camera, strips of zero and triple length, bursts of
   one ability 4x on one spot, armed-then-cancelled aims (also with a second button pressed while aiming), aircraft
   and payload (parachuting squads included) killed mid-flight, enemies killed, attack-moves of every player unit,
   pause, time dilation 0.2-4x, garbage collection every frame. Every action is logged and flushed
   (`STRESS_ACTION n name`), so the last line before a crash names it.
3. **Travel**: 6 abilities fired, the map reloaded 4 s later with them in flight, then three abilities fired in the
   fresh world.

Progress goes line by line to `%LOCALAPPDATA%\RTS_Survival\Saved\GlobalAbilityTests\<RunId>.txt`
(`RESULT PASS|WARN|FAIL <name> ...`). A relaunched process continues there; a wave that was open when the process
died is recorded as `FAIL crash_or_hang_while_running`. A mission defeat/victory (game lock) reloads the map and retries
the open wave once. Options: `-RTSGATestMode=Sweep|Stress|Both`, `-RTSGATestOnly=GA_A+GA_B`,
`-RTSGATestShard=i -RTSGATestShardCount=n`, `-RTSGATestRunId=`, `-RTSGATestSeed=`, `-RTSGATestNoTravel`,
`-RTSTestExitOnComplete`.

## Scripts

`Source/Tools/GlobalAbilityTests/`:

- `package_global_ability_tests.ps1` - full BuildCookRun into `E:\RR_test\FInalWindows` (missions, tutorials, survival
  maps, menu, campaign, two test maps); `-CodeOnly` rebuilds the exe and copies it into the stage; `-SkipCook` re-stages.
- `run_global_ability_tests.ps1 -Maps Ger_FirstMission,Ger_Trains [-Fresh] [-Mode Both]` - runs maps one after another,
  relaunches after a crash or a 25-minute hang, keeps each launch's log in `E:\RR_test\GA_logs`.
