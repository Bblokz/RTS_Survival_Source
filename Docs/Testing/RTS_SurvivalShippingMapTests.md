# RTS_SurvivalShippingMapTests

`RTS_SurvivalShippingMapTests` is a separate game target for running packaged-map tests in a build that is as close as possible to Shipping, while still compiling the test-only code.

The target is defined in:

```text
Source/RTS_SurvivalShippingMapTests.Target.cs
```

That target sets:

```csharp
GlobalDefinitions.Add("RTS_WITH_SHIPPING_MAP_TESTS=1");
```

Do not define `RTS_WITH_SHIPPING_MAP_TESTS` in `DeveloperSettings.h`. It needs to be a compile-time target definition so the normal `RTS_Survival` Shipping target does not include the shipping-map-test code.

The target also sets `RTS_WITH_MAP_TESTS=1`. `RTSMapTests.h` defaults that macro to `!UE_BUILD_SHIPPING`, so without it the map smoke test (`-RTSMapSmokeTest`, `-RTSMenuFlowTest`) and the command-line validators (`-UnitVariantValidate`, `-AircraftDeathValidate`, the pit validators, ...) are compiled out of every Shipping executable. With it, this target's Shipping executable runs them too, while the regular `RTS_Survival` Shipping target still compiles them out. It also sets `UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING=1`: a Shipping build otherwise ignores the map URL on the command line (`GameInstance.cpp`) and always boots to the main menu, so `<map> -RTSMapSmokeTest` lines would never reach their map. A guard of the form `#if !UE_BUILD_SHIPPING || RTS_WITH_MAP_TESTS` in game code needs `RTSMapTests.h` included, or targets without the define fail with C4668. A test `.cpp` added for those scenarios must be wrapped in `#if RTS_WITH_MAP_TESTS` (include `RTS_Survival/Utils/RTSMapTests/RTSMapTests.h` first), otherwise the regular Shipping target fails to link.

## Executables

In a staged Windows package, the launcher executable is named after the target:

```text
<PackageRoot>/Windows/RTS_SurvivalShippingMapTests.exe
```

The child executable it launches is:

```text
<PackageRoot>/Windows/RTS_Survival/Binaries/Win64/RTS_SurvivalShippingMapTests-Win64-Shipping.exe
```

The normal shipped game target is still `RTS_Survival`. This test executable exists because it is built from the separate `RTS_SurvivalShippingMapTests` target.

The Enemy AI suite (`-RTSRunEnemyAITests`) and the campaign map tests are compiled only into their own targets, `RTS_SurvivalShippingEnemyAITests` and `RTS_SurvivalShippingCampaignMapTests`. The map test executable starts the game normally when given their switches and never runs them. Build those targets in the same configuration and copy their child executables (with PDBs) next to `RTS_SurvivalShippingMapTests-Win64-<Config>.exe`; they share the package's cooked content.

## Build

Build the target with UnrealBuildTool:

```powershell
& 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Build\BatchFiles\Build.bat' -Target='RTS_SurvivalShippingMapTests Win64 Shipping' -Project='D:\UE5Projects\RTS_Survival\RTS_Survival.uproject' -WaitMutex -FromMsBuild
```

After a code-only change, copy the rebuilt child executable into the staged package if the package already exists:

```powershell
Copy-Item -LiteralPath 'D:\UE5Projects\RTS_Survival\Binaries\Win64\RTS_SurvivalShippingMapTests-Win64-Shipping.exe' -Destination 'E:\RR_test\Windows\RTS_Survival\Binaries\Win64\RTS_SurvivalShippingMapTests-Win64-Shipping.exe' -Force
```

## Run All Nomadic/BXP Scenarios

This runs the currently available shipping map test suite for Nomadic/BXP and exits when done:

```powershell
E:\RR_test\Windows\RTS_SurvivalShippingMapTests.exe -RTSRunMapTest=NomadicBxp -RTSTestExitOnComplete -unattended -log
```

`NomadicBxp` will travel to the test map if needed:

```text
/Game/RTS_Survival/Maps/UnitTests/UT_Nomadics
```

`-RTSRunMapTest=All` also starts this runner. At the time of writing, `NomadicBxp` is the available shipping map test.

## Unit Roster (every unit and variant)

`-RTSRunMapTest=UnitRoster` (or `-RTSRunUnitRosterTests`) starts `Source/RTS_Survival/UnitTests/UnitRosterShippingTestRunner.cpp`, compiled only into this target. It travels to `UT_Nomadics` (override with `-RTSUnitRosterMap=<package>`) and runs one scenario per entry, one at a time:

- every training option the player's async spawner holds (German and Soviet tanks, harvesters, squads, nomadics, aircraft), then
- every non-base variant of `DA_UnitVariants` (tanks and squads; building expansion variants are left to the Nomadic/BXP runner).

Each scenario: spawn through the real spawner path (`AsyncSpawnOptionAtLocation`, variants through `SetPlayerUnitVariant`; aircraft through `FRTSUnitOwnerSpawning`, airborne when the class does not start landed), check the owner and the spawned class, move 15 m, duel a copy owned by player 2 for 20 s (tanks and squads), deploy and pack up again (nomadics), kill both with lethal damage and wait for the death. Error lines logged meanwhile are counted per scenario.

```powershell
E:\RR_test\Tests\Windows\RTS_SurvivalShippingMapTests.exe -RTSRunMapTest=UnitRoster -RTSTestExitOnComplete -unattended -windowed -ResX=1280 -ResY=720 -log
```

Options: `-RTSUnitRosterFilter=<text>` (only entries whose name contains it), `-RTSUnitRosterShard=<i> -RTSUnitRosterShardCount=<n>` (split the roster over parallel processes), `-RTSUnitRosterOrigin=X,Y` (spawn point, default the camera pawn), `-RTSUnitRosterNoDuels`.

Output: one `RTS_UNIT_ROSTER_SCENARIO PASS|WARN|FAIL <name> class=... moved=... dealt=... taken=... deployed=... packed=... died=... errors=N fails=[...] warns=[...]` line per entry, then `RTS_UNIT_ROSTER_RESULT`. The same lines go to `%LOCALAPPDATA%\RTS_Survival\Saved\UnitRosterTests\Run_<stamp>_shard<i>.txt`. FAIL means the unit did not spawn, had the wrong class or owner, could not deploy/pack, or survived lethal damage; WARN means it did not move, dealt and took no damage in the duel, or errors were logged while it ran.

## Useful Options

```text
-RTSRunMapTest=NomadicBxp
```

Starts the Nomadic/BXP shipping map test. `All` is also accepted.

```text
-RTSRunNomadicBxpTests
```

Legacy equivalent for starting the Nomadic/BXP runner.

```text
-RTSTestExitOnComplete
```

Requests process exit after the run finishes or after a travel failure.

```text
-RTSNomadicBxpMaxScenarios=2
```

Limits the BXP scenario count. `0` means no cap.

```text
-RTSUnitAbilityMaxScenarios=50
```

Limits the unit ability stress scenario count. `0` means no cap.

```text
-RTSNomadicBxpNameFilter=SomeName
```

Only runs BXP scenarios whose generated scenario name contains this text.

```text
-RTSNomadicBxpExcludeNameFilters=NameA;NameB;NameC
```

Skips BXP scenarios whose generated scenario name contains any semicolon-separated filter.

```text
-windowed -ResX=640 -ResY=360
```

Optional window settings for local runs.

## Focused Example

Useful when you want a quick BXP sanity pass, while still running the full unit ability stress phase:

```powershell
E:\RR_test\Windows\RTS_SurvivalShippingMapTests.exe -RTSRunMapTest=NomadicBxp -RTSNomadicBxpMaxScenarios=2 -RTSTestExitOnComplete -unattended -windowed -ResX=640 -ResY=360 -log
```

Useful when known content setup issues should be skipped while testing code behavior:

```powershell
E:\RR_test\Windows\RTS_SurvivalShippingMapTests.exe -RTSRunMapTest=NomadicBxp -RTSNomadicBxpMaxScenarios=2 "-RTSNomadicBxpExcludeNameFilters=Ger_Airbase_Truck_C_1.BTX_GerHQPlatform;Ger_Airbase_Truck_C_3.BTX_GerHQPlatform;BXT_SolarSmall;BTX_GerMarderStugFactory" -RTSTestExitOnComplete -unattended -windowed -ResX=640 -ResY=360 -log
```

## Console Command

When already in the supported test map, the runner can also be started through:

```text
RTS.UnitTests.NomadicBxp.Run
```

Passing an integer argument to the console command sets the BXP scenario cap.

## Logs

Packaged Windows runs write the main game log here:

```text
%LOCALAPPDATA%\RTS_Survival\Saved\Logs\RTS_Survival.log
```

Expanded example:

```text
C:\Users\<UserName>\AppData\Local\RTS_Survival\Saved\Logs\RTS_Survival.log
```

Use `-log` to show the live log window while the packaged build runs. The saved log above is the authoritative file to inspect after the run.

Useful markers:

```text
RTS_NOMADIC_BXP_TEST_BEGIN
RTS_NOMADIC_BXP_TEST_TRAVEL
RTS_NOMADIC_BXP_TEST_SCENARIOS
RTS_UNIT_ABILITY_STRESS_BEGIN
RTS_UNIT_ABILITY_STRESS_BEGIN_SCENARIO
RTS_UNIT_ABILITY_STRESS_PASS
RTS_UNIT_ABILITY_STRESS_SKIP
RTS_UNIT_ABILITY_STRESS_RESULT
RTS_NOMADIC_BXP_TEST_RESULT
```

The final result line should look like:

```text
RTS_NOMADIC_BXP_TEST_RESULT PASS ...
```

For PowerShell log scanning:

```powershell
$Log = Join-Path $env:LOCALAPPDATA 'RTS_Survival\Saved\Logs\RTS_Survival.log'
Select-String -LiteralPath $Log -Pattern 'RTS_UNIT_ABILITY_STRESS_RESULT|RTS_NOMADIC_BXP_TEST_RESULT|Fatal error|Unhandled Exception'
```

To check for a specific known error pattern:

```powershell
Select-String -LiteralPath $Log -Pattern 'counter became lower than zero|subtype counter'
```

## Interpreting Results

`unit_attempted`, `unit_skipped`, and `unit_failed` belong to the unit ability stress phase.

Skipped unit ability scenarios are not automatically failures. They usually mean the scenario could not produce a valid target, command, or state for that ability in the current map.

Content or asset setup issues can appear as ordinary log errors during a run. If the final test result is `PASS`, the runner did not consider those errors fatal to the scenario set. Log them separately when they point at missing sockets, missing assets, invalid references, or incomplete Blueprint setup.

## DebugGame package under a debugger

For hunting crashes, package the same target in **DebugGame** (unoptimised game code, checks and ensures on, full PDB) and run the child executable under a debugger. E: is full, so packages go to `D:\RR_test`.

The target sets `BuildEnvironment = Unique` in every configuration. Its `GlobalDefinitions` change the shared build environment, so a DebugGame build would otherwise be refused.

```powershell
& 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Build\BatchFiles\RunUAT.bat' BuildCookRun -project=D:\UE5Projects\RTS_Survival\RTS_Survival.uproject -noP4 -platform=Win64 -target=RTS_SurvivalShippingMapTests -clientconfig=DebugGame -build -cook -iterate -stage -pak -iostore -compressed -archive -archivedirectory=D:\RR_test\ShippingMapTests_DebugGame -nocompileeditor -unattended
```

The test-only maps and the Soviet pits that the pit scenarios spawn are not referenced by any shipped asset, so the normal cook leaves them out. Add them to the test package only:
- `-MapsToCook=<the cooked maps>+/Game/RTS_Survival/Maps/UnitTests/UT_EnemyAI+/Game/RTS_Survival/Maps/TestCover/TestCover+/Game/RTS_Survival/Maps/GerMissions/RedLine`
- `-cookdir=<Content>\RTS_Survival\Blueprints\Buildings\BuildingExpansions\Rus53KPit+...`

The cook has no map list. It cooks the default maps and everything reachable from them by hard or soft references. An asset that only C++ names by path is never reached: settings defaults set in a constructor, widget constructors, test scenarios. `DefaultGame.ini` lists the folders of such shipped assets under `+DirectoriesToAlwaysCook`. Add to that list whenever C++ starts naming a new asset by path.

A rebuilt executable can replace the one in a package only while no reflected layout (`UPROPERTY`, `USTRUCT`) has changed since the cook. Otherwise cooked Blueprints fail on the async loading thread (`FUnversionedPropertySerializer::LoadZero`, `FExportArchive::HandleBadImportIndex`). Other sessions edit the same checkout, so check for headers newer than the cook before swapping, back up the package's exe and PDB, and recook when in doubt.

### Debugger

No cdb or WinDbg is installed. Rider's bundled `lldb.exe` reads the PDBs, and `Source/Tools/Debugging` drives it:
- `run_under_lldb.sh <child exe> <log> <seconds> [game args]` launches the game under lldb. It logs every ensure break and exception with a symbolised stack, steps over ensure breaks, dumps all threads and writes a minidump on a crash, and kills the game after the time limit. Launch the child exe from `Windows/RTS_Survival/Binaries/Win64`, not the launcher.
- `run_test_batch.sh <child exe> <log dir> <batch file>` runs a list of scenarios and writes `summary.txt` with one line per run. Environment variables:
  - `RTS_BATCH_LANES`: runs at the same time (default 1).
  - `RTS_BATCH_RHI`: `-RenderOffscreen` (default) or `-NullRHI`.
  - `RTS_BATCH_CVARS`: low scalability console variables, applied by default.
  - `RTS_BATCH_RESX` / `RTS_BATCH_RESY`: 640x360 by default.
- Run the two lists separately:
  - `shipping_batch_logic.txt` (missions, pits, cover, RedLine, GC stress) with `RTS_BATCH_RHI=-NullRHI`.
  - `shipping_batch_rendered.txt` (the menu-to-victory round trip, field defenses and the focused BXP set) rendered offscreen. The BXP map holds laser weapons; under `-NullRHI` each reports its missing beam every frame and the stack dumps slow the game to about 1 fps.
- On the integrated GPU, a mission map renders at well under 1 fps, so a rendered mission smoke test does not finish.
- The cover scenario (`-CoverFinderValidateTestCover`) logs its result but never quits, so its batch line carries `-benchmark -seconds=300`.

A `-NullRHI` run tests game logic only. Expect these, and do not read them as game bugs:
- Every run crashes at exit in an engine static destructor (`Nanite::FFeedbackManager` → `GPUMessage::FSocket`), after the result line.
- The fog of war never starts, so enemy turrets and infantry never acquire targets (RedLine's `pillboxes_fire_on_assault` fails).
- Niagara never creates a component (`FApp::CanEverRender()` is false), hence errors such as "No laser beam system was set up for the laser weapon!".
- Cooked static meshes load without render data, so `UStaticMesh::GetRenderData()` returns null. Game code that reads mesh vertices must check it (`ANomadicVehicle::InitSmokeLocations` does).

Avast CyberCapture sandboxes a freshly built executable. The audio device enumerator then fails, which crashes at start-up in `WindowsMMDeviceInfoCache.cpp`, and D3D12 cannot open the discrete GPU, so the game falls back to the integrated one. Run with `-nosound`, or add `D:\RR_test` to the Avast exceptions for real-GPU runs.

### Map smoke and menu flow tests

`UMapSmokeTestSubsystem` exists in non-shipping builds only.
- `-RTSMapSmokeTest` starts any map the way the player's "Press to start" does, picks Normal on a difficulty picker, plays for `-RTSMapSmokeTestSeconds=N` (default 180), and logs `RTS_MAP_SMOKE_TEST RESULT` with the error, ensure and fatal counts per log category. It also checks that every living tank, nomadic vehicle, aircraft and building expansion is counted under its own subtype in `UGameUnitManager` (`counter_mismatches`); missions and global abilities read those counters, and a mismatch fails the run.
- `-RTSMenuFlowTest`, started on `/Game/RTS_Survival/Blueprints/GameUI/StartGame/StartGame`, follows the shipped route instead. It presses the main menu's Load Campaign button, launches the first mission on the campaign map, and then smoke-tests that mission.
