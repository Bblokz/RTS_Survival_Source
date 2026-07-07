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
