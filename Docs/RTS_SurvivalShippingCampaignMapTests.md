# RTS_SurvivalShippingCampaignMapTests

`RTS_SurvivalShippingCampaignMapTests` is a separate game target for stress-testing the **campaign map backtracking generator** across many deterministic seeds, in a build that is as close as possible to Shipping while still compiling the test-only harness.

It is the campaign-generation counterpart to [`RTS_SurvivalShippingMapTests`](RTS_SurvivalShippingMapTests.md) (which tests Nomadics/BXPs/infantry/vehicles). The two are independent targets with independent macros.

The target is defined in:

```text
Source/RTS_SurvivalShippingCampaignMapTests.Target.cs
```

That target sets:

```csharp
GlobalDefinitions.Add("RTS_WITH_CAMPAIGN_MAP_TESTS=1");
```

Do **not** define `RTS_WITH_CAMPAIGN_MAP_TESTS` in `DeveloperSettings.h`. It must be a compile-time target definition so the normal `RTS_Survival` Shipping target does not include any of the harness code.

## Guaranteed zero impact on the regular game

The harness lives entirely in:

```text
Source/RTS_Survival/WorldCampaign/CampaignGeneration/MapTests/CampaignMapTestRunner.h
Source/RTS_Survival/WorldCampaign/CampaignGeneration/MapTests/CampaignMapTestRunner.cpp
```

Every line of both files is wrapped in `#if RTS_WITH_CAMPAIGN_MAP_TESTS ... #endif`. In any other target the macro is undefined, so both translation units compile to nothing: no UObjects, no console command, no auto-run ticker, no static registration. No existing source file was modified to add this feature, so `RTS_Survival`, `RTS_SurvivalEditor`, and `RTS_SurvivalShippingMapTests` produce identical game code with or without this target present.

There are deliberately **no UObjects** in the harness, so Unreal Header Tool never generates reflection code for it.

## Executables

In a staged Windows package the launcher executable is named after the target:

```text
<PackageRoot>/Windows/RTS_SurvivalShippingCampaignMapTests.exe
```

The child executable it launches is:

```text
<PackageRoot>/Windows/RTS_Survival/Binaries/Win64/RTS_SurvivalShippingCampaignMapTests-Win64-Shipping.exe
```

## Build

```powershell
& 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Build\BatchFiles\Build.bat' -Target='RTS_SurvivalShippingCampaignMapTests Win64 Shipping' -Project='D:\UE5Projects\RTS_Survival\RTS_Survival.uproject' -WaitMutex -FromMsBuild
```

Because the Shipping configuration uses a **Unique** build environment with a distinct global definition, the first build of this target recompiles engine modules for that environment; subsequent code-only rebuilds are fast.

## Run

The test runs the campaign map `RTS_WorldCampaign` (`/Game/RTS_Survival/Maps/Campaign`). Point the executable at it, then let the harness take over.

### Unattended (recommended)

Once the campaign map is loaded and has settled (~3s), the harness automatically runs the full sweep and then exits:

```powershell
E:\CampaignTest\Windows\RTS_SurvivalShippingCampaignMapTests.exe /Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign -RTSCampaignMapTestsFirstSeed=0 -RTSCampaignMapTestsSeedCount=100 -unattended -log
```

Command-line options (all optional):

```text
-RTSCampaignMapTestsFirstSeed=N   First seed to generate. Default 0.
-RTSCampaignMapTestsSeedCount=N   Number of sequential seeds. Default 100.
-RTSCampaignMapTestsNoQuit        Keep the process alive after the sweep (default is to exit).
```

### Console command (when already on the campaign map)

```text
RTS.WorldCampaign.RunMapTests [FirstSeed] [SeedCount] [Quit]
```

Examples:

```text
RTS.WorldCampaign.RunMapTests            // seeds 0..99, stays open
RTS.WorldCampaign.RunMapTests 0 100 Quit // seeds 0..99, exits when done
RTS.WorldCampaign.RunMapTests 500 25     // seeds 500..524
```

Under the hood, each seed calls the existing deterministic entry point
`AGeneratorWorldCampaign::GenerateAndValidatePrunedWorldForSeed`, so the harness exercises the exact
generation + pruning path the real game uses, one seed after another, logging every result.

## Output

Results are written under the project Saved directory:

```text
<ProjectSaved>/CampaignMapTests/Run_<YYYYMMDD_HHMMSS>/
    Seed_000000.log
    Seed_000001.log
    ...
    Summary.txt
```

For a packaged Windows build `<ProjectSaved>` expands to:

```text
C:\Users\<UserName>\AppData\Local\RTS_Survival\Saved\CampaignMapTests\Run_<stamp>\
```

### Per-seed log (`Seed_XXXXXX.log`)

A canonical, sorted, whitespace-stable dump of one generation:

```text
FINGERPRINT 1A2B3C4D
SEED 7
STATE_SEED 7
RESULT SUCCESS
ANCHORS 42
CONNECTIONS 58
PLAYER_HQ key=... loc=1200,-3400,0
ENEMY_HQ  key=... loc=8800,5100,0
ENEMY_ITEMS 9
ENEMY key=... loc=... type=EnemyBunker
...
NEUTRAL_ITEMS 5
NEUTRAL key=... loc=... type=...
MISSIONS 6
MISSION key=... loc=... type=...
ALL_ANCHORS 42
ANCHOR key=... loc=...
```

Everything is sorted by anchor GUID and coordinates are rounded to whole units, so two runs of the
**same seed** produce byte-identical files when determinism holds.

### Summary (`Summary.txt`)

Header with succeeded/failed counts and the list of any failed seeds, followed by one row per seed:

```text
Seed      Result Anch   Conn   Enemy  Neut   Miss   FingerPr   Note
0         OK     42     58     9      5      6      1A2B3C4D
1         OK     40     55     8      5      6      9F0E1122
...
```

## Verifying determinism

The `FINGERPRINT` is a CRC32 of the canonical per-seed dump. To confirm the generator is deterministic,
run the sweep twice and compare:

- The **fingerprint for a given seed must be identical across runs.** Any seed whose fingerprint changes
  between runs is non-deterministic and worth investigating.
- The `Summary.txt` fingerprint column gives an at-a-glance diff; the `Seed_XXXXXX.log` files give the
  exact placement that differed.

```powershell
# Compare two runs seed-by-seed
$A = 'C:\Users\<User>\AppData\Local\RTS_Survival\Saved\CampaignMapTests\Run_A'
$B = 'C:\Users\<User>\AppData\Local\RTS_Survival\Saved\CampaignMapTests\Run_B'
Get-ChildItem $A -Filter 'Seed_*.log' | ForEach-Object {
    $diff = Compare-Object (Get-Content $_.FullName) (Get-Content (Join-Path $B $_.Name))
    if ($diff) { "DIFFERS: $($_.Name)" }
}
```

Any seed that failed to generate is marked `FAIL` in the summary with its failure reason, and appears in
the `Failed seeds:` line — this is how a crash-free run across all 100 seeds is confirmed.

## Logs

The main game log (with the `LogCampaignMapTest` category lines) is written to:

```text
%LOCALAPPDATA%\RTS_Survival\Saved\Logs\RTS_Survival.log
```

Use `-log` to show the live log window during the run.
