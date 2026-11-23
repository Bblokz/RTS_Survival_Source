# Player economy simulation (RTS_EconomySim)

A headless simulation of the player's economy against the default enemy AI, for balancing. It changes no game data.
The findings of the first run and the rebalancing options are in [EconomyFindings.md](EconomyFindings.md).

## Not in the shipping binary

- The code is its own module, `Source/RTS_EconomySim`. Only `RTS_SurvivalEditor.Target.cs` lists it, in
  `ExtraModuleNames`. It is not in the `.uproject`, and no game, shipping or test target builds it.
- Nothing loads the module by itself. The runner passes `-ExecCmds="Module Load RTS_EconomySim"`, and without
  `-EconSim` on the command line the module stays idle.
- It uses only hooks that already exist under `RTS_WITH_MAP_TESTS`, which compiles out of shipping:
  - `ACPPController::MapTest_PlaceLoadedBxpAtLocation`
  - `ANomadicVehicle::ShippingTest_ForceFinishConversionToBuilding`
- The rest is public game API plus reflection reads of `UPROPERTY` members, for example the converter's
  `M_Settings` and `bM_Enabled`. No `RTS_Survival` source file was changed for it.
- The test map `Content/RTS_Survival/Maps/UnitTests/UT_EconomySim.umap` is a copy of `UT_EnemyAI`. The folder is
  gitignored, like the other unit-test maps.
- Opened in the editor, the map still shows UT_EnemyAI's actors: the director clears them and lays out the economy
  scenario at runtime, so every run starts identically. Edit the layout in `AEconSimDirector::BuildScenario`.

## Running it

Run from PowerShell (Git Bash rewrites `/Game/...` paths):

```powershell
.\Source\Tools\EconomySim\run_economy_sim.ps1 -Minutes 30
.\Source\Tools\EconomySim\run_economy_sim.ps1 -Minutes 5 -Extra '-EconSimNoProtect'
.\Source\Tools\EconomySim\run_economy_sim.ps1 -Minutes 30 -Extra '-EconSimConverter=15,5,6.667'
python .\Source\Tools\EconomySim\analyze_economy.py Saved\EconomySim\<run>
python .\Source\Tools\EconomySim\compare_runs.py Saved\EconomySim\<baseline> Saved\EconomySim\<variant>
```

**What-if rates.** `-EconSimConverter=In,Out,Seconds` and `-EconSimSmelter=In,Out,Seconds` rewrite the live
converter's or smelter's tick: input per tick, vehicle parts per tick, tick interval. The director applies the rate
when it enables conversion, then restarts the timer. Blueprints and game data stay as they are, so a rebalancing
option can be measured before anyone edits it. `context.md` records the override.

**Reading or changing the live converter rate.** `set_converter_rate.py` reads `Ger_Ref_Converter`'s
`ConversionSettings` default, or writes it when `CONVERTER_RATE=In,Out,Seconds` is set, then compiles and saves the
Blueprint. Run it in an editor commandlet while no editor has the asset loaded:

```powershell
$env:CONVERTER_OUT = "$env:TEMP\converter.json"   # optional JSON copy of the result
& 'D:\UnrealEngine\UE5.5_ReleaseBranch\UnrealEngine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
    D:\UE5Projects\RTS_Survival\RTS_Survival.uproject -run=pythonscript `
    -script=<path>/Source/Tools/EconomySim/set_converter_rate.py -unattended -nosplash -NoLiveCoding `
    -DisablePlugins=Fab,BpGeneratorUltimate
```

- The process needs a DebugGame editor build. Pass `-Configuration Development` for a Development build.
- It is a `-game -RenderOffscreen` process. Do not use `-nullrhi`: the enemy's fog-of-war vision needs a renderer.
- It exits by itself when the simulated minutes are up.

## What the director does

1. **Starts the game.** It presses Start, clicks Normal on the difficulty picker and unpauses. The faction is
   Strike Division, which is the game's fallback when nothing was picked.
2. **Lays out the scenario** at the map's `RTS_PLayerStart`, (-21595, -15029):
   - Removes the map's pre-placed units on both sides, its nomadic trucks and its resource nodes. A destroyed truck's
     drop-off is deregistered first; otherwise its storage stays with the player.
   - Spawns three resource fields:
     - FieldA: 2 large and 3 small radixite crystals, 30 to 45 m from the HQ.
     - FieldB: 3 large crystals, next to the refinery site.
     - Metal: a metal tower and a ruin.
   - Spawns a Soviet Research Center in the enemy base, which unlocks Tier 2 for the AI.
   - Spawns the German HQ truck and deploys it.
   - Grants the start bank (`-EconSimStart=R,M,VP`; default 800 R, 300 M, 0 VP, which is HQ storage).
3. **Runs a build order through the real paths:**
   - Units are queued on the HQ trainer, which pays per second.
   - Nomadic trucks are deployed with `CreateBuildingAtLocation`. The montage is force-finished, because unattended
     runs never finish it.
   - Expansions are placed with the placement click, which pays. The preview is aimed at the socket or origin by
     reflection, because there is no cursor.
   - Conversion is enabled with the expansion's Enable Conversion command.

   The order is:
   1. Pz I harvester x2 on FieldA
   2. Refinery, then the converter
   3. Pz I harvester on FieldB, Pz I harvester on Metal
   4. Metal Vault, then the smelter
   5. Pz I harvester on FieldB, Pz I harvester on Metal
   6. Kaskade

   No units are bought and nothing is scavenged.
4. **Measures** until the time is up:
   - **Harvester cargo** every 0.25 s, turned into drop-offs, delivered amounts and round-trip times.
   - **Stocks and storage.**
   - **A resource "bank".** After the build order, radixite and metal above half of storage are moved into a
     virtual bank. Vehicle parts are banked from the start. This keeps full storage from hiding income or wasting
     conversion.
   - **Enemy units** as they spawn, with type, subtype and place, plus the AI's training points, tech levels and
     income per minute.
   - **Player protection.** Player units and buildings take 2% damage and are healed every second (switch off
     with `-EconSimNoProtect`). This keeps an attack from cutting the income measurement short. The health it
     restores is logged as the damage the enemy would have dealt.
5. **Controls the enemy:**
   - The strategic AI (the map's decision tree) keeps running.
   - Test-only workaround: every 2 s the director moves the AI's base points (`EnemyBasePoints` on its blackboard)
     onto the navmesh. Its training otherwise spends points and spawns nothing; see EconomyFindings.md, bug 1.
     `-EconSimRawEnemyBasePoints` turns this off.
   - Its training income follows the survival Normal curve (`SurvivalWaveDefaults::BaseStrategicTrainingPointsPerMinute`:
     4 / 8 / 12 / 18 points per minute at minutes 0 / 10 / 20 / 40). Set `-EconSimEnemyIncome=N` for a fixed rate,
     or 0 to keep the map's 3.
   - The map's mission waves are starved: wave supply is zeroed every frame and the wave controller's and mission
     system's timers are cleared.
   - UT_EnemyAI's enemy controller also carries a test ability loadout: `GA_SovietT34Drop` and
     `GA_HeavyCarpet_PE8_500Gr` with short cooldowns. The T-34 drop delivers a T-34/85 and two T-34/76s every 12 s
     along the south edge.
   - Any enemy unit that appears south of `-EconSimMissionLaneY` (default -20000) is counted as a wave/drop unit
     and removed. No Soviet building or strategic-AI base point is near that lane.
   - The carpet bombing is not stopped. It shows up as "damage absorbed" and can break harvester tracks, which the
     protection does not repair.
   - Use `-EconSimKeepMissionWaves` to let all of this run.

## Outputs (`Saved/EconomySim/<timestamp>/`)

| file | contents |
|---|---|
| `context.md` | faction, difficulty, balance mode, node totals and per-harvest amounts, the enemy's training settings |
| `timeline.csv` | every 10 s: stocks, storage, bank, harvested totals, converters on, damage absorbed, enemy points/units/tech/income |
| `harvest_drops.csv` | every drop-off: harvester, field, resource, amount, round trip |
| `harvester_state.csv` | every 30 s per harvester: position, speed, cargo, drops, target node, path-following status |
| `enemy_units.csv` | every enemy unit: time, origin (`trained`, `wave`, `preplaced`), class, type, subtype, x, y |
| `events.csv` | build order steps, deploy and placement results, warnings |
| `summary.md` | per-harvester table, nodes left, live converter settings, the enemy at the end |
| `analysis.md` | written by `analyze_economy.py`: measured rates, steady state, enemy pressure, what-if model |

## Pitfalls found while building it

- **Destroying pre-placed trucks.** `Destroy()` skips the death path, so the truck's drop-off stays registered:
  the player keeps its storage, and harvesters drive to the dead drop-off ("Owner invalid for:
  TRASH_ResourceDropOff"). Deregister it with `UPlayerResourceManager::RegisterDropOff(DropOff, false)` first.
  Do not also call `UGameResourceManager::RegisterResourceDropOff(..., false)`: in run 6 that stopped every
  harvester from ever dropping off. The leftover registry entries only cost a burst of errors.
- **Regrowing crystals.** In run 5 the `Resource_Radixite_Crystal_LargeWithGrowth` field's harvesters stalled
  early; one never delivered. The cause was not isolated; carpet bombing is the other suspect. The scenario now uses
  plain large crystals.
- **The fields run dry at about minute 17.** The FieldB crystal at (+6500, −1800) from the HQ is never harvested:
  the harvester sent there idles beside it for the whole run, and the others join it once the rest is empty (run 10's
  `harvester_state.csv`). Compare runs with the opening and the best 5-minute window rather than the 30-minute
  totals. For longer runs, move that crystal and add nodes in `BuildScenario`.
- **An intermittent no-harvest start.** In run 9 both opening harvesters accepted their orders but never delivered.
  The rerun with the same files was normal. If the refinery step times out, read `harvester_state.csv`.
- **Rally move cancels orders.** A freshly trained unit first takes the trainer's rally move, which cancels a deploy
  or harvest order given on the spawn tick ("terminate building command as truck"). Order 4 s later and re-issue
  while the unit idles.
- **Comma-separated switches.** `FParse::Value` into an `FString` stops at the first comma unless
  `bShouldStopOnSeparator` is false; the first option-B run silently ran with the Blueprint rate because of it.
- **The player start is gone in `-game`.** `RTS_PLayerStart` no longer exists when the module joins the world; the
  director falls back to the map's known start.
- **The T-34 trio is not a wave.** Zeroing the wave supply each frame and clearing the wave controller's and
  mission's timers never stopped it, because it is the enemy's `GA_SovietT34Drop` global ability (found by scanning
  the umap for the controller's loadout). The director removes what appears in its drop lane instead. Removing the
  abilities from the loadout would be the cleaner fix next time.
