# Ore Mountains: Soviet sites (stand-ins from existing content)

Script: `Source/Tools/OreMountainsMission/place_soviet_sites.py` (built 2026-11-23). It places Base A, Base B, the
radio outpost, the weighbridge checkpoint and the battery pits in `Ger_2ndOreMountains` from Rus buildings, BXPs and
props that already exist. The new site models are still waiting for the owner's pick from the concept sheets
(`Saved/OreMountains/sites/v01/`). When a model is picked, swap its asset in the plan and run the script again.

See also: [OreMountainsMission.md](OreMountainsMission.md) and [OreMountainsMap.md](OreMountainsMap.md).

## Running it

The script runs on the open map in the editor; it does not save the map.

```python
import importlib, sys; sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/OreMountainsMission")
import place_soviet_sites as pss; importlib.reload(pss)
report = pss.run()                  # every site
pss.run(only=["Checkpoint"])        # one site: BaseA, BaseB, Radio, Checkpoint, Battery
pss.check()                         # report only: counts, tags, owners, float/sink, corridors, overlaps
```

- **Re-runnable.** Every actor it places is labelled `ORE_Site_<Site>_<Name>` and sits in the folder
  `OreMountains/SovietSites/<Site>`. The one exception is the radio mast, labelled `ORE_BaseB_RadioMast`.
- **What a re-run does.** It moves and resets the actors that already exist under their labels, spawns the missing
  ones, and deletes `ORE_Site_*` actors of that site that are no longer in the plan. Because existing actors are kept,
  references to them from other actors survive (for example a route's `RadioPostActor`).
- **Ground.** Positions are cm from the map centre (+X north, +Y east). Every actor is seated on the landscape by line
  traces over its mesh footprint: the lowest point, minus 5 cm. Hits on anything other than the landscape are ignored.
- **The `run()` report.** It logs `ORE_SITES ...` lines and returns a dict. Anything listed under `problems` needs a
  look: it floats or sinks, it is in a keep-clear corridor (road, ammo track, counterattack spawn, knoll ramps, spur
  slab, gate), or it overlaps another solid actor.
- **What `check()` compares against.** Only the script's own actors and the `ORE_Prop_*`, `ORE_CP_*`, `ORE_PH_*`,
  `ORE_Spur*`, `ORE_CrushRock_*` and `ORE_Tree_*` actors. Strongpoint posts are also checked against the corridors, solid
  boxes and the first hit of a trace down. Run `pss.check()` again once the other mission actors are in (routes,
  dormant defence slots, lookout towers, buried armour).
- **Moving something.** Edit its `item(...)` or `post(...)` line in the plan and run again. A move made by hand in the
  editor is undone by the next run.
- **Placeholders.** The script removes the `ORE_PH_*` blocks and `ORE_Note_*` labels it replaces (listed in
  `REPLACED` in the script). `ore_map_build.step_placeholders()` would bring them back on a full map rebuild; run this
  script after it.

## Owners and tags

- **Owner:** every Rus building and BXP has RTSComponent `OwningPlayer` 2, the map's one enemy controller. The
  strongpoints run with `EnemyPlayer` 2. Props (crates, sandbags, kerbs) and plain meshes have no owner.
- **Tags on every placed actor:** `ORE_SovietSite` and `ORE_Site_<Site>`.
- **Mission tags:** the `Mission_Ore_*` stages track these with `GetAllActorsWithTag` at stage start.

| Tag | Actors |
|---|---|
| `ORE_BaseA_Target` | `ORE_Site_BaseA_Supplies`, `_AmmoStore_1`, `_AmmoStore_2`, `_FuelStore` |
| `ORE_BaseB_Target` | `ORE_Site_BaseB_Headframe`, `_HoistHouse`, `_Store`, `ORE_BaseB_RadioMast` |
| `ORE_RadioOutpost` | `ORE_Site_Radio_Hut` |
| `ORE_BaseB_RadioMast` | `ORE_BaseB_RadioMast` (the convoy routes' `Ambush.RadioPostActor`) |

## What is placed

40 actors: 35 buildings, BXPs, props and meshes, and 5 strongpoints. Garrison units spawn at begin play. Squads that
only spawn on harder difficulties are marked (Normal+, Hard+, Brutal).

**Base A, convoy yard** (yard about X -1842..3852, Y 20435..27135). 8 actors. Bonus target, loot only.
- **Targets:** Rus_Building_Supplies (2600, 25700), which replaces the ore plant block; 2 Rus_Building_AmmoStorage at
  (3250, 24700) and (1500, 24800); Rus_Building_FuelStorage (-1400, 24300).
- **Defences:** 2 Rus_BXP_GuardTowerMG_Soviet at (-1200, 20950) (south-west) and (3000, 20850) (north-west);
  Rus_BXP_MGBunker (-200, 26700) by the east road entry.
- **Strongpoint `ORE_BaseA`:** T-70, BT-7, 2 Mosin squads, and a PTRS squad on Hard+. The tanks do not dig in.
- **Kept clear:** the valley road, the ammo track exit and the counterattack spawn at (2010, 22500).
- **Not replaced:** the conveyor placeholder stays, because nothing in the project fits it.
- **Convoy routes:** no route may use these buildings as its owning building (`bHasOwningBuilding` stays off).

**Base B, Wismut knoll** (top at 16 m). 8 actors. Each listed actor is one tagged target unless marked otherwise.
- **Headframe stand-in:** Rus_BXP_LargeGuardTower (15.7 m) at (13350, -4500).
- **Hoist house stand-in:** Rus_Building_AdminBunker (12600, -8250).
- **Store:** Rus_Building_AmmoStorage (13300, -6000).
- **Radio mast:** `ORE_BaseB_RadioMast` is the plain alarm Rus_Bxp_Megaphone at (12500, -5900), the destroyable part.
  A no-collision SM_Antenna2 mesh (scale 1.3, about 16 m) is attached beside it as the visible mast.
- **Defences (not targets):** Rus_BXP_Bunker02_ZIS facing the road at (11000, -8300); a GuardTowerMG over the rear ramp
  at (14200, -3100).
- **Strongpoint `ORE_BaseB`:** on the south rim, ZiS-3 76 mm guns at (10211, -6947) and (10100, -7800) and a 53-K at
  (10050, -6030), all facing the road. Also 2 Mosin squads, a PTRS squad and a Maxim, plus a Mosin squad on Hard+ and a
  fourth ZiS-3 on Brutal.
- **Kept clear:** both ramp tops and the viaduct spur's approach slab. The layout's AT point (10211, -5113) lies on the
  slab, so that gun moved to (10100, -7800).

**Radio outpost, spot R3** (11000, 4500). 8 actors.
- **Target:** Rus_Bxp_ContainerHut radio hut at (11000, 4300).
- **Dressing:** a no-collision SM_Antenna2 mast, 3 crate stacks (the Berlin crates) and 2 sandbag arcs.
- **Strongpoint `ORE_RadioOutpost`:** 2 Mosin squads, a third on Normal+, and 1 Squad_Rus_HazmatBazooka.
  - The strongpoint can spawn the HazmatBazooka squad: it spawns through `ARTSAsyncSpawner`, whose `SetupRusSquads` has
    a row for it (added 2026-11-19).
  - The survival note about it was the strategic AI's training filter, which strongpoints do not use.
- **What "taking the outpost" means:** strongpoint squads spawn at runtime and carry no tags, so the stage's tag
  `ORE_RadioOutpost` is on the hut. Taking the outpost therefore means destroying the hut, not killing the guards.
- **To make it "guards dead" instead:** change the stage to bind `AEnemyStrongpoint::OnStrongpointCleared` (ID
  `ORE_RadioOutpost`), or place the guard squads directly as tagged actors.

**Weighbridge checkpoint.** 7 actors.
- **Booth:** Rus_Bxp_ContainerHut at (-604, 7504).
- **Towers:** 2 GuardTowerMG at (950, 6450) and (-872, 8978).
  - The north tower is 2 m nearer the road than its placeholder. The bank rises north, so its legs bury up to about
    1 m on the uphill side.
- **Weighbridge deck:** SM_SovietSlab scaled to 4 x 18 m, centred at (88, 8107) and turned 5.5 degrees to the road.
  - It sits flush with the road and has no collision and no nav effect.
  - The lead's (133, 7705) lies on the deck, so a convoy `StopPoint` there works.
- **Kerbs:** 2 concrete barriers (BP_Military_Dest_ConcreteBarrier) on the north road edge along the deck (the road
  runs east-west here).
- **Collision added:** the booth, the towers and the kerbs have real collision, where the placeholder blocks had
  none. Together with the existing step_checkpoint fence, wire, hedgehogs and barrier (`ORE_CP_*`), they line the 12 m
  gate.
  - **Not verified:** whether vehicles are really held to the gate. That line ends about 35 m either side of the road,
    and nothing was nav-tested.
  - The grey `cp_barrier` block was removed because the real barrier already stands there.
- **Strongpoint `ORE_Checkpoint`:** 2 Mosin squads, a Maxim, and a PTRS squad on Hard+. They hold their posts and fight
  anything in range, including a strike on the stopped convoy 2. If the stage wants them to push out, it can call
  `ReleaseGarrisonToEnemyAI`.

**Artillery battery** (pad at 9.5 m around (18760, 13400)). 9 actors.
- **Pits:** 8 Rus pit BXPs:
  - 2 BS-3 at (18150, 11750) and (17150, 14650);
  - 53-K at (17400, 13150);
  - 2 120 mm mortars at (20050, 12550) and (18900, 15000);
  - 2 DShK at (18450, 10250) and (17592, 15827);
  - Ampulomet at (20700, 16300).
- **Distance from the centre:** 14 to 35 m, so the battery's 60 m auto-collect takes all of them.
- **Facing:** each pit faces the player's HQ.
- **Moved spots:** some layout spots moved 2 to 10 m off the existing camo net, supply storage, crates and sandbags,
  and the Ampulomet off the ammo track.
- **Strongpoint `ORE_BatteryGuard` (light guard):** 2 Mosin squads, and a PTRS squad on Normal+.
  - **Squads only.** The battery would count any team weapon or BXP of player 2 within 60 m as a pit that must die.
  - **The battery actor itself is not placed here.** Another agent places `AEnemyArtilleryBattery` `ORE_Battery` at the
    centre.

**Placeholders removed (28):**
- Ore plant, headframe and hoist house blocks.
- The checkpoint's weighbridge, booth, tower and barrier blocks.
- Their red labels, and the yellow notes for Base A, Base B, the checkpoint, the battery and its 8 pits.

## Choices and limits

- **No AI trainer buildings.** The script places no barracks, tents, factories or yards, because these carry
  `UEnemyBuildingTrainerComponent`. With a decision tree on the enemy controller they would train units, and Base A is
  meant to stay loot-only.
- **The radio mast does not fall.** The antenna mesh is a separate actor attached to the megaphone. When the megaphone
  dies the antenna stays standing, so a model that breaks with it is still needed.
- **Death behaviour (read from the assets, not run).** The tracked kill stages need each target actor destroyed, not
  just dead. The 7 tagged classes appear to do that:
  - AmmoStorage, Supplies, FuelStorage, AdminBunker and ContainerHut call `DestroyActor` in `BP_OnUnitDies`.
  - The Megaphone and the LargeGuardTower use the vertical collapse with `Destroy Post Vertical Collapse` set to true.
- **Not checked in play:** the garrisons spawning and standing on the navmesh, how the guns face, the deaths above, and
  whether the convoy road is clear in practice.
- **How it was tested (2026-11-23).** The test ran headless on a file copy of the map in `Maps/Tmp`, with 2 runs and a
  save of that copy, which was reloaded and then deleted.
  - Both runs placed 40 actors with 0 problems.
  - The second run kept the same objects.
  - The tags, owners, posts and the antenna's attachment survived the save and reload.
  - The saved copy came out at 26 MB instead of 46 MB, most likely because the commandlet does not build the navmesh.
    The real map is saved from the editor, which builds it.
