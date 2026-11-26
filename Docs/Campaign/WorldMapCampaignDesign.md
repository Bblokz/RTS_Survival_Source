# World map campaign design

Status: direction picked by the owner on 2026-11-22. A first C++ implementation was built on 2026-11-23; see
section 10 for what exists, what was checked and what is open.
Sections marked **Proposal** are suggestions that the owner has not confirmed.

## 1. Decisions

| Topic | Pick |
|---|---|
| Map layout | **Fixed spine, procedural flanks.** Story missions, both HQs and the wall sit on hand-placed named anchors. The seed places enemy items, neutral items and side missions around them. |
| Player turn | **One operation plus free spending.** Spend map resources first, then launch a mission or attack an enemy item. Launching ends the turn; the enemy then reacts. |
| Countries | **Build nodes.** A country has its own build nodes, separate from the mission and enemy anchors. They unlock step by step as more of the country is freed. The buildings give the benefits, not the country itself. |
| Buildings | **Three roles for now**, to be expanded later: watch post (reveals fog), supply depot (radixite and blueprints per turn), airfield or battery (lowers enemy strength for missions in range). **Changed 2026-11-25, see section 15.** |
| Commander | **One passive plus one map ability** per commander. The ability has a cooldown counted in turns. |
| Tech tree | **Buy on the map, permanent.** Spend blueprints; the tech is already researched at the start of every later mission. |
| Enemy turn | **One budget, split by alert.** The enemy gets a fixed number of points each turn. An alert level decides the share spent on convoys and divisions; the rest goes to the superweapon. See section 5a. |
| Superweapon | **The great freeze**, a giant Forge cryo engine. What the freeze does to the map and to missions is kept open. |
| Lost mission | A free turn for the enemy. Nothing else. |
| Losing ground | A freed country or a building cannot be lost again, for now. |
| Ability tier cap | Set by the number of missions completed. |
| Divisions | **Enemy pieces the player can strike.** Only the enemy has divisions and convoys. The player can attack one as the operation for the turn, or weaken it with an airfield or battery. |
| Ranks and perks | **Rank shows the tier; perks are removed.** Rank rises with missions completed, and a promotion means a new ability tier. |
| Player HQ | Pinned in the **Harz mountains**, with the intro and first base missions. |
| Wall and finale | The wall is the Svarog Line on the **Dnieper and Dvina** (Smolensk gap). Moscow is a late mission behind it. The finale is the freeze engine at the **Ural Shield**, where the enemy HQ sits. Lake Vygozero is a northern branch on the way. |

## 2. What exists today

Read from code on 2026-11-22; nothing was run. Items marked (inferred) were not confirmed at runtime.

| System | State |
|---|---|
| Generator | Works: seeded, backtracking, async solver. Hand-placed and generated anchors are mixed. |
| `EMapMission` | 14 values. Berlin, Ore Mountains, Oder-Warthe, Pripyat and Frozen Lake are missing. |
| Countries | `UWorldCountryOccupationRegulator` swaps one of three materials once at start-up. The update function has no callers. Nothing is saved. Anchors are matched to a country by bounding box. The Soviet and Resistance materials are instances of one master, `M_StripeMaterial` (`Content/RTS_Survival/WorldCampaign/CountriesShading`), with the parameters `BaseColor`, `BaseColorB`, `StripeColor`, `WideStripeColor`, `StripeIntensity`, `StripeSpacing`, `StripeSpeedWorld` and `StripeWidth01` (names read from the asset files; the graphs and the world-map Blueprints were not opened). No parameter stands for ownership or progress. |
| Turn | Nothing ends a player turn. The counter advances once per load of the world map. No map resources. |
| Fog | Computed once: the HQ plus chains of neutral items. Never refreshed. |
| Divisions | Movement, pathing, influence and save exist. No input, no enemy orders, no combat. |
| Mission return | Missing. No result is written back, rewards are display only, a won mission stays on the map. Returning may regenerate the campaign (inferred). |
| Commander | In game-instance memory only. In no save. The map never reads it. |
| Ability tier | A cap set by hand per mission map (`AMissionManager::PlayerGlobalAbilitySettings`). |
| Tech | 24 techs work inside missions. Nothing persists. Two tech cards exist. |
| Perks, ranks, XP | Data and empty widgets. |
| Deck builder | Saves to the campaign save. Missions read the separate `PlayerProfile` save, so the deck does not reach missions. |

## 3. Map layout: spine and flanks

**Spine (authored).** Each story mission gets a hand-placed anchor at its real location, with a name plate and a
point of interest on the map art. The player HQ, the enemy HQ and the wall are pinned the same way.

**Flanks (seeded).** Enemy items, neutral items and side missions are placed by the existing generator on the
remaining anchors, so two campaigns differ in what lies between the story locations.

**How to pin without a new generation step.** A new `ECampaignGenerationStep` would touch the sync generator, the
async solver, the snapshot, the parity check, the debugger and the test fingerprints. Pre-seed instead:

1. After connections are built, write each pinned mission into `MissionsByAnchorKey`.
2. Remove pinned missions from the mission placement plan.
3. Pin both HQs and the wall through their existing candidate lists (one anchor each).

Seven places can move or break a pin and need a guard:

- the setup wipe: every re-run of ConnectionsCreated (macro undo or a game-thread backtrack) clears the placement
  state, so the pre-seed must be applied again each time;
- the timeout fail-safe, which ignores all rules;
- `EnsureMissionObjectsAreReachableBeforeEnemyHQ`, which swaps a mission with the enemy HQ;
- HQ candidate building, which ignores occupied anchors;
- the random HQ fallback when strict rules fail;
- mission spacing bands, which are hard in both directions and count pinned missions;
- anchor key repair for duplicated hand-placed anchors.

**Mission list.** `EMapMission` must be reconciled with the missions that exist. Each mission needs a flag:
pinned (story) or seeded (side).

## 4. Countries and build nodes

**Country state.** Each country has a liberation value from 0 to 1:

- the story mission in the country is won, and
- enemy items in the country are destroyed.

**Proposal:** liberation = cleared targets / all targets in the country, where the story mission counts double.

**Country look.** Keep `M_StripeMaterial` and give each country its own dynamic material instance instead of
swapping whole materials. Blend the existing colour and stripe parameters between the Soviet and the player values
by the liberation value, so the colour moves as the country is freed. For example, stripes (`StripeIntensity`) can
mark a contested country and fade out at 100 %. The border mesh, which is stored but unused today, can change with
it.

**Membership.** Replace the bounding-box test with a real shape test, or store the country on hand-placed anchors.
Give each country a stable ID; today it is only an array index.

**Build nodes.** A new hand-placed actor type, separate from `AAnchorPoint`. Each node belongs to one country and
has an unlock threshold.

**Proposal:** three nodes per country, unlocking at the first cleared target, at 50 % and at 100 %.

**Buildings (first set):**

| Building | Effect | Reuses |
|---|---|---|
| Watch post | Reveals fog in a radius | `AWorldFowManager`, which needs a refresh call and a new reveal source |
| Supply depot | Radixite and blueprints each turn | `FPrimaryReward` already has both resources |
| Airfield or battery | Lowers enemy strength for missions in range | `UWorldStrategicSupportArea` and the strength estimation, with a negative contribution |

The unused `EMapPlayerItem` values (Factory, Airfield, Barracks) can be replaced by this set.

The depot, airfield and battery rows above are replaced by the picks of 2026-11-25 in section 15.

## 5. The turn

1. **Start of player turn.** Supply depots pay out. Cooldowns count down.
2. **Spending (free, any order).** Build or upgrade on unlocked build nodes. Buy tech. Use the commander ability.
   Edit the deck.
3. **Operation.** Launch a story mission, a side mission or an attack on an enemy item. This ends the turn.
4. **Result.** Win: rewards are granted, the target is marked cleared, the country value and the fog update,
   the mission count (and with it rank and tier cap) goes up. Loss: nothing changes for the player; the enemy turn
   that follows is the whole cost.
5. **Enemy turn.** See section 5a.

## 5a. The enemy turn

**Budget.** The enemy gets a fixed number of points each turn.

**Alert.** A value from 0 to 1. Player wins and destroyed enemy items raise it. **Proposal:** it falls a little each
turn in which the player does not strike, and a struck target adds more alert the closer it is to the wall.

**Split.** Alert is the share of the budget spent on reaction. The rest goes to the superweapon.

- **Reaction.** Convoys and divisions are sent toward the targets nearest to where the player struck. On arrival
  they raise the strength of targets in their radius (the influence code exists; its sign for player items must
  be fixed first). The player can attack a convoy or division as the operation for a turn.
- **Superweapon.** Progress is counted in stages. Each finished stage of the freeze changes the map; what the
  change is stays open.

This gives the player a real choice: striking hard slows the weapon and hardens the next missions; a quiet turn
keeps missions easier and lets the weapon advance.

Code that exists for this: division spawn, pathing, per-turn movement, influence and save. Missing: enemy orders,
convoys as a piece, the arrival effect, the attack operation, alert, the budget and the weapon state.

## 5b. Rank and ability tier

- Rank rises with missions completed. The 16 ranks in `ERTSPlayerRank` stay.
- The global ability tier cap comes from the rank instead of from the hand-set value on each mission map.
  **Proposal:** Tier II at 2 missions, Tier III at 5, Tier IV at 9. A mission map can still lower the cap for story
  reasons.
- Perks are removed: `ERTSPerkType`, the perk save fields and the perk widgets.

## 5c. Pinned places

In campaign order. Places marked (docs) come from the mission documents; the rest were picked on 2026-11-22.

| Order | Place | What | Source |
|---|---|---|---|
| Start | Harz mountains | Player HQ, intro and first base missions | picked |
| 2 | Ore Mountains, Czech border | Convoy raids | docs |
| 3 | Berlin | Forge relay at the Zoo tower | docs |
| 4 | Oder-Warthe line, west Poland | Radio direction finding | docs |
| Last of Act 1 | Pripyat marshes | Airstrip and the ME410 | docs |
| Act 2 | Svarog Line on the Dnieper and Dvina, Smolensk gap | The wall | picked |
| Opener of Act 3 | Lake Vygozero, Karelia | Frozen lake, northern branch | docs |
| Late | Moscow | Assault behind the wall | picked |
| Finale | Ural Shield | Freeze engine and enemy HQ | picked |

**Open:** which of the 14 older `EMapMission` entries stay as seeded side missions, and whether the dam and the
Black Forest from the first notes become pinned points of interest.

## 6. Commander on the map

First step: save the chosen commander in the campaign save and pass it to missions from there.

**Proposal** for passive and ability per commander, to be reviewed:

| Commander | Passive | Map ability |
|---|---|---|
| Field Commander | Buildings cost less | Lower one target's strength for one turn |
| Luftwaffe Liaison | Airfields have a larger range | Recon flight: reveal a region |
| Barrage Marshal | Batteries lower strength more | Bombard: remove one fortification from a target |
| Radixite Herald | More radixite from depots | Convert radixite to blueprints |
| Scrap Warlord | Extra blueprints from destroyed enemy items | Salvage: refund part of a mission's losses |
| Rail Engineer | Depots also support missions in range | Rail strike on an enemy division |
| Light Bringer | Watch posts reveal further | Mark a target: its strength is shown in full detail |
| Feuersturm | Enemy barracks nearby count for less | Burn a supply route: stop strength growth for some turns |
| Stahlläufer | Operations ignore one fortification type | Forced march: launch on a target one step outside the fog |
| Festungsbaumeister | One extra build node per country | Fortify: a building cannot be destroyed for some turns |
| Sturmkommandant | Side missions pay more | Infiltrate: start the next mission with extra infantry |
| Sky-Drop Quartermaster | Buildings finish in the same turn | Supply drop: resources now, on a long cooldown |

## 7. Tech tree

- Paid with blueprints on the map; permanent for the campaign.
- Applied at mission start through the same call the tech cards use (`UPlayerTechManager`).
- Branches follow the buildings: infantry, light armour, heavy armour, air, economy.
- The 24 `ETechnology` values are the starting content.
- **Open:** whether tech cards stay in the deck builder once the tree exists.

## 8. Build order

Each step is useful alone and unblocks the next.

1. **Return path.** Mission result back to the campaign; cleared flag on map items; continue a campaign without
   regenerating it; commander and deck read from the campaign save; launching ends the turn.
2. **Pinned spine.** Named anchors, pre-seeding, the seven guards, new mission list.
3. **Country state.** Stable IDs, shape membership, liberation value, material parameter, save.
4. **Map resources and build nodes.** Radixite and blueprints in the campaign save, the node actor, three buildings.
5. **Fog refresh.** Reveal from cleared anchors and watch posts.
6. **Tech tree.**
7. **Commander passive and ability.**
8. **Enemy turn.** Alert and budget, enemy orders for divisions, convoys, the attack operation, weapon stages.
9. **Rank and tier cap.** Can come right after step 1, since it only needs the mission count.

## 9. Open decisions

- What the freeze does at each stage, on the map and inside missions, and what happens when it completes.
- The numbers: enemy budget, alert gain and decay, weapon stages, mission counts per tier.
- Which older `EMapMission` entries stay as side missions.
- Fortification modifiers: nothing changes them in play yet. They could be what a convoy delivers.
- The commander table in section 6 is unreviewed.

## 10. Implementation status (2026-11-23)

Built in C++ in one unattended session. The DebugGame editor target compiles. The Development editor DLL was
**not** rebuilt, so the open editor does not have any of this until it is closed and rebuilt. No asset (`.uasset`,
`.umap`) was changed. The Source changes are uncommitted. The map content appended to `Config/DefaultGame.ini`
was committed by the auto-commit in the outer repository ("Update on Config/DefaultGame.ini").

### What was checked

- 35 automation tests `RTS.WorldCampaign.*` pass (rules, operation result, pins, countries, build nodes, enemy turn,
  tech, commander kit). Run headless with `Automation RunTests RTS.WorldCampaign`.
- Two headless runs of `RTS_WorldCampaign` (`-game -nullrhi`, own save slot through
  `-RTSWorldCampaignSaveSlot=`). The real campaign save was not touched.
  - **Run A, new campaign:** generated with all 11 pins, 20 countries found, all 46 kept anchors mapped to a
    country, 42 build nodes spawned (9 open), turn 1 reached and saved. An operation was then committed on the
    Berlin pin and saved as pending.
  - **Run B, load path with a simulated victory:** the campaign restored, the result was applied (1 mission
    completed, Berlin cleared, nothing pending, alert 0.27), the enemy turn ran and called back (7 of 10 points
    went to the freeze, 3 were banked), the turn went from 1 to 2 and one more build node opened.
- **Not checked:** anything on screen (country colours, build node look, fog), a real mission played and
  returned from, the mission end screen buttons, the deck and bought tech reaching a mission, the tier cap inside a
  mission, a convoy or division being spawned and arriving. The generator was not compared against the old seed
  fingerprints with pins off.
- **A new campaign has an empty deck** (0 selected cards in run A). Until the player fills it in the deck builder,
  a campaign mission unlocks no unit that needs a unit card.

### What exists now

| Part | Where | What it does |
|---|---|---|
| Progress state | `WorldCampaign/CampaignProgress` | `FWorldCampaignProgressState` inside the campaign save: missions won and lost, cleared anchors, map resources, bought tech, buildings, enemy alert and freeze, commander map state, pending operation, difficulty. `UWorldCampaignProgressManager` on the world controller owns it. |
| Rules | `WorldCampaignRules`, `UWorldCampaignRulesSettings` | Rank and tier from missions, alert, budget split, freeze stages, liberation, node unlocks. Numbers are on the project settings page "World Campaign Rules". |
| Return path | `URTSGameInstance`, `AWorldPlayerController`, `AMissionManager`, `UW_MissionEnd` | Launching records the pending operation and saves. Victory reports a result and Continue opens the world map. On a lost mission the second button reads WITHDRAW, reports a defeat and opens the world map. The map then applies the result, clears the target, pays the reward and runs the enemy turn. |
| Tier cap | `ACPPController::SetupGlobalAbilityManager` | In a campaign operation the cap is the lower of the mission's own cap and the rank's cap. |
| Commander | campaign save | Saved with the campaign and restored into the game instance when a campaign is continued. A mission map's backup commander no longer replaces it in a campaign operation. |
| Deck and tech in missions | `UPlayerProfileLoader`, `UTrainingLayoutSubsystem` | A campaign operation takes cards from the campaign deck and researches the bought tech at mission start. Outside the campaign nothing changes. |
| Pinned places | `CampaignGeneration/PinnedPlaces`, generator, async solver | Pins from config are resolved to anchors, pre-seeded after every connection build, and guarded. Five missions appended to `EMapMission`. |
| Map content | `UWorldCampaignMapContentSettings`, `Config/DefaultGame.ini` | 11 pinned places, 20 countries, 42 build nodes and 5 mission levels as config text. Plot: `Saved/WorldCampaignContent/WorldCampaignPlacement.png`. |
| Countries | `WorldCountryOccupation` | Stable ids from config, membership by the real mesh shape, a liberation value per country, one dynamic material per country blended from the Soviet to the Resistance look. |
| Build nodes | `WorldCampaign/WorldBuildNodes` | `AWorldBuildNode` and `UWorldBuildNodeManager`: unlock by liberation, build for a price, depot income, watch post fog, airfield and battery lower enemy strength in range. |
| Fog | `WorldFow` | Refreshes at runtime. Cleared anchors and watch posts reveal. |
| Enemy turn | `WorldCampaign/EnemyTurn`, `WorldDivisions` | `UWorldEnemyTurnDirector`: alert decay, budget split, convoys and divisions toward struck areas, freeze stage counter with events, waits for movement. New convoy piece. Divisions are enemy only. |
| Division as target | world controller | `LaunchOperationAgainstDivision`; a win removes the piece. |
| Tech | `WorldCampaign/CampaignTech` | 24 technologies in five branches with prices and prerequisites; `TryResearchTechnology`. |
| Commander map kit | `WorldCampaign/CommanderMapKit` | One passive and one ability per commander with a turn cooldown. Resource abilities work; reveal and division damage are wired; the rest only broadcast a request. |

### No screens were built

Everything is reachable from Blueprint through getters on the world controller (`GetWorldCampaignProgressManager`,
`GetWorldBuildNodeManager`, `GetWorldCampaignTechManager`, `GetWorldCommanderMapKit`, `GetWorldEnemyTurnDirector`),
their Blueprint-callable functions and delegates, and three new controller events: `BP_OnBuildNodeClicked`,
`BP_OnEnemyDivisionClicked`, `BP_OnEnemyTurnStarted`. The screens (resources and rank, build menu, tech tree,
commander ability, enemy turn summary) still have to be designed and built.

### Decisions made without the owner (change freely)

- A new campaign starts on the player turn 1. The enemy no longer gets a turn on every load of the map; it gets
  one only after an operation, won or lost.
- Restarting a lost mission is free. Only WITHDRAW costs the turn.
- If the game is closed during an operation, the operation never happened and the turn is not spent. A victory
  whose Continue was not pressed before the game closed is lost for the same reason.
- A cleared mission or enemy item is hidden on the map. A Blueprint can show a wreck or flag instead through
  `BP_OnClearedByPlayer`.
- The map has one blueprint currency; the blueprint types in a reward are summed. Commander XP rewards are ignored.
- Rank goes up one step per mission won. Tier II at 2, III at 5, IV at 9.
- All numbers: start with 200 radixite and 2 blueprints; enemy budget 10 per turn; freeze stages cost 30, 40, 50,
  60, 70 points and there are five; building prices and effects; tech prices; commander values.
- A division or convoy is fought on the next enemy procedural map and gives no reward. (2026-11-24: with the map
  source set to HandmadeMaps it is fought on a handmade field map instead; see `EnemyItemMaps_Plan.md`.)
- Convoys add a fortification (bunkers, minefields or AA battery) to their target on arrival.
- Pinned missions count toward the minimum mission spacing only, so far story places cannot block side missions.
- Oder-Warthe is pinned about 42 km east of Miedzyrzecz and Lake Vygozero about 31 km south-west of the lake,
  to keep the spacing and stay inside the map boundary. The wall mission sits west of the wall, in Belarus.
- The Air tech branch holds the two anti-air technologies, because no aircraft technology exists.
- A campaign operation left through a menu ends there: its context is dropped as soon as another level loads,
  so a later survival or hangar game is never treated as a campaign mission.
- On the load path the anchors placed in the level are removed; a restored campaign uses only its saved anchors.

### Test switches (not in shipping builds)

- `-RTSWorldCampaignSaveSlot=<name>`: use another save slot. Always pass it in a headless run of the world map.
- `-RTSWorldCampaignForceLoad`: take the load path when a save exists.
- `-RTSWorldCampaignTestLaunchPin=<PinId>`: commit an operation on that pinned mission without opening a level.
- `-RTSWorldCampaignSimulateResult=Victory|Defeat`: stand in for the mission result on load.
- The log line `RTS_WORLDCAMPAIGN_SUMMARY` prints the campaign state at each of these moments.

### Not wired yet

- Commander passives: the query functions exist but nothing calls them (building cost, income, range, rewards).
- Commander abilities other than resources, reveal region and division damage: no system carries them out.
- Timed commander effects are saved but not re-applied after a load.
- Card rewards and variant rewards are still not granted.
- The perk code was left in place; nothing uses it. It can be deleted.
- A "continue campaign" menu entry: `URTSGameInstance::RequestLoadSavedCampaign` exists for it, no menu calls it.

### For the owner to do in the editor

1. Close the editor and rebuild the Development target.
2. WorldData needs base difficulty entries for the five new mission types and for the supply convoy; until then
   an error is reported for each at campaign start.
3. `MissionObjectClassByType` has no class for the five new mission types; they borrow the first mission class.
4. No level exists for the wall mission, Moscow or the Ural finale.
5. The freeze engine pin is only reachable through the enemy HQ (logged at generation). Move the pin or accept it.
6. `BP_AnchorPoint4` sits 38 units from the Oder-Warthe pin. Remove it or give it the pin id `OderWarthe`.
7. 51 hand-placed anchors share keys and are repaired at every generation; the generator asks for 400 new anchors
   and can fit 163.
8. Set Allow CPU Access on the country meshes, or country membership falls back to bounding boxes in a cooked build.
9. Three Russia mesh actors are stacked; `StaticMeshActor_12` is the one in the config.
10. Russia is 71 % of the playable area and holds five pins. Splitting it into regions would make countries matter
    more there.
11. To switch pinning off, set `bUsePinnedPlaces=False` under
    `[/Script/RTS_Survival.WorldCampaignMapContentSettings]` in `Config/DefaultGame.ini`.

## 11. World map UI picks (2026-11-23)

Sketches: `Saved/WorldMapUIDesigns` (outside the Source repository). `sheet_1.png` to `sheet_6.png` hold three
versions per element, `sheet_7.png` shows the picks together, `sheet_8.png` is the tech wheel. Regenerate with
`build_ui_sheets.py`, `build_ui_sheets2.py` and `build_ui_sheets3.py` (HTML rendered by headless Edge). Nothing is
built in the game.

| Element | Pick | Owner's notes |
|---|---|---|
| Campaign bar | **Top strip** (1A): turn, rank, tier pips, missions to the next tier, radixite, blueprints | Use the resource icons the game already has. |
| Build node menu | **Card row** (2A): a panel above the site with one card per building | Same: real resource icons for the prices. |
| Tech tree | **Wheel web, big badges on branch colours** (sheet 13): HQ in the centre, five branches as coloured sectors, the real hexagon technology icons at about 86 px (rim 104 px), cream paths, minor nodes as small pips, bridge nodes between neighbouring branches | Flat wheel, not an instrument panel (sheet 8 rejected). Big badges and paths of 10C on the branch-coloured background of 10B. |
| Commander | **Portrait and button** (4A), bottom-left | |
| Enemy status | **A small bar that shows only the freeze progress in percent**, smaller than the sketched options | No alert gauge, no point split, no enemy turn dialog. |
| Enemy pieces and countries | **On the map** (6B): dashed arrow and tag on each enemy piece, liberation bar under each country name | |
| Blueprints | **One blueprint currency** | Four types would make the design too complex. |

Open after these picks:

- The single blueprint is drawn with the construction blueprint icon. That icon choice is not confirmed.
- A web needs many more nodes than the 24 technologies. The small nodes and their bonuses are not designed.
- With no alert gauge and no enemy turn dialog, the player sees the enemy's reaction only as pieces on the map.

### Tech wheel details (picked 2026-11-24)

- **Icons:** the 25 German technology icons in `Content/RTS_Survival/Textures/GameUI/TechTree/GerTech` (hexagon
  badges). Sketch copies are in `Saved/WorldMapUIDesigns/tech`.
- **Cross-branch requirements:** the path says how a technology is reached. A technology can also need one specific
  technology from any other branch. That is shown three ways on the node:
  1. an outline in the colour of its own branch (every technology has this);
  2. a second, outer outline in the colour of the branch the needed technology is in;
  3. a key badge on the corner with the needed technology's icon, a red mark until it is owned and a green tick after.
  Selecting the node draws a dotted chord across the wheel to the needed technology.
- **Branch colours:** infantry green, light armour blue, heavy armour red, support purple, economy yellow.
- **Not decided:** which technology sits where, the small nodes and their bonuses, and which technologies carry a
  cross-branch requirement. The placement in the sketches is a first guess, and the Air branch was renamed Support
  there because its icons are anti-air and utility upgrades.
- **Code follow-up:** `FWorldCampaignTechDefinition::Prerequisites` already allows any technology as a prerequisite.
  The wheel needs to tell a path neighbour from a cross-branch requirement, which the data does not do yet.

## 12. World map UI build (2026-11-24)

The picks of section 11 are built in C++ as self-building widgets (no Blueprint, no new asset), in the manner of the
deck builder. Status of checks is at the end of this section.

### Where it lives

| Part | Folder under `WorldCampaign/WorldMapUI` | Main class |
|---|---|---|
| Shared look and colours | `CampaignHUD` | `FWorldCampaignUILook`, `WorldCampaignUIColors` |
| Everything drawn over the map | `CampaignHUD` | `UW_WorldCampaignHUD` |
| Top strip | `CampaignHUD` | `UW_WorldCampaignBar` |
| Build site card row | `BuildSites` | `UW_WorldBuildSitePanel`, `UW_WorldBuildCard` |
| Arrows, tags and country bars on the map | `MapTags` | `UW_WorldMapTags`, `UW_WorldMapArrowLayer` |
| Commander portrait and ability button | `CommanderPanel` | `UW_WorldCommanderPanel` |
| Tech wheel page | `TechWheel` | `UW_WorldTechWheel`, `UW_TechWheelPaintLayer` |
| Tech wheel data and rules | `WorldCampaign/CampaignTech` | `FWorldTechWheel`, `WorldCampaignTechWheel`, `UWorldCampaignTechManager::TryBuyWheelNode` |

### How it is hooked up

- `UW_WorldMenu` creates the HUD and the tech wheel page (`bUseCampaignHUD`, on by default). The HUD is added to the
  viewport above the world menu; the tech wheel is a page of `FullUISwitcher`, like the deck builder.
- With the HUD on, the Blueprint header (five buttons and the turn box) is collapsed. Its jobs moved to the strip:
  ARMY opens the deck builder, TECHNOLOGY the tech wheel, ARCHIVE the archive. The Command Perks button is gone,
  because perks were removed from the design.
- The HUD is hidden while a full-screen page is open.
- `AWorldPlayerController`: a click on a build site opens the card row; any other map click or a right click closes
  it; while the commander ability waits for a target, the next map click picks that target and a right click cancels;
  the enemy turn blocks the ability and closes the cards; the start of a player turn refreshes the panel.

### What each part shows

- **Top strip:** turn, rank, four tier pips with the tier number, missions to the next tier, the freeze as a small bar
  with its percentage and a tick per stage, radixite and blueprints with the real icons and their income per turn
  from supply depots, and the three page buttons.
- **Build site cards:** one card per building the rules allow, with its effect text written from the real numbers
  and its price; unaffordable cards are dimmed; a locked site says what opens it; a built site shows what stands
  there. The plate follows the site on screen.
- **Map tags:** a dashed arrow from each moving enemy piece to its destination with a tag (strength and turns away,
  or CONVOY and when it arrives); each country that has targets gets its name with a liberation bar.
- **Commander:** round portrait, name plate, one ability button (lit when ready, grey with the turns left when
  cooling down), a passive plate, and hover plates with the descriptions. Abilities that need a target ask for one.
- **Tech wheel:** the flat wheel of sheet 13 with the real technology icons, branch-coloured sectors, cream paths,
  pips for minor nodes, a branch outline on every technology, and for a cross-branch requirement a second outline,
  a key badge and a chord to the needed technology. The side panel explains the selected node and buys it.

### Tech wheel content (first version, to be reviewed)

- 61 nodes: the HQ, 24 technologies and 36 minor nodes. Minor nodes cost 40 radixite and do nothing yet.
- Technology prices in blueprints: 1 for the first step of a branch, 2, 2, 3 for the next three and 4 for the rim.
- Four cross-branch requirements as examples: Tungsten 8.8 needs Harvester Capacity, Flak 88 Ground Role needs Track
  Improvements, Panzerjaeger needs Spall Liners, Flame Damage needs Scavenging Reward.
- The Support branch has no rim technology (its rim slot is a minor node), because only 24 technologies exist.
- Names follow the sketches; the descriptions on the side panel were written for the wheel and need a review.
- Icons are referenced by path from `UWorldCampaignTechSettings` (the game's own icon data is only in Blueprints).

### Decisions made while building

- The build site marker on the map is a small plain cylinder, coloured by state. A Blueprint can replace it.
- Locked technology icons are darkened, not greyed: a UMG image tint cannot desaturate.
- The commander ability button shows a ring-and-dot glyph, because no map ability icons exist.
- Country names sit at the mean position of the anchors in that country, since several country meshes span the map.
- Only enemy pieces with a move order or a reaction target get a tag; the two starting divisions get none.

### Checked and not checked

Checked:

- DebugGame editor build compiles and links (2026-11-24 13:54).
- Offscreen run with `-WorldCampaignUICapture -RTSWorldCampaignSaveSlot=<name>` (class
  `UWorldCampaignUICaptureSubsystem`): nine pictures in `Saved/WorldCampaignUICapture`, no crash, no assertion.
  The pictures show the top strip, the card row before and after building a supply depot, the commander plate with
  its hover text, the tech wheel (plain, with a cross-branch node selected, after buying a node) and the map after
  one enemy turn.
- Automation tests `RTS.WorldCampaign.*`: 52 of 52 pass (2026-11-24, DebugGame, headless).
- By reading the code: every full-screen HUD layer is hit-test invisible outside its plates, so map clicks pass;
  the deck builder, the archive and the tech wheel each have their own way back to the map.
- The capture log has no error from the new widgets. It does list content gaps: `WorldData` has no base difficulty
  for the pinned missions (Berlin, Ore Mountains, Oder-Warthe, Frozen Lake, Ural Shield) and no strength entry
  for the supply convoy.

Not checked:

- Anything done with the mouse: clicks on cards and wheel nodes, picking a target for the commander ability, clicks
  that should or should not pass through the plates to the map. The capture drives the widgets through code.
- A real mission launched from the map and the return to it with this UI.
- The Development build (only DebugGame was built) and cooked builds (technology icons are referenced by path).
- The build site marker on the map: in the capture the chosen site lies just outside the picture.

Known rough spots:

- Tech wheel: names no longer sit on icons, but a few sit a short way from their node (Autoloader, Long-Range
  Calibration, Barricades), because the nodes are packed; it goes away when the node placement is redone.
- The enemy piece tag reads "+15 %" (the strength it adds); it shows no turns-away line for pieces without a
  destination.
- A thin cyan line runs from the piece south-east of the HQ into open ground in every map picture. Nothing in the
  UI or country look code draws it; its source is not found.
- The country tag keep-out box around the HQ has a fixed size, so it can be off at other zoom levels.

### Polish round (2026-11-24, evening)

- Top strip: all seven cells in the same metal frame, rank insignia inside the RANK cell, regular gaps, freeze
  track as a trough with a rim and stage ticks.
- Commander plate: ability button moved clear of the portrait and the passive tag; name plate, tag and hover texts
  share one plate style.
- Map tags: a country tag under the top strip is hidden whole, a tag on the HQ is lifted above it, overlapping tags
  are nudged or hidden, the liberation bar is a trough. Maths in `WorldMapTagsMath`.
- Build site panel: opaque backing, plate sized to the cards shown (one card when built), kept below the top strip
  and on screen, "BUILT" as a state plate, same footer line on every card.
- Tech wheel: names placed by `TechWheelLabelPlacement` (least overlap out of twelve spots around the node) on a
  dark plate above the icons; header resources in metal cells; "reached through" hidden when nothing reaches.
- The capture has a ninth picture: freeze bar part filled and the ability button on cooldown.
- Checked: DebugGame build 18:10, `RTS.WorldCampaign.*` 69 of 69 pass, nine pictures looked at. Still not checked:
  mouse input, a real mission, Development and cooked builds.

### Mission names and build site hover (2026-11-24, night)

- **Mission names.** `WorldCampaignMissionNames` (next to `AWorldMissionObject`) names a mission: its entry in
  `UWorldCampaignMapContentSettings::MissionNames` (config, `+MissionNames=(Mission=...,DisplayName=NSLOCTEXT(...))`)
  first, else the `DisplayName` of the pinned place that holds it. A mission with neither keeps the title of its
  Blueprint.
- The name shows in two places: as the title of the briefing panel (inside the rich text style of the title it
  replaces) and on a small dark plate under the mission piece on the map (`UW_WorldMapTags`). The plate is hidden for
  a mission the fog hides or the player won, and mission names win over country names when two would overlap.
- Story missions still spawn the stand-in mission Blueprint, so their description, objectives and rewards in the
  briefing panel are still those of the stand-in. Only the title is right.
- **Map pieces per mission.** Five concept drawings per hand-made mission are in
  `Saved/WorldMapUIDesigns/MissionObjects/<Mission>/` (`overview.png`, `pick.md` once picked). A picked piece goes in
  as a Blueprint child of `AWorldMissionObject` in `UWorldCampaignSettings::MissionObjectClassByType`.
- **Build site hover.** The site whose cards are open stays lit, and a site behind the cards no longer lights up
  under the cursor (`AWorldPlayerController::Tick_UpdateHoveredBuildNode`, `UW_WorldCampaignHUD::GetOpenBuildSite`).
- A country name is lifted above a mission piece and its plate, as it is above the HQ.
- The capture holds the camera (`SetIsCameraMovementDisabled`): an offscreen run reads the real desktop cursor, and a
  cursor at a screen edge scrolls the camera off the map.
- Checked: DebugGame build 21:47, `RTS.WorldCampaign.*` 71 of 71 pass, capture pictures 01 (BERLIN and ORE MOUNTAINS
  plates, GERMANY above them; the six story missions under fog have no plate) and 02 (open site lit). Not seen in
  any picture: the briefing panel title, and the cursor on the cards.

## 13. Country look (2026-11-24)

Owner picks: liberation as a moving front, border tells the state, staff-map fill. Border thickness approved 2026-11-24.

- **Fill** (`M_CountryStaffMap`): occupied ground is dark red with diagonal hatching, free ground is clean blue. Up to 8 front points (cleared anchors) push the free ground outward over 2.5 s; a cream line marks the front.
- **Border band**: drawn inside the fill from a per-country edge-distance picture (`WorldCountryEdgeDistance`, built once at runtime from the fill mesh triangles). Solid red = occupied, solid blue = free, moving red and cream dashes = contested, grey = no targets. Brighter while the cursor is over the country. Width is `Border Band Width` on the regulator (14 map units; 0 turns it off). The thin outline mesh repeats the colour.
- **Known limit**: dashes are a world-space checker, not laid along the border, so their length varies with border direction.
- **Code**: `WorldCountryOccupation/WorldCountryStaffMapLook.cpp`, `WorldCountryEdgeDistance.h/.cpp`, hover in `AWorldPlayerController::Tick_UpdateHoveredCountry`.
- **Tools**: `Source/Tools/WorldCampaign/create_country_look_materials.py` rebuilds both materials; `run_country_look_capture.ps1` (`-WorldCountryLookCapture`) writes seven pictures to `Saved/WorldCountryLookCapture`.
- **Status**: built and captured in the isolated DeckTest copy only; main DebugGame and Development DLLs need a rebuild.

### Colour pick 2026-11-24: steel and cream (sheet 14A)

The owner rejected the light-blue neon accent: the game's UI is scratched gunmetal with glass, in the manner of
Company of Heroes. Of three versions (`Saved/WorldMapUIDesigns/sheet_14A-C.png`, `build_ui_sheets7.py`, drawn with
the Frontstahl skin parts) the owner picked A, steel and cream: no colour accent; lit lamps, the selected rim and the
ready ability button are cream; the freeze bar is frosted white; owned and good values are a muted olive
(`WorldCampaignUIColors::Accent`, `Reach`, `Freeze`, `Good`). The campaign widgets no longer read the deck builder's
cyan `AccentColor`. Red stays for the enemy, and the tech wheel keeps its branch colours.

Built the same day from the skin's own textures (`FWorldCampaignUILook::MetalCell`, `BoltedPanel`, `RoundBezel`):
a metal cell behind each value of the top strip, the bolted gunmetal frame on the build site panel, and a bezel and
glass dome on the ability button. The frame, the dome and its bezel are the campaign's own textures in
`/Game/RTS_Survival/Textures/GameUI/WorldCampaign` (`T_Campaign_BoltedPanel`, a half-size copy of the skin panel so
its corners draw at 37 units, `T_Campaign_GlassDome`, `T_Campaign_GlassBezel`), made by
`Source/Tools/WorldCampaign/make_campaign_hud_art.py` and imported by `run_import_campaign_hud_art.ps1`. The dome is
grey-scale and the button tints it per state.

## 14. Build site markers (2026-11-24)

Owner pick: design 6, hex plate (sheets `Saved/WorldMapUIDesigns/site_*.png`).

- `AWorldBuildNode` shows a hex plate on a flat plane: grey rim, hatched centre and lock while locked; yellow rim, glow and plus while open; gunmetal rim, the building's sign and a green lamp once built.
- The plate is drawn by one material, `M_BuildSiteHexPlate` (`Source/Tools/WorldCampaign/create_build_site_materials.py` rebuilds it); no mesh or sign texture of its own.
- `Hex Plate Width` on the node sets the size (57.6 map units; owner asked for 20% over the sketch's 48). The old cylinder is hidden but stays as the click target; it shows again when the material is missing.
- Hover: `AWorldPlayerController::Tick_UpdateHoveredBuildNode` calls `SetIsHovered` on the site under the cursor trace; the plate brightens and gets a cream outline.
- Capture steps 08 to 10 of `run_country_look_capture.ps1` show the sites.

### Story mission map pieces (2026-11-25)

The five story missions the map can launch have their own map piece. The owner picked each design in its drawing
session (`Saved/WorldMapUIDesigns/MissionObjects/<Mission>/pick.md`).

| Mission | Piece | Blueprint (under `WorldCampaign/MapObjects/Missions/Story/`) |
|---|---|---|
| Ore Mountains | Stollenmund: fortified adit in a crag, the game's Panzer IV H in front | `OreMountains/BP_Mission_OreMountains` |
| Berlin | The level's damaged flak tower and Reichstag ruin in small, on a cut-corner base | `Berlin/BP_Mission_Berlin` |
| Oder-Warthe Line | Peilturm with an H aerial on the roof | `OderWarthe/BP_Mission_OderWarthe` |
| Pripyat Marshes | Hangar Island with the Me 410 | `Pripyat/BP_Mission_Pripyat` |
| Lake Vygozero | A Forge heavy tank going through the ice | `FrozenLake/BP_Mission_FrozenLake` |

- **Registration:** `MissionObjectClassByType` in `Config/DefaultGame.ini`. Pripyat uses the old `ME410UnlockMission`
  value, so that entry now points at the new Blueprint; `BP_Mission_ME410` is no longer used but still exists.
- **Structure:** each Blueprint is a child of `BP_MissionWorldObject` with static mesh components under one
  `PieceRoot` scene component. `PieceRoot` has yaw 90 because the models have their front on +Y and the map camera
  looks along +X.
- **Game meshes used as they are:** Berlin's flak tower (`SM_BF_FlakTower_Damaged`) and the Reichstag (the instanced
  meshes of `BP_ReichstagRuin_B2`, without decals, forecourt and ramp). The Reichstag keeps its level materials; the
  tower's concrete slot is overridden on the piece (see Concrete). The Ore Mountains tank is `SM_PzIV_H_TrainingPreview` in plain dark
  yellow: the UVs of that one-slot preview mesh do not fit the tank camo texture.
- **Materials:** three shared masters in `Story/Shared`: `M_WorldPiece_PBR` (base colour, normal, ORM),
  `M_WorldPiece_Albedo`, `M_WorldPiece_Flat`.
- **Concrete (2026-11-25):** all concrete on the pieces is `Story/Shared/M_WorldPiece_Concrete`, built by
  `concrete_lib.py`. It uses the Sooted Fortress base colour, normal and ORM textures in a box projection (no mesh
  UVs), with pour and panel joints in relief, colour variation, rain streaks and ground grime. Every size is a
  parameter in object centimetres.
  - **Why not the bunker material:** `M_BF_SootedFortress_Procedural` has fixed sizes for a real bunker (noise every
    13 to 140 cm, pours of at least 35 cm). On a 60 cm piece that is one blotch; on the shrunk flak tower it is noise.
  - **Instances:** `MI_WP_Ore_CastConcrete` (headwall, cap slab, wing walls), `MI_WP_OW_CastConcrete` (tower, annexes,
    casemate, dragon's teeth), `MI_WP_Berlin_TowerConcrete` (the flak tower; its lengths are 1 / tower scale larger,
    because that mesh is real size).
  - **Setting:** texture once per 45 cm of piece, pours of 12 cm, panels of 26 cm (`CONCRETE_LOOK` in
    `build_pieces.py`), picked from four in one picture (`concrete_variants.py`, `run_lineup.ps1 -Variants 1`).
  - **Not concrete:** Pripyat and Lake Vygozero have none; the Reichstag is stone and was left.
  - **Saving:** the shared editor keeps the piece assets open, and headless saves then fail. `wait_editor_free.ps1`
    waits for it; this change was saved through the editor itself (three assets).
- **Piece lights (2026-11-25):** the campaign level lights its pieces dim and from behind. Each story piece Blueprint
  has two spot lights on lighting channel 1 only, and only the piece's own meshes are on channels 0 and 1, so the map
  and other pieces are not lit by them.
  - `PieceLight`: warm key from the front left (400 cd, at -200 / -260 / 380 from the pivot), casts shadows,
    specular scale 0.15.
  - `PieceFillLight`: weak white fill from the camera's side (60 cd, at -250 / 0 / 420), no shadows, no highlights.
  - A first try with one white 250 cd light from the camera's side was rejected by the owner: flat, and it washed
    the colours out.
  - Constants: `PIECE_LIGHTS` in `piece_lib.py`; picked from four set-ups in one picture
    (`run_lineup.ps1 -LightTest 1`). The HQ piece and the other map objects do not have these lights.
  - Cost: one shadow-casting movable spot light per story piece.
- **Briefing text:** description and objectives come from the mission design docs, without late reveals. Rewards are
  empty and the "on defeat" text is still the stand-in of `BP_MissionWorldObject`.
- **Rebuild:** `Source/Tools/WorldMapPieces` (`build_pieces.py` headless import and Blueprint build,
  `lineup_capture.py` line-up pictures, `check_pieces.py`). The models and textures are in the drawing folders under
  `Saved/`, which git does not track.
- **Checked:** all five spawn beside the HQ piece at about its footprint (`Saved/WorldMapPieces/lineup_*.png`); the
  game capture shows the Berlin and Ore Mountains pieces with their name plates; the log has no class fallback
  warning for the five missions.
- **Not checked / open:**
  - Oder-Warthe, Pripyat and Lake Vygozero in the running game: they start under fog.
  - In the campaign level the pieces look dim and brown, like the HQ piece. Their true colours were only seen
    under a neutral test light (`Saved/WorldMapPieces/story_pieces_neutral_light.png`).
  - The flak tower and Reichstag materials turn to a noisy pattern at this size; the real tower mesh at the same
    scale shows the same.
  - Clicking a piece, the outline, and the briefing panel with the new texts.
  - The aerial does not turn.
  - Decided (owner, 2026-11-25): the Lake Vygozero tank stays an IS-3, although the mission spawns a Shield IS-2.
  - The Pripyat piece has a base colour texture only. The flak tower mesh has 75,000 triangles.


### Enemy map object prototypes (2026-11-25)

Pieces for the eight things the Soviet enemy can build on the map. Registered on 2026-11-25 (owner's go-ahead):
`EnemyObjectClassByType` in `Config/DefaultGame.ini` points at them. The old `MapObjects/Enemy/<Item>/BP_Enemy*`
Blueprints still exist; the old config line is kept in
`Saved/WorldMapPieces/EnemyObjectClassByType_before_prototypes.txt`.

- **Assets:** `MapObjects/Enemy/Prototypes/<Item>/BP_EnemyProto_<Item>` (child of `BP_EnemyWorldObject`), mesh
  `SM_WorldPiece_Enemy<Item>`; items: HQ, Wall, Factory, Airfield, Barracks, SupplyDepot, ResearchFacility,
  FortifiedCheckpoint.
- **How they were made:** one model agent per item wrote a CPU-only Python script that makes a glb with PBR
  textures, to one contract (`Saved/WorldMapUIDesigns/EnemyObjects/BRIEF.md`: 180 cm octagon slab, front on glTF +Z,
  at most 8,000 triangles, slot and texture names, concrete left to the shared piece concrete, no marking).
  `check_enemy_model.py <Item>` checks a model against it. Each `<Item>/model/HANDOVER.md` says which game building
  the piece is based on.
- **Soviet marking:** one shared plinth under every piece (`Prototypes/Base/SM_WorldPiece_EnemyBase_A|B|C`, made by
  `make_enemy_base.py`), in three versions for the owner to pick: A dark steel with red top rim and side band,
  B dark steel with a red ring on the map round it, C all red with corner posts. The owner picked A on 2026-11-25;
  all eight Blueprints use it.
  A permanent red outline was not built: the outline is the hover highlight (`UPlayerWorldOutliner`, enemy objects
  use the Metal type), so an always-on one needs a new outline type and a C++ change.
- **Import and pictures:** `build_enemy.py` (headless), `run_lineup.ps1 -Enemy 1` (pictures `enemy_*.png` on Soviet
  ground). The pieces carry the same two piece lights as the story mission pieces.
- **Not done:** registration, briefing texts (still the parent's stand-in text), click and outline check in a
  running game, the wall as a line across a route (it is one node piece).

## 15. Buildings rework (picked 2026-11-25)

Owner picks. **Design only: nothing of this section is built yet.** The code still has the first set of section 4.

### Picks

| Building | New rule |
|---|---|
| Battery | Damages enemy divisions near it automatically. It no longer lowers enemy strength for missions. Missions in range get **one charge of a barrage**. |
| Airfield | Missions in range get **one charge of an air raid**. Three levels (below). |
| Supply depot | Gives **radixite only**. No blueprints. |
| Watch post | Unchanged: reveals fog in a radius. |

- **Blueprints are for research only.** No building gives them and no building costs them.
- **Enemy divisions can destroy the player's buildings.** The player defends on a small map against the incoming
  division, with what stands on the world map at that place.
- **Site traits** (for example rail junction, high ground, old airstrip, which make one building cheaper or stronger
  on that hex): the owner likes the idea. To be worked out later.

### Battery

- **On the map:** each enemy turn or player turn (to be decided) it damages enemy divisions inside its radius.
- **In a mission in range:** one barrage charge for the player.
- **Later:** the battery gets levels, and the level sets the barrage: light, medium or heavy. The light, medium and
  heavy barrages already exist as abilities.

### Airfield

| Level | Aircraft | Mission bonus |
|---|---|---|
| 1 | Fighters | One fighter charge |
| 2 | Fighter-bombers (Ju 87) | One Ju 87 raid charge |
| 3a | Me 410 advanced ground attack | One Me 410 raid charge |
| 3b | Heavy bomber | One heavy bomber raid charge |

Level 3 is a choice between 3a and 3b; an airfield cannot have both.

### Building defence

- An enemy division that reaches a building attacks it.
- The fight is a **small map** where the player defends. The player's side is what the world map has at that place.
- Losing the defence destroys the building.

### Follows from the picks (to change when this is built)

- `SupplyDepotIncomePerTurn.Blueprints` goes to 0; the airfield and battery cost loses its blueprint part.
- The strength reduction of airfield and battery in the strength estimation is removed.
  For the airfield this is the writer's reading of the pick ("it gives that air raid as one charge"); confirm.
- Commander passives that lean on the old rules need a new meaning: Luftwaffe Liaison (airfield range) still fits;
  the battery strength reduction passive does not.
- `UWorldEnemyTurnDirector` needs a new action: send a division at a player building.
- The hex plate has one sign per building; levels need a mark on the plate (pips or a changed sign).

### Open

1. Battery: damage per turn, radius, and whether it can kill a division or only weaken it.
2. Battery levels: how many, and the upgrade price.
3. Airfield: upgrade prices; what the fighter charge does in a mission (air cover against enemy aircraft, or a strafe).
4. Charges from several buildings in range: do they add up, or is it one per type?
5. Building defence: does it use up the player's operation for the turn, or is it an extra fight? What does the player
   field on the small map (a fixed garrison per building and level, or units the player picked)? Can the player decline
   and give the building up?
6. Does the enemy raid every building type, or only the ones that hurt it (battery, airfield)?
7. Whether the other building ideas come in: field workshop, direction-finding station, strongpoint, flak position,
   heating station against the freeze.
8. Site traits.

## 16. Battery fire mission (built 2026-11-25)

Owner picks of 2026-11-25. This replaces the "damages enemy divisions near it automatically" line of section 15 for
the battery: the player orders the fire. The battery's passive (20 % less enemy strength for targets in its support
radius) is kept for now, against the note in section 15.

### Rules

- Open a built battery, press FIRE MISSION, click an enemy division or convoy inside the range ring. Right click or
  CANCEL ends the pick.
- One mission per battery per player turn, free. A mission destroys whole roster entries (2 by default), the last
  ones of the roster. A division with no entry left is removed. It works on tank divisions, squad divisions and
  convoys.
- A target must be enemy-owned, inside the range (3000, measured on the map plane) and in sight: divisions are no fog
  participants, so the fog mask is asked whether the place they stand on is revealed
  (`AWorldFowManager::GetIsWorldLocationRevealed`).
- A division destroyed by a battery is added to the enemy's "recently struck" places, like one beaten in an operation.

### Where it lives

| Part | Where | Notes |
|---|---|---|
| Rules | `WorldCampaign/BatteryFire/WorldBatteryFireRules` | Pure functions: every refusal as `EWorldBatteryFireResult`, valid targets, casualties, which shell kills which tank, the arc, the timing. Tests `RTS.WorldCampaign.BatteryFire.*`. |
| Settings | `UWorldBatteryFireSettings`, page "World Campaign Battery Fire" | Numbers, timing, tank piece sizes and every asset as a soft path. An unset or missing asset skips that effect and is logged once. |
| State | `FWorldCampaignProgressState::BatteryFire` | Missions fired per build node id, stamped with the turn. Records of another turn count as zero, and they are cleared at the start of a new player turn. |
| Director | `UWorldBatteryFireDirector` on the world controller | Checks the rules, marks the battery, applies the damage, saves, and only then plays the show. Also holds the target pick. |
| Formation | `UWorldDivisionTankFormation` on `AWorldTankDivision` | One small tank per roster entry, in rows facing the division's forward direction. Slot i is roster entry i. |
| UI | `UW_WorldBuildSitePanel`, `UW_WorldBatteryFirePrompt` | The button on a built battery's plate (off with the reason under it and as tooltip), and the prompt plate while picking. |
| Capture | `UWorldBatteryFireCaptureSubsystem` | `-WorldCampaignBatteryFireCapture`, see below. |

### The show

- About four seconds: 0.4 s for the camera to frame gun and target, five shells 0.35 s apart, 1.6 s of flight each
  on an arc, 0.6 s on the result. Map clicks, camera input and the campaign widgets are blocked for that time
  (`AWorldPlayerController::SetIsInputBlockedForMapShow`), then the camera goes back.
- The show is cosmetic. Damage, the fired mark and the save are done before it starts. An emptied division leaves the
  division manager and the save at once (`UWorldDivisionManager::ReleaseDivisionForDeferredDestroy`); only its actor
  stays for the show and is destroyed when the show ends, or after 30 s at the latest.
- Without a renderer (headless runs) no show plays and everything is finished at once.
- The shells leave from the socket `Muzzle` of the battery piece mesh when it has one (it has none today), else from
  26 units above the build node.
- Tank divisions: the destroyed tanks keep standing until their shell lands. With five shells and two casualties the
  second and the fifth shell kill a tank; the other shells land within the scatter radius. A destroyed tank gets the
  explosion effect, then its wreck mesh (and the optional fire), or is hidden when no wreck mesh exists. Wrecks are
  not saved and go when the next player turn starts.
- Squad divisions and convoys keep their look; all shells land around them.

### Tank formation

- The division Blueprint `BP_Division_SovietMediumTanks` has one static mesh (the T-34 training preview at scale
  0.1, about 34 x 68 x 26 units, collision BlockAllDynamic), which is what takes the cursor. The strength number is
  not on the actor; `UW_WorldMapTags` draws it.
- When at least one tank mesh loads, the Blueprint's own meshes are hidden (visibility only, their collision stays)
  and a query-only box around the formation is added, so every tank answers the cursor. The tanks themselves have no
  collision. When no mesh loads, nothing changes.
- Each tank is scaled so its longest side is `TankPieceTargetLength` (22 units), whatever size the mesh was made at,
  and a mesh whose long side lies along Y is turned by -90 degrees. `TankPieceYawOffset` corrects a mesh that then
  looks backwards.
- Mesh per subtype: `/Game/RTS_Survival/WorldCampaign/Divisions/TankPieces/SM_DivisionTank_<T26|BT7|T34_76|T34_85|IS1|IS2|IS3|KV1>`
  and `SM_DivisionTankWreck_<same>` (all sixteen were in the project when this was checked). A subtype without a mesh
  shows the fallback mesh (the T-34 training preview).

### Capture run

`-WorldCampaignBatteryFireCapture -RTSWorldCampaignSaveSlot=<name>` on the world campaign map: builds a battery on
the open site nearest the HQ, finds or spawns an enemy medium tank division 700 units away, opens the fog there,
frames both, fires, and writes `Saved/WorldCampaignUICapture/batteryfire_0_before.png` and
`batteryfire_1_after_0s6.png`, `_2_after_1s5`, `_3_after_2s5`, `_4_after_4s5`, then quits. It refuses to run without
the save slot switch.

### Checked and not checked

- Checked: DebugGame editor build; `RTS.WorldCampaign.*` 77 of 77 pass (71 older, 6 new); one headless (`-nullrhi`)
  run of the capture switch, read from its log: battery built on `LowCountries_01`, an enemy tank division of five
  entries found 790 units away with five formation slots, mission carried out (five entries to three), battery
  spent afterwards (`NoMissionsLeft`). The capture switch makes the show's timeline run without a
  renderer: it ended after 4.00 s with two wrecks and input given back, and the four picture times were reached.
  Nothing was drawn.
- Not checked: anything on screen. No picture was made. Effect sizes, the trail parameters, the tank orientation,
  the camera framing, the prompt and the button layout, and every mouse path (button, pick, wrong click, right
  click) are untested.

- **Tuning after the first captures (2026-11-25):** the values that differ from the C++ defaults are in
  `Config/DefaultGame.ini`, section `[/Script/RTS_Survival.WorldBatteryFireSettings]`: larger tanks (target length 40,
  spacing 30 / 48), the muzzle effect replaced by a small howitzer impact (the game's `NS_MuzzleFlash_Arty_1` ignores
  scale: its smoke crossed the map and it threw green puffs), a wider shell trail and a small sphere as the shell.
  Capture and tuning: `Source/Tools/WorldMapPieces/run_batteryfire_capture.ps1 -Set "Name=Value;..." -Tag <name>`
  (overrides for one run, no rebuild). The shell is now a mesh of its own, `WorldCampaign/BatteryFire/SM_WorldMapShell` with the additive glow
  material `M_WorldMapShell` (made by `Source/Tools/WorldMapPieces/make_map_shell.py`; nose on +X, tail fading along
  U), set as `ShellMesh` in the ini; it shows clearly in flight. Open: the camera does not visibly
  reframe; the Soviet tank pieces carry the game's red and white camo, which reads as red blocks at map distance.
- **Division plate (2026-11-25):** the owner found the tanks blend into the Soviet country shader, which must stay as
  it is. Each tank division now stands on a dark steel plate with a thin red rim
  (`Divisions/TankPieces/SM_DivisionPlate`, made by `Source/Tools/WorldMapPieces/make_division_plate.py`).
  `UWorldDivisionTankFormation::CreatePlate` stretches the 100-unit block to the formation footprint; settings
  `FormationPlateMesh`, `FormationPlatePadding` (26) and `FormationPlateHeight` (4). An empty mesh setting means no
  plate. The plate never takes the cursor trace: the click box does. Seen in one DebugGame capture.

## 17. Fog hints: roads and unknown missions under fog (built 2026-11-25)

Owner decisions after playing: the whole road network shows faintly under fog; a mission under fog shows as a
plain grey marker with the hover tag "Unidentified" and nothing else; enemy buildings and divisions stay fully
hidden; a mission the player has vision of is unchanged.

### How the fog hides things (found while building)

- No fog state hides an actor. The fog is one translucent cloud mesh (`BP_WorldFowCloud`, top at Z 160 on the
  campaign map) whose mask opens it. The four states only decide the mask channel, the click and the outline:

| State | Mask | Click | Outline and name plate |
|---|---|---|---|
| NotVisible | none: under the cloud | no | no |
| Explorable | explorable channel: cloud partly open | yes | yes |
| Visible | visible channel: cloud open | yes | yes |
| POIVisible | POI channel around the object | no | yes |

- The road material (`M_WorldConnection`) and the build site plate are translucent, like the cloud. Whether they
  drew over or under the fog depended on the camera position (translucent sort order). That is why some roads
  showed bright through the fog in earlier pictures.

### What was built

- **Roads.** `AWorldConnectionSplineRenderer` makes a second, dim ribbon for every road, flat at the top of the
  cloud plus `HeightAboveFog`, with a higher translucent sort priority than the cloud. It is cut in pieces of at
  most `FogRoadSegmentLength`; a piece shows when its connection is NotVisible and the fog mask is closed at that
  piece. The real road of a NotVisible connection gets a sort priority below the cloud, so it no longer shines
  through. Roads of Explorable and Visible connections are untouched.
- **Missions.** A NotVisible mission that is not won, standing where the fog mask is closed, hides every component
  of its piece (meshes, both lights) and switches their collision off, and shows a
  `UWorldUnknownMissionMarkerComponent`: the engine cylinder as a low puck with `M_WorldPiece_Flat` in one grey,
  the same for every mission, on top of the cloud. When the state or the mask changes, the piece and its collision
  come back and the puck goes. This runs on every fog pass, so a loaded campaign starts right.
- **Hover tag.** The world controller remembers the mission whose puck the cursor trace hit
  (`GetHoveredUnknownMission`); `UW_WorldMapTags` shows one plate with the hover text under it. No outline, no
  click, no briefing: those were already refused for NotVisible.
- **Enemy objects and divisions.** Not touched. They are still hidden by the cloud alone.
- **Why above the fog and not on the ground:** a road on the ground drawn over the fog would be cut by every
  hidden enemy piece standing on it, and show its outline.

### Where it lives

`WorldFow/WorldFogHintSettings` (project settings page "World Campaign Fog Hints"), `WorldFow/WorldFogHintRules`
(pure rules), `WorldFow/WorldUnknownMissionMarkerComponent`, `AWorldMissionObject` (switching),
`AWorldConnectionSplineRenderer` and `AWorldConnectionRibbon` (dim roads), `AWorldFowManager::OnWorldFowRefreshed`,
`UWorldMapFowComponent::OnFowStateApplied`, `AWorldFowCloud::GetFogTopZ`.

| Setting | Default |
|---|---|
| `bShowRoadsUnderFog` | true |
| `FogRoadBrightness` / `FogRoadOpacity` | 0.4 / 0.85 of the road's `Color` parameter |
| `FogRoadWidthScale` | 0.6 |
| `FogRoadSegmentLength` | 300 |
| `FogRoadSortPriority` / `RoadBelowFogSortPriority` | 10 / -10 (the cloud is 0) |
| `bShowUnknownMissionMarkers` | true |
| `UnknownMissionMarkerColor` | grey 0.36 |
| `UnknownMissionMarkerDiameter` / `Height` | 170 / 22 |
| `UnknownMissionHoverText` | "Unidentified" |
| `UnknownMissionMarkerMesh` / `Material` | `/Engine/BasicShapes/Cylinder` / `M_WorldPiece_Flat` |
| `HeightAboveFog` | 6 |

### Checked and not checked

- Checked: DebugGame editor build; `RTS.WorldCampaign.*` 83 of 83 pass (6 new, `RTS.WorldCampaign.FogHints.*`);
  two headless (`-nullrhi`) runs of the map on their own save slot, new campaign and load, read from the log line
  `RTS_WORLDFOGHINTS`: 17 missions, 2 as piece, 15 as marker, 20 enemy objects and 6
  divisions with no marker, the nearest marker 361 units from the HQ in the probe's campaign, 52 roads in 447 dim pieces of which 423 shown. A third and fourth run (launch on the Berlin pin, then a simulated victory) showed the switch back: 13 markers, 4 pieces and 372 dim road pieces after the fog opened.
- Not checked: anything on screen, the hover tag, the place of the puck, and a mission inside open fog showing its
  piece (rule test only). All missions get the marker, not only story missions. Open points for the first picture:
  - The dim roads and the pucks lie about 166 units above the ground, so the tilted camera shifts them a little
    against the ground at the edge of the fog.
  - Whether the `Color` alpha of the road material drives its opacity; if not, `FogRoadOpacity` does nothing.
  - The puck is lit by the level light; its grey may need another value.
  - The build site plates still draw over or under the fog by camera position, as before.
- **Fog hints, owner's tuning (2026-11-25):** after the first capture the owner asked for the roads to show clearly
  through the fog (roads only) and for smaller markers. `Config/DefaultGame.ini`, section
  `[/Script/RTS_Survival.WorldFogHintSettings]`: road brightness, opacity and width at 1.0; marker diameter 100,
  height 14, dark grey. Open: whether unseen missions should show their real piece instead of the plain marker.

### Change 2026-11-25 (evening): unidentified missions show their real piece

Owner: "unseen missions should reveal their real piece through the fog". The plain marker stays as an alternative.

- **Way chosen: an opening in the cloud mask.** `AWorldFowManager::ApplyMissionFogHints` stamps a small circle into
  the visible channel of the mask around every unidentified mission, after the real reveal has been copied to
  `M_RevealedPixels`. `GetIsWorldLocationRevealed` (battery fire, reveal rules) answers from that copy, so the
  opening is drawn only. The mission's fog state stays NotVisible: no click, no briefing, no name plate, no
  outline. The piece stands at its real place with its own lights; nothing is lifted.
- **Presentation** is decided per fog pass by `WorldFogHintRules::GetMissionFogPresentation` and given to the
  mission (`AWorldMissionObject::ApplyFogPresentation`): as its fog state, plain marker, or real piece through fog.
- **Enemy pieces stay hidden.** A mission gets the plain marker instead of an opening when a hidden enemy object or
  division stands within opening radius + soft edge + one mask pixel + `EnemyPieceHalfWidth` of it (307 units with
  the defaults). The fog is also refreshed at the start of every player turn, after the enemy pieces moved.
- **Hover tag** now also shows on the piece seen through the fog. Dim road pieces inside an opening are hidden; the
  real road shows there.
- **Settings added:** `UnknownMissionLook` (RealPiece; PlainMarker), `MissionOpeningRadius` 120,
  `MissionOpeningFalloff` 50, `EnemyPieceHalfWidth` 100. The owner's tuning of roads and marker is in
  `Config/DefaultGame.ini`, section `[/Script/RTS_Survival.WorldFogHintSettings]`.
- **Checked:** DebugGame build; `RTS.WorldCampaign.*` 85 of 85 (8 fog hint tests); two headless runs: new campaign
  14 missions as real piece, 1 as marker (a hidden enemy piece 125 units from it), 2 identified, 314 of 447 dim road
  pieces; after a simulated Berlin victory on load 13 as real piece, 0 markers, 4 identified, nearest hidden enemy
  to an opening 308 units.
- **Not checked:** anything on screen: how clear and how wide the opening looks (one mask pixel is about 37 units),
  whether the ground and country fill inside it give anything away, the hover tag on the piece.
