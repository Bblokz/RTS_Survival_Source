# Economy findings: radixite and metal harvest plus conversion only (2026-11-18)

This is an analysis only: no balance value was changed. The options at the end are for discussion.
The harness is described in [EconomySim.md](EconomySim.md).

## Setup

- **Where:** UT_EconomySim, a copy of UT_EnemyAI. Strike Division, Normal difficulty, Arcade balance. No scavenging
  and no units bought.
- **Runs:**
  - Run 5 is the player economy over 30 game minutes.
  - Run 7 repeats it with plain crystals on FieldB and with enemy training that spawns (see section 4).
  - Results are in `Saved/EconomySim/run5` and `run7` of the RTS_Shorts worktree.
- **Where it lives:** everything is uncommitted in the RTS_Shorts worktree (branch `shorts`). Main and all game
  code are untouched.
- **Economy data matches main where it matters:**
  - cost and storage constants are identical;
  - `Ger_Ref_Converter`, `Ger_Bxp_MetalVaultSmelter`, the Pz I and Pz IV harvesters, the resource nodes and the
    enemy controller Blueprint are byte-identical;
  - the nomadic truck Blueprints differ (deploy and visual work).
- **Caveats:**
  - The fifth harvester never delivered in either run, so 5 of 6 harvesters worked. Run 10 found why: it idles
    at a FieldB crystal that is never harvested (section 6c).
  - The map's enemy loadout carpet-bombs the base throughout.
  - In run 7 the harvesters stopped around minute 18 with nodes left: 3190 R on FieldB and 3050 M on the ruin.
    Run 10 showed this is the reachable fields running dry plus that unharvested crystal, not bombing (section 6c).
  - The player figures below therefore use the rates from before the stall (minutes 7–17).
- **The books balance.** Run 7's harvest plus start bank, minus build costs, bank and stock, implies about 1235 VP
  against 1245 logged. So the cargo tracking misses nothing, the fifth harvester truly delivered nothing, and the
  smelter ran at full rate only about 58% of the time it was on.

## 1. What the economy produces

**Harvesters** (Pz I: 150 R or 75 M per trip):

| harvester | rate | round trip |
|---|---|---|
| radixite next to the refinery (18–25 m), while fed | ~295 R/min | ~31 s |
| radixite 33–47 m from the HQ, while fed | ~195 R/min | 39–46 s |
| metal, vault 14–26 m away | 100–130 M/min | 29–46 s |
| average over a whole run (re-routing, thinning fields) | 130–180 R/min or 70–110 M/min | |
| sustained in the best 5 minutes after the build order | 165 R/min (run 5) to 227 R/min (run 7) per radixite harvester, ~100 M/min per metal harvester | |

**Converters** (live `M_Settings`):

| converter | per 10 s | per minute | how many |
|---|---|---|---|
| Refinery converter | 25 R → 5 VP | 150 R → **30 VP** | up to 2 per refinery |
| Metal Vault smelter (Strike Division only) | 25 M → 5 VP | 150 M → **30 VP** | 1 per vault (origin expansion) |

- `SM_OpelRefinery_BuildingMesh` has two expansion sockets, BXP1 and BXP2, and a refinery takes three expansions.
- Nothing in `IBuildingExpansionOwner::GetUnlockedBuildingExpansionTypes` prevents a second converter. The second
  converter is read from the data; the sim built one.
- **The price of +30 VP/min:** a converter (200 R + 100 M) and 150 R/min to run it. Past two per refinery, also a
  new refinery (250 R + 125 M).

**Storage:**

| building | adds |
|---|---|
| HQ | 800 R, 350 M, 250 VP |
| Refinery | +1500 R |
| Kaskade | +750 R |
| Metal Vault | +800 M, +400 VP |

- Training pays per second over the training time (`UTrainerComponent::GetCanPayForItemTick`), so storage never
  blocks a unit purchase.
- Expansions and field defences pay in full at placement.

## 2. The opening (800 R / 300 M start)

| t | event |
|---|---|
| 0:08, 0:18 | two Pz I harvesters (300 R each) |
| 0:55 | refinery truck out; it waited 37 s for radixite |
| 1:19 / 1:37 | refinery deployed / converter on |
| 2:06, 2:46 | 3rd and 4th harvester; they waited 29 s and 40 s, because the converter eats 150 R/min of the same income |
| 3:43 / 4:43 | metal vault (waited 57 s) / smelter on |
| 6:25 to 7:26 | base complete: 6 harvesters, refinery with converter and Kaskade, vault with smelter |

- Radixite sits at 0–100 for the first 3 minutes.
- Vehicle parts banked:

  | time | run 5 | run 7 |
  |---|---|---|
  | 5:00 | 85 | ~50 |
  | 7:00 | 195 | ~150 |
  | 10:00 | 375 | 335 |
  | 30:00 | | 1245 |

- The first medium tank's 200 VP arrives at about 7–8 minutes, and only if nothing else costs VP.

## 3. Diagnosis

1. **Every vehicle part costs 5 radixite, and defences need that radixite too.**
   - What the sim measured directly is the **opening crunch** (section 2).
   - Whether radixite is also short mid-game depends on field quality:
     - at run 5's sustained 165 R/min per harvester, a base buying medium tanks has about 0 R/min left for
       defences;
     - at run 7's 227 R/min it has about 375 R/min left (section 6, "today").
   - The sim's fields are large, uncontested and 18–47 m from a drop-off; real maps are probably worse.

   Counting the radixite burnt for parts, VP are 56–77% of a vehicle's real price:

   | unit | effective radixite price |
   |---|---|
   | Pz IV G | 1400 R |
   | Tiger | 3150 R |
   | E-100 | 6500 R |

   The cost note in `DeveloperSettings::GameBalance::UnitCosts` says VP are 3x as rare as radixite; the converter
   charges 5x.
2. **Parts come slowly and need building out.**
   - One converter plus the smelter gives 60 VP/min: a Pz IV G every 3.3 minutes, a Tiger every 7.5.
   - In the best window radixite piled up: 346 R/min (run 5) and 532 R/min (run 7) left over after conversion, and
     3625 R banked by minute 16 in run 7.
   - So mid-game the base could feed 2–3 more converters, but each costs metal and a socket (two per refinery).
   - Once the player builds them, the 5:1 price is what eats the radixite meant for defences.
3. **Metal is the scarcest harvested resource.**
   - A metal trip brings half as much as a radixite trip.
   - Every defence and expansion costs metal, and the smelter alone eats 150 M/min.
   - Metal never reached half of its storage in either run.
   - The smelter turns 5 M into 1 VP, which is 10 R of harvest time: the worst trade in the game.
4. **The opening is radixite-starved.** The first converter takes 150 R/min, about three quarters of a harvester,
   from the same income that pays for harvesters and trucks.
5. **Breakthrough has no smelter**, so its parts come from refinery converters (and scavenging) alone.

## 4. Enemy side

- **The strategic AI on its own is a steady trickle.**
  - UT_EnemyAI's controller has the C++ defaults: 10 points to start, 3 per minute.
  - The sim gave it the survival Normal training curve: 4 to 15 points per minute over 30 minutes.
  - **Run 7: 15 tanks and 3 squads, 277 points.**
  - Light tanks came first: T-26, BT-7, 4 T-70 and 4 SU-76 by minute 15.
  - Tier 2 followed after minute 19: T-34/76, T-34/76L, T-34/85, T-34E and an SU-85; Red Hammer squads at minute 28.
  - It saves for batches, so units arrive in groups of 2–3 (minutes 12, 15, 19, 23, 25, 28).
- **Bug: without the sim's workaround it trains nothing** (run 5: 0 units in 30 minutes, while it spent its points).
  - It pays for a batch first (`Training_TrySpendTrainingPoints`), then looks for a spawn point.
  - No `Rus_Building_*` Blueprint in `Blueprints/Buildings/BuildingExpansions/RusBuildings` calls
    `InitTrainerAsEnemyTrainerComponent` (string scan of the assets), so the spawn point is the base cluster's centre.
  - That centre is projected onto the navmesh with a **2 cm** extent (`StochasticHelpers::CanProjectNavigable_TrainingLocation`).
    A centroid between buildings almost never lands that close, so the batch is dropped without a message.
  - Main has the same code. Run 7 moved the base points onto the navmesh (test-only).
- **The heavy pressure comes from scripted waves and global abilities.**
  - UT_EnemyAI's controller carries a test loadout, `GA_SovietT34Drop` and `GA_HeavyCarpet_PE8_500Gr`, on short
    cooldowns.
  - The T-34 drop delivers a T-34/85 and two T-34/76s every 12 s: about 340 points per minute, 444 tanks in
    30 minutes. The sim removed them.
  - The carpet bombing ran on the base throughout.
  - Survival Normal's wave budget is about 18 points/min at minute 0, 75 at minute 10 and 163 at minute 20, on top
    of training.
- **Exchange rate:** one point buys about 44 RE of the matching German tank (RE = R + 2 M + 3 VP, the cost note's
  weights).
- **Comparison.** Player figures are medium tanks' worth of parts (Pz IV G) per 30 minutes.

  | player side | source | player : strategic AI alone (18 units, measured) |
  |---|---|---|
  | one converter + smelter, 60 VP/min, about 7 | measured | about 1 : 2.5 |
  | today's mid base built out to 4 converters, about 18 | modelled | about 1 : 1 |
  | option B mid base, about 27 | modelled | about 1.5 : 1 |

  - Against survival waves (Normal, minute 10: about 4 T-34/76 per minute, not measured) the modelled mid base
    today is about 1 : 7, and with option B about 1 : 4.4.
  - **The enemy side is the sim's own construction:**
    - training income from the survival curve, not this map's 3/min;
    - a Research Center added for Tier 2;
    - the spawn workaround for bug 1;
    - the map's T-34 drops removed.
  - So the strategic AI alone is no tuning target: even today's built-out base matches it, and option B overtakes it.
  - The real pressure, and the real tuning target, is scripted waves and global abilities, which this sim did not
    measure.
  - Survival also pays the player, and campaign wave budgets live in mission Blueprints. Which mode and what
    player-to-enemy ratio to tune against is your call, and it picks between the options below.

## 5. Bugs found (not fixed)

1. **Strategic AI training** spends its points before checking where to spawn, and projects onto the navmesh with a
   2 cm extent. Without enemy trainer components it never spawns anything (section 4).
2. **`UResourceConverterComponent::TryApplyAllDeltasAtomic` is not atomic.**
   - Radixite (or metal) is subtracted first, then vehicle parts are added.
   - When parts storage is full, `AddResource` clips the parts and returns false, which raises an error, but the
     input is already gone.
   - A player who is not spending VP loses 25 R every 10 s per converter.
3. **Metal Vault cost:** both pairs are keyed `Resource_Metal` (`CPPGameState.cpp`, "Nomadic Ger Metal Vault"), so
   the 250 R is lost. The sim's queue showed `cost=M125`.
4. **Pz IV F1 training time** is `MediumTankTrainingTime - 20` = 4 s.
5. **Drop-offs stay registered after a plain `Destroy()`.**
   - They only leave `UGameResourceManager` in `BeginDestroy`, at GC.
   - Stale `TRASH_ResourceDropOff_*` entries show up as "Owner invalid for" errors whenever a harvester looks for a
     drop-off.
6. **"Player faction was requested before it was set"** repeats when a map is launched directly.

## 6. Rebalancing options (not applied, for discussion)

**Model basis.** `analyze_economy.py` holds the full tables for Pz IV G, Panther and Tiger.
- Fed with run 7's sustained rates: 227 R/min per radixite harvester, 101 M/min per metal harvester.
- Converters are capped at 2 per refinery; the smelter runs if there is metal for it.
- Each base keeps 150 R + 75 M per minute for defences.
- The rest buys one tank type, with radixite split optimally between the tank and converters.
- Run 5's lower sustained rate (165 R/min) leaves about 0 R/min spare in the "today" row.

**Pz IV G (400 R + 200 VP): one bought every N minutes, and the radixite left per minute beyond the 150 R/min
defence budget:**

| option | early: 4 R + 1 M harvesters, 1 refinery | mid: 6 R + 2 M, 2 refineries, 1 vault | late: 8 R + 3 M, 3 refineries, 2 vaults |
|---|---|---|---|
| today: 5 R → 1 VP, 30 VP/min | 3.3 min, +340 R | 1.7 min, +375 R | 1.0 min, +349 R |
| A: converter 3:1 (15 R → 5 VP / 10 s) | 3.3, +460 | 1.7, +615 | 1.0, +709 |
| B: converter 3:1 at 45 VP/min (15 R → 5 VP / 6.7 s) | 2.2, +310 | 1.1, +315 | 0.7, +259 |
| C: A plus smelter 25 M → 10 VP | 3.3, +460 | 1.7, +615 | 0.8, +649 |
| D: today's converter, vehicle VP costs x0.6 | 2.0, +260 | 1.0, +215 | 0.6, +69 |
| E: today's ratio, converter twice as fast (60 VP/min) | 2.5, +0 | 1.3, +0 | 0.9, +0 |

**Reading the table:**
- **The ratio (A, C) is the defence lever.** It keeps the tank rate, because converter count still caps parts, and
  frees 120–360 R/min: one or two more Pak 40 pits every 2.5 minutes. This is the "enough radixite for defences" part.
- **Throughput (B, D, E) is the tank lever.**
  - E (twice as fast at 5:1) buys tanks with all the radixite: nothing is left for defences, the trap you describe.
  - B (3:1 and 45 VP/min) gets about 50% more tanks and still leaves ~300 R/min.
  - D gets the most tanks, but it shrinks wreck scavenging (40–70% of a unit's VP cost) and touches every unit's data.
- **C's smelter change** only matters once there is metal to spare: in the late base, or on metal-rich maps.

**Also worth deciding:**
- Fix the converter so it does not burn input when parts storage is full (bug 2).
- Fix the Metal Vault cost (bug 3).
- Raise HQ parts storage from 250 to about 400, so one T3 tank can be banked. Low priority, since training pays as it
  goes.
- Fix the strategic AI's spawn bug (bug 1), then re-check enemy training income per difficulty. Today, on this map
  and on any map whose Soviet buildings register no enemy trainer, the AI fields nothing beyond its scripted waves
  and abilities.

## 6b. Option B prototyped in the sim (run 8)

The converter ran at 15 R → 5 VP every 6.67 s (`-EconSimConverter=15,5,6.667`). The director rewrote the live
converter's settings at runtime; `Ger_Ref_Converter` itself was not changed in this run (it was on 2026-11-19, see
section 6c). The build order, enemy and map were the
same as run 7; the smelter stayed at today's rate.

| metric | run 5, today | run 7, today | run 8, option B |
|---|---|---|---|
| converter on (min) | 1.6 | 1.9 | 1.6 |
| build order done (min) | 6.4 | 7.4 | 6.2 |
| VP banked at 5 min | 85 | 50 | **110** |
| VP banked at 7 min | 195 | 160 | **260** |
| VP banked at 10 min | 375 | 335 | **485** |
| VP banked at 15 min | 675 | 640 | **860** |
| VP banked at 30 min | 865 (harvest stalled) | 1245 | **1635** |
| best 5-minute window: VP made per minute | 60 | 60 | **74** |
| best window: R left after conversion per minute | 346 | 532 | 546 |
| enemy units trained (strategic AI) | 0 (bug) | 18 | 24 |

**What it shows:**
- **The first medium tank is affordable sooner.** The first 200 VP arrive at about 6 minutes instead of 7–8.
- **The opening is less starved.** The converter takes 135 R/min instead of 150, and makes half as many parts again.
- **Mid-game parts go up by about a quarter.** The best window rose from 60 to 74 VP/min, and the 30-minute total
  from 1245 to 1635 VP: about 8 instead of 6 medium tanks' worth.
- **Radixite left for defences is unchanged** (~540 R/min in the best window).
- The single converter shows only part of B. Each extra converter now adds 45 VP/min for 135 R/min instead of 30
  for 150, and the section 6 model puts a built-out mid base at a Pz IV G every 1.1 minutes against 1.7 today.

**Caveats:**
- The near fields' harvesters stopped at about minute 17–18 again (the same stall as run 7), so the 30-minute totals
  lean on the converter burning banked stock. Both runs share it, so they compare. Run 10 showed the cause: the
  scenario's fields run dry (section 6c).
- The enemy count differs only by the AI's random picks; it had the same income curve.

## 6c. Option B applied to the Blueprint (2026-11-19, run 10)

`Ger_Ref_Converter`'s `ConversionSettings` default is now 15 R → 5 VP every 6.667 s (it was 25 R → 5 VP every
10 s). It was changed in the main checkout with `Source/Tools/EconomySim/set_converter_rate.py` and copied to the
Shorts worktree. The smelter is unchanged. Only the async spawner references the Blueprint, and nothing derives from
it, so every refinery converter gets the new rate.

Run 10 used the Blueprint as saved, with no override. The converter reported `TickRate=6.667`, Radixite −15 and Vehicle
Parts +5 when it was enabled.

| metric | run 7, old rate | run 8, override | run 10, Blueprint |
|---|---|---|---|
| build order done (min) | 7.4 | 6.2 | 6.2 |
| VP banked at 5 / 7 / 10 / 15 min | 50 / 160 / 335 / 640 | 110 / 260 / 485 / 860 | 120 / 270 / 495 / 870 |
| VP banked at 30 min | 1245 | 1635 | 1795 |
| best 5-minute window: VP made per minute | 60 | 74 | 76 |
| best window: R left after conversion per minute | 532 | 546 | 577 |
| enemy units trained (strategic AI) | 18 | 24 | 25 |

Run 10 matches run 8 within the run-to-run noise, so the saved Blueprint behaves like the prototype. Its 30-minute
total is higher because its fields held more radixite: node amounts are random per run.

**The minute-17 stall: the reachable radixite runs out, and one crystal is never harvested.** The new
`harvester_state.csv` shows:
- By minute 19, every FieldA node, two of the three FieldB crystals and the metal tower were empty. Radixite
  deliveries stopped; the metal ruin kept two harvesters busy until minute 28.
- The third FieldB crystal, at (+6500, −1800) from the HQ, was never harvested. It still held all 2060 R at the end.
  - The fifth harvester, sent there at minute 5, drove up to it and stood idle with no cargo for the rest of the run.
    So "the fifth harvester never delivers" in every run is this crystal, not its spawn point.
  - When the other fields ran dry, the remaining radixite harvesters were sent to the same crystal and idled there
    too.
- Carpet bombing was not the cause.
- **Possible game bug, not investigated:** a harvester can reach a node and stand idle forever without harvesting.
  The unstuck and teleport fallbacks never fire. It may only be this spot on UT_EconomySim; check the node before
  treating it as general.
- From minute 17 on, the 30-minute totals say more about the scenario than about the converter. Compare runs by
  the opening and the best 5-minute window.

**Run 9 was discarded.** Its two harvesters took their harvest orders, but no harvest was ever registered. The player
could not pay for the refinery, so the step timed out at 900 s. The binaries, the map and every other file were the
same as in run 8; the converter Blueprint was the only changed file, and it is not built until later. Run 10 used
the same setup and ran normally. Treat run 9 as an intermittent fault in the harness; the state log will show the
cause if it recurs.

## 7. Suggested direction, to discuss

1. **Converter at 3:1 with a little more throughput (B):** 15 R → 5 VP every 6.7 s, so 135 R → 45 VP per minute.
   *Applied to `Ger_Ref_Converter` on 2026-11-19 (section 6c).*
   - A mid base fields a Pz IV G about every 1.1 minutes and keeps ~300 R/min for defences.
   - Over 30 minutes that is about 27 mediums against the strategic AI's 18 units: the player ahead about 1.5 : 1.
     Today's mid base, once built out to 4 converters, is already about 1 : 1. Both figures are modelled.
   - Against survival-style waves (about 4 T-34 per minute at minute 10) it is about 1 : 4.4, so still "barely
     holding on" through quality, defences and abilities.
   - If B is too generous against the AI-only case, A (3:1 at today's 30 VP/min) keeps today's tank rate and only
     frees radixite for defences.
2. **Smelter at 25 M → 10 VP**, so metal-rich maps or late bases can turn spare metal into parts at a fair rate.
3. **Leave unit VP costs and scavenging as they are**, so scavenging stays the secondary bonus it is meant to be.
4. **Fix bugs 1–3 first.** Then re-run this sim (`run_economy_sim.ps1`) on the chosen option before touching
   mission wave budgets.
