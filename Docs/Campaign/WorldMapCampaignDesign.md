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
| Buildings | **Three roles for now**, to be expanded later: watch post (reveals fog), supply depot (radixite and blueprints per turn), airfield or battery (lowers enemy strength for missions in range). |
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
