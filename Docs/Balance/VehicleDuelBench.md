# Vehicle duel bench (Realistic vehicle balance)

A headless map test that fights fixed tank matchups head on in **Realistic** vehicle balance and writes down every
shell: what it hit, the plate, the armor and pen, whether it bounced, penetrated, over-penetrated, overmatched
("ARMOR DESTROYED!!") or set off a flank ammo detonation, and the health the victim really lost. Use it to tune
`Docs/Balance/TankLethality.md` numbers (overmatch, plate multipliers, health) and check what a change does.

It never reaches a player: the subsystem body and the projectile hooks it listens to compile only with
`RTS_WITH_MAP_TESTS` (`Utils/RTSMapTests/RTSMapTests.h`), which is off in every shipping build.

## Running it

```powershell
.\Source\Tools\VehicleDuelBench\run_vehicle_duel_bench.ps1                               # all scenarios, 3 repeats
.\Source\Tools\VehicleDuelBench\run_vehicle_duel_bench.ps1 -Scenario Tiger_vs_KV1 -Repeats 10
```

It needs a DebugGame editor build (`-Configuration Development` uses the Development one). The script starts
`RTS_ToCopySmall` with `-game -unattended -RenderOffscreen -VehicleDuelBench` and waits for it to quit. Output lands
in `Saved/VehicleDuelBench/<timestamp>/`:

- `summary.md`: one table per run (every tank: shells, hits, bounces, pens, overpens, armor destroyed, flank
  detonations, damage dealt and taken, hits taken and how many were not frontal, first shot, death time, killer and
  cause), then averages per scenario (wins, average kill time, average hits a victim took, overmatches, bounce share).
- `hits.csv`: one row per shell impact, appended as it happens, with the shell's launch id. An over-penetrating shell
  that hits the same tank twice shows as two rows with the same id.
- `bench.log`: the game log; `RTS_VDB` lines trace the runs.

Switches: `-VehicleDuelBenchScenario=<name>`, `-VehicleDuelBenchRepeats=<n>` (default 3),
`-VehicleDuelBenchAt=X,Y` (arena centre, default 16796,-15808), `-VehicleDuelBenchDir=<dir>`.

**Not `-nullrhi`:** the enemy's fog of war vision needs a renderer. Without one, player 2 turrets never see a target
and the Soviet side never fires.

## How a run goes

1. The bench sets Realistic on the game instance and the game state, and every base unit to its base variant.
2. The German side (player 1) and the Soviet side (player 2) spawn 26 m apart facing each other, in lines with 10 m
   spacing. 26 m is inside the 34.8 m light cannon range, so every gun reaches. Each tank's max health is checked
   against the Realistic tank data; a mismatch fails the run.
3. Tanks open fire on sight at once, so the clock starts at spawn. After 4 s each tank is ordered to attack the one
   opposite it, and a tank whose target died is sent at the nearest living enemy.
4. German units are kept tagged as seen by the enemy. The enemy's fog of war otherwise tags them several seconds late,
   which handed the German side a free first shot.
5. The run ends 3 s after one side is destroyed, or after 150 s. Tanks and wrecks are removed before the next run.

Times in the summary are seconds since spawn. "Shell dmg taken" is health lost to logged shell hits; "Other dmg taken"
is the rest (machine guns, splash, cook-off and engine fires).

## Scenarios

| Name | German side | Soviet side |
|---|---|---|
| `Tigers3_vs_T34_76x3` | 3 x Tiger | 3 x T-34/76 |
| `Tiger_vs_KV1` | Tiger | KV-1 |
| `PzIVG_vs_T34_76` | Pz IV G | T-34/76 |
| `Pz38tRail_vs_T34_76` | Pz 38(t) Railgun | T-34/76 |
| `PzIIF_vs_BA14` | Pz II F | BA-14 |
| `KingTiger_vs_IS2` | King Tiger | IS-2 |
| `E25HunterAmbush_vs_KV1` | E-25 Hunter, Ambush activated at the attack order | KV-1 |
| `Pz38tRailOvercharge_vs_KV1` | Pz 38(t) Railgun Overcharge | KV-1 |

Variants are found in the unit variant data asset by class name (`Hunter`, `Overcharge`). The summary marks whether
the Ambush behaviour really applied. Add a scenario to `GetScenarios()` in
`Units/Tanks/Tests/VehicleDuelBench/VehicleDuelBenchSubsystem.cpp`.

## Code

- `Weapons/Projectile/Tests/ProjectileMapTestHooks.h/.cpp`: `FProjectileMapTestHooks::OnShellLaunched()` and
  `OnArmorHit()`. `AProjectile::SetupProjectileForNewLaunch` and `ArmorCalc_KineticProjectile` broadcast them, guarded
  by `RTS_WITH_MAP_TESTS`, and only do the work while something listens. A flank ammo detonation is recognised by the
  lethal multiplier without destroyed armor. Trace weapons (machine guns) are not reported per hit.
- `Units/Tanks/Tests/VehicleDuelBench/VehicleDuelBenchSubsystem.h/.cpp`: the bench.
- `Source/Tools/VehicleDuelBench/run_vehicle_duel_bench.ps1`: the runner.
