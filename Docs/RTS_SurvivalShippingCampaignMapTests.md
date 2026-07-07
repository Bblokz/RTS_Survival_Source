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

Just launch the executable. The harness **navigates to the campaign map itself** — you do not pass a map
URL. The game boots to its front-end menu (which ignores a map argument); the harness primes a valid
player faction, waits a few seconds, and if the campaign map has not loaded it force-opens
`/Game/RTS_Survival/Maps/Campaign/RTS_WorldCampaign` on its own. Once the map settles it runs the full
sweep and exits.

```powershell
E:\RR_test\Tests\Windows\RTS_SurvivalShippingCampaignMapTests.exe -RTSCampaignMapTestsFirstSeed=0 -RTSCampaignMapTestsSeedCount=100 -unattended -windowed -ResX=640 -ResY=360 -log
```

> **Run with a real RHI — do NOT use `-nullrhi`.** The game's front-end menu is UMG-heavy and crashes
> under `-nullrhi` before the harness can navigate to the campaign map. `-windowed -ResX=640 -ResY=360`
> (as above, matching the sibling shipping-test workflow) works. The determinism sweep itself is pure
> game-thread logic and needs no rendering, but the boot path passes through the menu, which does.

Command-line options (all optional):

```text
-RTSCampaignMapTestsFirstSeed=N   First seed to generate. Default 0.
-RTSCampaignMapTestsSeedCount=N   Number of sequential seeds. Default 100.
-RTSCampaignMapTestsNoQuit        Keep the process alive after the sweep (default is to exit).
```

Verified: a 100-seed unattended run (`-RTSCampaignMapTestsSeedCount=100`) completes with 100/100 seeds
generated crash-free in ~30s, and a repeat run reproduces byte-identical per-seed logs and fingerprints.

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

Run the test twice with the **same seed range**, then compare. This script picks the two most recent run
folders and compares only the seeds they have **in common**, so it is safe even if the runs used different
seed counts (a common mistake is comparing a 5-seed run against a 100-seed run — the seeds beyond 4 simply
don't exist in the shorter run):

```powershell
$base = "$env:LOCALAPPDATA\RTS_Survival\Saved\CampaignMapTests"
$runs = Get-ChildItem $base -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 2
$a = $runs[0].FullName; $b = $runs[1].FullName
$bNames = (Get-ChildItem $b -Filter 'Seed_*.log').Name
$common = (Get-ChildItem $a -Filter 'Seed_*.log').Name | Where-Object { $bNames -contains $_ }
$diffs = 0
foreach ($n in $common) {
    if (Compare-Object (Get-Content "$a\$n") (Get-Content "$b\$n")) { "DIFFERS: $n"; $diffs++ }
}
"Compared $($common.Count) common seed(s): A=$a  B=$b"
if ($diffs -eq 0) { "All common seeds byte-identical - determinism holds." } else { "$diffs seed(s) differ." }
```

For a full 100-seed verification make sure both of the two newest runs were 100-seed runs.

Any seed that failed to generate is marked `FAIL` in the summary with its failure reason, and appears in
the `Failed seeds:` line — this is how a crash-free run across all 100 seeds is confirmed.

## Logs

The main game log (with the `LogCampaignMapTest` category lines) is written to:

```text
%LOCALAPPDATA%\RTS_Survival\Saved\Logs\RTS_Survival.log
```

Use `-log` to show the live log window during the run.
