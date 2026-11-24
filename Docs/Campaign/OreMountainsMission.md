# Ore Mountains mission (design draft, working title)

Status: flow designed (26 owner picks on 2026-11-22, written up 2026-11-23 in "Mission structure"); mission scripting
not started. Built: the map (`Ger_2ndOreMountains`, 570 x 570 m, see [OreMountainsMap.md](OreMountainsMap.md)), the
clue and intercept triggers, the briefing voice lines and the `ASurrenderingCargoTruck` C++ (not play-tested). No
mission Blueprints, convoy route or Soviet sites in the map yet. Started 2026-11-13. This is the **second mission** of
the campaign.
Lore reference: `Source/DesignTexts/Lore.txt`. Related: [BerlinMission.md](BerlinMission.md) (the mission that follows).
Every item marked **(open)** is undecided.

## Where the files are

- **This doc:** `Source/Docs/Campaign/OreMountainsMission.md` (Source is a submodule; commit from inside `Source`).
- **Layout sketch (v5, current):** `MapDesigner/OreMountains/ore_01.layout.json` (outer repo root). Older versions kept next to
  it as `ore_01_v1` to `ore_01_v4.layout.json`. Regenerate the renders from `MapDesigner/` with
  `python md.py all OreMountains/ore_01.layout.json --out OreMountains/out` (`out/` is gitignored).
- **Tracked render copies:** `Source/Docs/Campaign/OreMountainsMission_layout_v5.png` and
  `OreMountainsMission_layout_v5_report.md`.
- **Spec generators:** `MapDesigner/OreMountains/generators/` (`gen_ore.py` to `gen_ore5.py`, each version built from the
  previous one by `make_gen2.py` to `make_gen5.py`). The spec polygons have about 90 vertices each, so change the layout in
  the latest generator (`gen_ore5.py <out.json>`), not the JSON by hand.
- **Sketch facts (v5; the built map is 570 x 570 m):** map 850 x 850 m (units cm, +X north, +Y east), Czech border and ridge on the **south** edge,
  convoy road 868 m. The tool features it relies on (slope passability per unit class, curved graded ramps) are described
  in `MapDesigner/AGENTS.md`.

## One-line pitch

The resistance holds a base on an intact ridge in the Ore Mountains and hits Soviet supply convoys running along the
valley below, and the two small Soviet depots that feed them.

## Place in the campaign

- Second mission. Set in East Germany, **not near Berlin**, so the ridge is still intact. (Berlin took a single nuclear
  strike in January 1945.)
- **The Ore Mountains mission comes first, the Berlin mission after.**
  - This mission creates the **Berlin intel hook**: the special convoy carries something that shows the Forge survey team
    at the Berlin crater, which gives the resistance its reason to go.
  - It gives the campaign the right contrast: first a **power-fantasy mission with a base and tanks**, then the desperate
    small-force stealth raid in Berlin.

## Location: the Ore Mountains (Erzgebirge), Saxony

Roughly 200 km south of Berlin. A long ridge along the Czech border, overlooking the Dresden-Chemnitz road corridor (the
A4 line), which is a natural convoy route west.

### Why this location (lore)

- **The ridge survived.** The Svarog firebreaks were nuked as a line across the lowlands, so the mountains in the south-east
  of old East Germany could have been left standing. This is a working assumption and can be changed. **(open)**
- **The Wismut mines.** The Ore Mountains have a real history of Soviet uranium mining (the Wismut mines). The CSD has
  reused old mine and processing sites as depots or storage, which is what the Soviet bases are here. Uranium also echoes
  the "why did they stop using nukes" question without answering it.
- **The mine tunnels** give the resistance base a believable hideout, and a small foreshadowing of the Burrowers faction.
- **The supply route.** The Soviets supply their remaining western outposts along the main east-west corridors, and this
  valley road is one of them.

## Map character

- **Tank-friendly.** Large open areas, not a cramped or vertical map, but with **various levels of elevation**: the
  ridge, foothills and valley floor are distinct tiers that give high-ground positions and a reason to move.
- The ridge faces a long valley, so convoys are visible at a distance and can be ambushed from the high ground.
- **Map size: 1.67 times the linear size of Ger_MarderStugMission, and square** (confirmed as linear, not area).
  Ger_MarderStugMission is 510 x 510 m, so the map is about **850 x 850 m** (about 0.72 km², roughly 2.8 times the area
  of Ger_MarderStugMission). **The built map is 570 x 570 m** (850 m was too large, 2026-11-13); the sketch numbers
  in this doc (ranges, distances) are from the 850 m sketch and need retuning.
- **The mission sits along the Czech border.** One edge of the map is the border, with the ridge along it; the valley
  and the convoy road run beside it. **(open)** which edge (the ridge and border side is the south or south-east in the
  real geography, with the corridor running along it toward the north-west) and what the border edge looks like in-game.
- **The three tiers:** the player's **ridge on the south** (Czech border side), the **road in the valley floor**, and the
  **foothills to the north**.
- **(open)** whether it is a single valley or has side valleys.

### Resources

The map has to **contain resources, designed into the layout**: **a quite large Radixite field and metal as well.**
Tuning (amounts, positions, how far they sit from the base) comes later. **(open)**

### Wismut spoil heaps: metal expansions (decided 2026-11-18)

The real Ore Mountains are dotted with huge grey Wismut spoil heaps (*Halden*). They are the map's **metal nodes**, and
they are part of the exploration push: this is the first real base-building mission, and it should teach that the
economy grows by **taking ground**, not by turtling. The supply point mechanic is **not** used here; it is kept for
Berlin, where it is introduced.

**Model picked: capture station + harvest.**
1. Each far heap has an old Wismut **ore loading station** beside it (an ore bin or loading chute with a small
   headframe). **Infantry capture the station.**
2. Once captured, the station is a **forward drop-off**, so harvesters can work the heap with short trips.
3. Soviet sites near a station send a response to retake it, so the player has to defend it.

Capture claims the heap, defence holds it, and the harvesters work it. Metal is still gathered by harvesters, as the
intro mission teaches (one harvester gathers Radixite and metal there), so this adds no second economy model.

**Layout and pacing:**
- **One easy heap near the ridge** (the bench metal, reached by the narrow mine railway) needs no capture. It is small
  and runs dry around the middle of the mission.
- **The rich heaps sit out near the Soviet sites and in the north foothills**, each with a capture station. Only they
  keep paying after the easy heap is gone, which pushes the player out at the right time.
- The sketch has 2 near and 3 far metal spots, and the built map already has spoil-heap terrain (RockDesert paint, scree
  and stones). **Decided 2026-11-22:** 1 easy heap (no capture) + **2 capture stations**, one in the north foothills and
  one near Base B. **(open)** the amounts.

**Extra touches:**
- **Show the heap emptying:** if the heap mesh can step down as it empties (`ResourceStorageLevel` suggests stages), the
  player can see it go.
- **Soviet harvesters already work the far heaps.** A heap near Base B has a Soviet harvester on it (the T-34 Breaker
  pile-driver harvester exists). That shows the heap is worth taking, and the player can kill it or chase it off.
- **A voice line when a station is captured,** in the style of the intercepted radio lines.

**Implementation notes (checked in code 2026-11-18, not built):**
- `ACaptureActor` (`CaptureMechanic/CaptureActor`) already has infantry capture with settings, a progress bar and the
  Blueprint event `BP_OnCapturedByPlayer(Player)`. Squads call `ICaptureInterface::OnCaptureByPlayer`
  (`SquadController.cpp`).
- `UResourceDropOff` has a Blueprint-callable `SetIsDropOffActive(bool)` and `InitResourceDropOff(...)`. A Blueprint child
  of `ACaptureActor` with a drop-off component that starts inactive and is switched on in `BP_OnCapturedByPlayer` may
  need **no C++**. **(open)** check this before building:
  - The drop-off registers with the game state at `BeginPlay` and uses player controller 0. Confirm that harvesters
    really skip an inactive drop-off, and that the drop-off counts as the player's.
  - Only player squads call the capture today. Retaking by the Soviets means either AI capture or destroying the station
    through its optional HealthComponent (and switching the drop-off off again). **Decided 2026-11-22: the Soviets
    retake a station by destroying it**; the player repairs or recaptures it.
  - Whether a heap should be harvestable before its station is captured (long hauls to the base) or blocked until then.

## The player's base

- The player **starts with a base on the ridge**, built into the old mine tunnels.
- **The base can be expanded.** It starts with an **expanded HQ, a Gamma Facility, a refinery and a barracks.**
- **Starting force (decided 2026-11-22): by faction**, as in the intro mission: Strike Division and Breakthrough get
  different starting sets (for example Pz II/III against Pz IV). It drives out of the adits after the briefing.
  **(open)** the exact units per faction.
- **Base-building kit (decided 2026-11-22): everything** is available from the start; the player chooses the build.

## The enemy

### Convoys (the main point of the mission)

- **Count:** about **six convoys** over the mission, each with **4 to 6 trucks**, coming from the **east end of the valley**
  and driving **west** along the valley road below the ridge. Road only, no rail line.
- **Escort:** the first convoys are **lightly escorted** (a car or two). Later ones add **armoured cars** and then a few
  **tanks**. The **last one is a heavy convoy with a real escort**. Escort size **scales with difficulty**.
- **Success (replaced 2026-11-22):** there is no convoy count to reach and no "supply delivered" fail meter. **An
  escaped convoy never loses the mission**; its cargo makes the Soviets stronger by type (see "Mission structure").
  The win is Base B destroyed and the Berlin cases recovered.
- **Bases (replaced 2026-11-22):** Base A is a loot-only bonus with no effect on the convoys; Base B is the road
  overwatch and radio command post (see "Soviet bases").

### What the convoys carry

- Most are **ordinary supply: fuel, ammo and parts**. This is what the western outposts need.
- **One special convoy carries sealed cases that connect to the Forge survey: sample canisters and documents marked for
  Berlin.** Wiping it out and **recovering the cargo is the Berlin hook**, and it is a good **climax** for the mission.
  **Decided 2026-11-22: convoy 5, disguised as an ordinary convoy, with an armoured case truck** (see "Mission
  structure" for recovery and what happens if it escapes).
- The convoy is **not the first time the player sees the Berlin crates.** An early clue comes from the radio outpost (see
  "Radio outpost with the Berlin crates" below), so the special convoy pays off something the player already knows about.

### Capturing convoy trucks (decided 2026-11-16)

This is the core loop of the mission. It answers "why do the convoys matter to *us*": the resistance needs what is on
those trucks, and the base grows from the raids.

- **Kill the escort and the trucks stop and surrender.** A truck that surrenders is no longer a Soviet unit.
- **A captured truck drives itself to the mine adit** (the "mine adit drop-off" by the Gamma Facility in the sketch) and
  **pays out its cargo when it arrives**: fuel, vehicle parts or Radixite, by the convoy's cargo type. The player cannot
  steer it. It pays **only on arrival**. **Decided 2026-11-22: nothing chases a captured truck**; the capture is the
  risky part and the trip is safe (a battery barrage that happens to land on it can still kill it).
- **The choice on every convoy: a fast kill or slower, riskier loot.**
  - **Destroy:** each dead truck already drops a scavengeable cargo cache (existing convoy behaviour,
    `FConvoyCargoSettings::CacheClass`: about 5 minutes before it is gone, about 12 s for a squad to scavenge). Fast and
    safe, but the player gets a share and has to send squads into the kill zone to pick it up.
  - **Capture:** the full cargo, but the player has to kill the escort **without** killing the trucks, then cover the
    trucks while they drive up to the adit.
- A captured truck counts as **stopped**: its cargo does not reach the Soviets (see "Escaped cargo" in "Mission
  structure").
- **The power fantasy grows from the raids.** Keep the starting resources low so that captured cargo is what pays for
  the base expansion and the tanks.

**Implementation requirement: a new derived class for cargo trucks that can surrender.**

- We want a **new C++ class derived from the class the convoy trucks use today** (convoy trucks are `ATankMaster`
  children, spawned from `FConvoyTruckSpec::TruckClass`). Built as `ASurrenderingCargoTruck`.
- **Only trucks of this class can surrender.** The Ore Mountains convoy definitions point their `TruckClass` at Blueprint
  children of this class.
- **No other cargo truck in the game gets this feature.** The intro mission's fuel convoy (`DA_Convoy_IntroFuel`), other
  convoys, survival-mode convoys and the supply point's Opel truck (`BP_Ger_OpelSupplyTruck`) keep their current
  behaviour. Do **not** add a switch on `AConvoy` or `UConvoyDefinition` that turns surrender on for every convoy; the
  class is the switch.
- **Hand-off after surrender:** the truck changes owner to the player, and `AConvoy` releases it (it is no longer a
  member, and it does not count as lost or escaped). It then reuses the supply-point truck logic (`USupplyTruckComponent`:
  drive to a target, pay the cargo on arrival), with the mine adit as the target instead of the HQ.
- **Built in C++ 2026-11-18, made hard to capture / easy to destroy the same day (compiles, not play-tested):**
  `Enemy/Convoy/SurrenderingCargoTruck/` (`SurrenderingCargoTruck`, derived from `ATrackedTankMaster`, and the
  countdown widget `W_SurrenderingTruckCountdown`). Rules and defaults:
  - **Trigger:** the last escort of the convoy dies while the column drives. A convoy that never had an escort starts
    when a player unit comes within 30 m or a truck is hit (`UnescortedAlertRadius`).
  - **Scuttle countdown, 12 s** (`CountdownSeconds`): the truck stops (`bStopDuringCountdown`) and shows a
    screen-facing "SCUTTLING n" plate with a shrinking red bar (pure C++, no asset; hidden by the fog of war like
    the health bar).
  - **Only infantry captures** (`Captors = SquadsOnly`; vehicles optional): a player squad within 20 m
    (`CaptorSearchRadius`) before the countdown ends makes the truck surrender. A truck below 60 % health
    (`MinHealthFractionToSurrender`) cannot be taken and only scuttles.
  - **Nobody in time:** the crew blows the truck up through the normal damage path (`BP_OnScuttled`, optional
    Niagara + sound), so the convoy counts it as destroyed and drops its normal cargo cache. A failed capture is a
    destroy. A truck shot dead during the countdown is an ordinary destroyed truck.
  - **Captured:** it stands 3 s, then drives to the nearest actor tagged `CapturedTruckDropOff` (or
    `SetDropOffTarget`) and pays **2.5x** the convoy's per-truck cargo there (`CapturedCargoMultiplier`; only Radixite,
    metal and vehicle parts are carried). It pays only on arrival.
  - Convoy hooks: `AConvoy::ReleaseCapturedTruck`, new outcome `TrucksStopped`, route counts
    `GetCapturedTruckCount` / `GetStoppedConvoyCount` and `OnConvoyTruckCaptured`. Blueprint hooks:
    `BP_OnScuttleCountdownStarted`, `BP_OnScuttled`, `BP_OnSurrendered`.
  - Still to do: the Blueprint child (it does not inherit `RTS_BP_TrackedTankMaster`, so copy its AI controller and
    setup), the Ore convoy definitions, and the drop-off marker at the adit.
- The trigger, the damaged-truck rule and the countdown are settled by the code above; nothing chases a captured truck
  (2026-11-22). **(open)** what shows the surrender (white flag, voice line, minimap icon change), the payout per truck,
  the truck's route up the main ramp to the adit, and fuel: convoys can carry `Fuel`, `VehicleParts` and `Ammo`
  (`EConvoyCargoType`), but the captured-truck payout only knows Radixite, metal and vehicle parts, so a captured fuel
  or ammo truck needs a mapping (for example fuel to Radixite).

### Convoy line-up (first three decided 2026-11-16)

Each convoy is a different problem, not the same ambush with a bigger escort.

1. **Light fuel run: a guided first ambush.** A few fuel trucks and a light escort (a car or two). The mission guides the
   player through it: where to set the ambush, and that killing the escort makes the trucks surrender. This is where
   capture versus destroy is taught. **Taught by (2026-11-22):** an ambush hint marker below the ridge; when the escort
   dies a "Capturing trucks" field-manual takeover explains the countdown, the 20 m squad rule and the 60 % rule; the
   Advisor coaches.
2. **The weighbridge convoy: a set-piece window.** This convoy **stops at the weighbridge checkpoint** for an inspection,
   which gives the player a fixed time and place to strike. The checkpoint guards and towers join the fight.
   **Decided 2026-11-22: a 60 s stop.** Strike early and the convoy floors it forward; miss the window and it drives on.
   Note: the weighbridge, booth and towers are still placeholders on the map, and the checkpoint has no collision yet.
3. **The ammo convoy that splits: two targets, pick one.** Part of the column (the **ammo trucks**) turns off toward the
   **artillery battery** in the north-east, while the rest drives on west. The player chooses: stop the ammo trucks and
   deny the barrage, or stop the supply trucks so their cargo does not strengthen the bases. **(open)** whether the
   battery still gets other ammo trucks on their own route as well, and whether captured ammo trucks pay out something
   the player can use.
4. **The hunter convoy (decided 2026-11-22):** its trucks carry infantry that dismount at the ambush zone and climb
   toward the ridge to hit the base.
5. **The case convoy (decided 2026-11-22):** looks like an ordinary convoy but carries the **armoured case truck** with
   the Berlin cases (see "Mission structure").
6. **The heavy convoy (decided 2026-11-22):** a heavy column that reinforces Base B; it leads into the finale.

Convoys 1 to 3 start from mission stages; convoys 4 to 6 run on a timer (see "Mission structure").

### Radio outpost with the Berlin crates (decided 2026-11-16)

A **small Soviet outpost with a radio antenna** that holds **some crates marked for Berlin**. It is the **early clue** for the
Berlin hook: the player finds the crates before the special convoy, so the climax pays off something already set up.

- **Guards: only a few infantry.** A few **Mosin squads** (`Squad_Rus_Mosin`) and **one bazooka hazmat squad**
  (`Squad_Rus_HazmatBazooka`), **and that is it**: no vehicles, no guns, no bunkers. It is meant as a small, early side
  target, not a third base.
- **Decided 2026-11-22:** it sits in the **north foothills near the Forge survey post**. Taking it plays a **manifest
  takeover** that marks convoy 5 as the case convoy and adds the "Recover the cases" major objective. **(open)** the
  exact spot, and whether the radio does anything in play while it stands.
- Check before building: the survival-mode base docs say `Squad_Rus_HazmatBazooka` is **not** in that mode's base spawner
  map. Confirm that the placement actor used here (for example `AEnemyStrongpoint`) can spawn it.

### Soviet bases (secondary targets)

**Two small bases**, in old Wismut mine and processing sites, as depots or storage that feed the convoys.

- **Base A, at the east end of the valley:** this is where the convoys originate. **Light defences**: MG nests, a couple
  of infantry squads and two light tanks. It is close to the start, so it can be hit early.
- **Base B, on the foothills across the valley from the player's ridge:** it overlooks the road and is **better defended**,
  with AT guns and some infantry on the high ground. It forces a **flanking approach** and uses the elevation tiers.
- **Decided 2026-11-22:**
  - **Base A is a bonus, loot only.** Destroying it gives resources and has no effect on the convoys. The convoy
    route must not use it as its owning building (`bHasOwningBuilding` off), or destroying it would stop the convoys.
  - **Base B is a major objective and the finale.** Before that it is the **road overwatch** (its AT guns cover the road
    below the knoll, so ambushes near it are costly) and the **radio command post** (while it stands, convoys get
    warnings and speed up through ambush zones; its radio mast is the tell). It receives the escaped-cargo effects and
    convoy 6.
- **(open)** the exact defences.

### Soviet artillery battery (top right)

Added 2026-11-13. A Soviet **artillery battery in the top right (north-east) of the map**, built from the **various
weapon pits the game already has** (for example the Rus pits: 53-K, BS-3, 120 mm mortar, DShK and Ampulomet).

- **Ammo trucks drive to the battery.** Separate from the supply convoys on the valley road.
- **Supplied state:** whenever an ammo truck **arrives**, the battery gets **increased range** and **starts barraging the
  player**. Unsupplied, it has its normal (shorter) range and does not reach the player's base.
- **Counterplay:** the player can intercept the ammo trucks to deny the barrage, or destroy the battery itself.
- **Decided 2026-11-22: a timed supplied state.** Each arriving ammo truck gives about **3 min supplied**: the range
  reaches the ridge and the battery barrages player units and buildings in range. Unsupplied it cannot reach. It is a
  **third site with a light guard**; destroying it is a bonus. Escaped ammo cargo keeps it supplied longer.
- **(open)** the ammo truck route (where they start, how many, how often) and what each pit type contributes. Range
  numbers need tuning against the pits' real ranges on the 570 m map.

### Landmarks and set dressing

Added 2026-11-13, so the map is not all nature. Each has a lore job.

- **Weighbridge / checkpoint on the valley road.** The convoys pass through it. It is a natural ambush spot with a clear
  sight line (from the ridge). Convoy 2 stops at it for 60 s (decided 2026-11-22). **(open)** how it is staffed.
- **Broken viaduct or overpass from the old A4 line.** Gives the road some history. Built in the map with a 10 m road
  gap. **Decided 2026-11-22: a natural chokepoint only**: convoys slow to single file there, which makes it the prime
  ambush spot; no extra mechanic.
- **Small Forge survey post** near the Radixite field: sampling stakes with flags, a cryo canister rack and a generator.
  It matches the Forge survey station in Berlin and **foreshadows it**, so the Berlin mission feels earned when the
  special convoy's cargo turns up. **(open)** whether it has a guard, and whether it is a target or only dressing.

### Fire lookout towers (look picked 2026-11-18)

Gameplay idea (approved 2026-11-18, hookup not built): 2 or 3 old lookout towers on high points away from the player's
base. A unit kept at a tower reveals a wide area of fog and warns earlier of each Soviet convoy, with its cargo type
(fuel, ammo or parts), so the capture-or-destroy choice can be planned. It gives the player a reason to explore the map.

- **Look: option D "Schachtwarte"**, picked by the owner from six greybox concepts ("Option D ; work it out in
  blender"). A small pre-war tin-shaft trestle headframe (riveted steel, two sheave wheels) over a concrete-capped
  collar, with a roofless stone hoist house beside it; a steel grating deck laid round the sheaves at about 12 m is the
  lookout, reached by a caged ladder on the south face. Top about 14.7 m; the whole site (headframe, struts, hoist
  house, track) is about 21.5 x 13.8 m, so it needs a fairly broad summit. Lore: Wismut prospectors reopened the
  shaft in 1947, found nothing worth it and capped it; the timber roof went for firewood; someone made the highest steel
  on the hill a lookout. It must stay clearly smaller and older than the Wismut headframe at Base B.
- **Occupy point:** the squad enters the roofless hoist house (doorway on the south); lookouts show on the deck; an
  orange panel hangs from the deck rail while the tower is held.
- Concepts, brief and model: `BlenderTanks/CustomMapItems/Environment/OreLookoutTowers/` (`OreLookoutTowers_Brief.md`);
  sheets also in `Saved/OreMountains/lookout/`.
- **Blender model v1 (2026-11-18):** `OreLookoutTowers/Model/` (README there). Turned 90 degrees against the concept:
  the rope line runs east-west with the hoist house east, so the two sheaves face the default camera; door, ladder, held
  panel and lookouts face south. Main mesh about 9.4k triangles with collision and `cargo_*` / `entrance_01` sockets for
  the garrison (`UCargo`). Not in Unreal yet. **Owner approved model v1 as built (2026-11-18, "Yes, keep it"),
  including the 90-degree turn and the added detail (fallen ridge beam, north wall breach, rubble, drum, track and tub).**
- **Decided 2026-11-22: 2 towers, captured like a capture point** (`ACaptureActor`); once taken they stay the
  player's with no garrison needed. (This conflicts with alert tier 2, "retake squads at captured towers"; see
  "Mission structure".)
- **(open)** which summits (foothill tops are 4-10 m; the Base B knoll is taken), reveal radius, and how much earlier the
  warning comes and how it shows the cargo.

## Environment: what happened here

The Phytophage passed through the Ore Mountains in 1945-46, but the ridge was not nuked in Operation Svarog. Thirty years
later the place should read as a **dying mountain range**: related to, but different from, the other maps (Berlin is
glass; the Svarog line is vitrified wall).

### Vegetation

The Ore Mountains were covered in spruce forest. The Phytophage attacked cellulose and organic matter, so most of it is
now **dead or blackened standing trunks and fallen timber, not a healthy forest.**

- **Peat bog scars:** lowland and valley patches of dark, boggy ground, with the odd smouldering patch (the
  "self-combusting peat bogs" from the lore).
- **Hardy survivors:** a few patches of scrub, grass tufts and lichen in sheltered pockets, for contrast and cover.
- **Fossilised, blackened roots** visible in cuts and erosion gullies. This is the Act 1 visual clue, and it fits the ridge
  as well as Berlin.

### Ground tint

- **A dry, dusty desert tint over most of the ground** (the world is heating up), but **less extreme than the
  Radixite-heavy areas.**
- The Ore Mountains are **far from the blast**, so the ground is **lighter than the Berlin glass**, with **some cooler
  higher ground on the ridge.**

**(open)** the rest of the environment ideas (erosion gullies, contaminated stream, Radixite dead ground, hazy sky and
dust, newer Soviet additions against 30-year-old ruins) are not decided yet.

## Act 1 clues (decided and placed 2026-11-16)

This mission only **raises a question** about the Purple Death: why would a weapon meant for people kill a whole
mountain forest, down to the roots? It does not answer it. Berlin's Demeter pod stays the first famine clue.

Rules for every clue here:
- No in-game text or line says Phytophage, Demeter, defoliant, wheat, crops, farms or famine.
- Nothing links the roots or the Purple Death to Radixite: no root framed with crystals, no purple mould in the shafts
  (adit 2 has a Radixite seep beside it), and no Forge cases holding roots or soil.
- Nothing spends a later mission's material. Oder-Warthe owns the tunnel-mouth mould and the 1946 papers.
- The survey post's cryo rack is **not** an Act 1 clue. Berlin is the first look at cryo tech, and cryo is the Act 3
  mechanism, so the rack is at most unexplained set dressing here. **(open)** keep it as dressing or leave it out.

Both clues fire from a **trigger area only**: the first player unit that comes near plays one portrait line, once.
They use `BP_ScriptTrigger_PortraitLine` (`/Game/RTS_Survival/Blueprints/Missions/TriggerAreas/`), a Blueprint child of
`AScriptedTriggerActor` with instance settings `TriggerPoints` (sphere centres relative to the actor; empty = the actor
itself), `TriggerRadius` (cm), `PortraitType` and `VoiceLine` (empty = nothing plays). No C++ and no mission manager.

### A. Fossil roots: "the forest died standing"

- **Where:** all five fossil-root gullies (`ORE_Roots_*`). One trigger actor (`ORE_ClueA_Trigger_FossilRoots`) has a
  15 m sphere at each gully; the line plays at whichever gully a player unit reaches first, then all five spheres go.
- **Speaker:** a Scavenger (`GermanScavengers` portrait). Line picked 2026-11-16 (take 4 of 5): "Kicked one of these
  roots once. Broke my toe. What kind of plague does that to a tree?" SoundWave `OreMountains_ScavFossilRoots`.
  Voice: ElevenLabs "Christian - Stadium announcer" (the old "German Football Stadium" Scavenger voice), eleven_v3,
  stability 0.69, similarity 0.71, speed 1.05, through the Scavenger radio band rebuilt from the old Audacity projects
  (scripts in `VoiceLines/VoiceChanger/Output/OreRootsAudition/_tools`).
- **Look:** the root masses are now charcoal-black and stone-hard (`MI_OreClue_FossilRoots`, a darkened copy of the
  dead-forest wood) and twice as dense (a second rib set across the first, and one running up to the root plate), with
  a soot stain under them. A shattered stump stands on each root plate, and three blackened standing trunks per gully
  splay their roots down into the cut. Labels `ORE_ClueA_<n>_*`.

### B. The miners' shelter: the old Wismut shaft

- **Where:** adit 3, the east shaft of the player's ridge base (about 66 m from the HQ). The grey placeholder box of
  adit 3 was removed: this spot builds its own portal. One trigger actor (`ORE_ClueB_Trigger_MinersShelter`), 13 m
  sphere on the apron.
- **Speaker:** the announcer / Advisor (`GermanAnnouncer` portrait). Content: these are the old Wismut shafts; in 1946
  the miners sealed themselves in while the Purple Death passed overhead; the ones who stayed below lived. Line picked
  2026-11-16 (take 2 of 5): "Glück auf, says ze sign. In forty-six ze miners bolted zis door and let ze Purple Death
  walk overhead. Ze ones who stayed below outlived everyone up top." SoundWave `OreMountains_MinersShelter`
  (`make_mission_lines.py`, the standard announcer chain).
- **Look:** a plank bulkhead in a spruce-timber portal frame, with the steel air door standing open; "GLÜCK AUF /
  SCHUTZRAUM 1946" painted on the planks and rows of tally marks (days below ground) beside the door; a dim warm lamp
  just inside the doorway (somebody still keeps a light down there: a quiet hint at the people who went underground,
  never named); sandbags, stores, a first-aid box and a tipped mine cart on the apron; and a row of six stone-cairn
  graves with plank crosses for the ones who went up. Labels `ORE_ClueB_*`.

**Build:** `Source/Tools/OreMountainsMap/ore_clues.py` (`build_all()`, re-runnable in the map's throwaway editor),
decal textures from `gen_clue_decals.py`, assets under `/Game/RTS_Survival/Blueprints/Environment/OreMountains/ClueSpots`.
In-game check shots: `ore_clue_shots.py` (writes `Saved/OreMountains/clues`).

**Lines in place (2026-11-16):** both SoundWaves are under
`/Game/RTS_Survival/Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/OreMountains` (sound class
`RTS_SClass_CinematicAndTransmissions`; sources in `BlenderTanks/VoiceLines/Announcer/AAAGerMissions/OreMountains` with
dry take, script and timing sidecar), subtitles are in `DT_CinematicTranscripts`, and each trigger actor's
`VoiceLine` is set in the saved map.

**Play test:** `Source/Tools/OreMountainsMap/ore_clue_trigger_test.py` (throwaway editor, via `ore_cmd.py`). It slides a
start-force tank into root gully 3 and onto the shelter apron, and checks six spheres (5 x 15 m + 13 m), that each
line fires at the sphere edge and plays once, and that the spheres go afterwards. 9/9 passed on 2026-11-16, and the UI
screenshots showed the Scavenger and Advisor portraits with their subtitles.

### Intercepted Soviet radio (9 spots, added 2026-11-16)

Same trigger-only setup as clues A and B: one `BP_ScriptTrigger_PortraitLine` per Soviet site, `SovietMosin` portrait
("Rifleman"), Russian line through the Mosin voice chain with an English subtitle, played once when the first player
unit comes near. Each line teaches the site or seeds lore that is safe this early (the same rules as the clues above;
no Radixite origin, no cryo, no reason for Berlin).

| Spot | Trigger (`ORE_Intercept_*`) | Centre (cm) | Radius | What the line is about |
|---|---|---|---|---|
| Checkpoint | weighbridge on the road | (133, 7705) | 20 m | guards weighing every truck for the western outposts |
| BaseA | Wismut yard, convoy staging | (2010, 24120) | 25 m | loading and dispatching the columns |
| BaseB | below the knoll's AT guns | (7705, -6030) | 22 m | the gunners watch the whole road from the shaft |
| Battery | artillery battery (NE) | (18760, 13400) | 25 m | out of range until the ammo trucks arrive (teaches the mechanic) |
| Viaduct | broken A4 viaduct gap | (3685, -2948) | 18 m | one truck at a time through the gap (chokepoint) |
| SurveyPost | Forge survey post | (-5628, 11658) | 18 m | sealed sample cases stamped for Berlin; don't ask (Berlin hook) |
| AmmoRoute | ammo trucks' road | (9045, 22445) | 18 m | "why shells, the Germans are dead?" |
| ConvoyRoad | ambush corridor below the ridge | (-2814, 1842) | 20 m | nobody has lived on that ridge for thirty years (irony) |
| PeatBog | smouldering peat bog (west) | (-2211, -24120) | 18 m | the bog has burned for thirty years |

Lines picked by the owner on 2026-11-16 (from three takes per spot in
`VoiceLines/VoiceChanger/Output/OreInterceptAudition`) and set on the map. All are SoundWaves
`OreMountains_Intercept_<Spot>` in `.../AAAGerMissions/OreMountains`, with subtitles in `DT_CinematicTranscripts`:

| Spot | English subtitle |
|---|---|
| Checkpoint | "Overweight again! Who loaded this, a blind man? Take off two crates, or you're not going any further." |
| Checkpoint, 2nd line (`_B`) | "Weighbridge here. Convoy's through, all twelve weighed. Putting the kettle on." |
| BaseA | "Yard here, over. Third convoy is loaded, waiting on the drivers. Where have they wandered off to now?" |
| BaseA, 2nd line (`_B`) | "Shells in the lead trucks, fuel at the back! How many times do I have to say it?" |
| BaseB | "I sit over an old mine shaft and watch the road. I've got a gun, and nobody to shoot." |
| Battery | "The shell trucks are on their way! Once we unload, we'll blanket the whole ridge." |
| Viaduct | "Old autobahn bridge, half of it gone. The convoy waits until every truck squeezes through." |
| SurveyPost | "The Forge takes samples, Shield stands guard. The cases are stamped "Berlin". We're not supposed to know more." |
| AmmoRoute | "Third run this week. Without us the battery is helpless, and nobody ever says thanks." |
| ConvoyRoad | "Convoy, over. Ridge is clear, forest is dead, not a soul up there. Same as always." |
| PeatBog | "The peat burns underground, where you can't see it. Step wrong and you fall through. I hate this post." |

Checkpoint and BaseA each have a second trigger (`ORE_Intercept_<Spot>_2`) at the same centre with half the radius.
Its line plays when the player pushes into the site, and the portrait manager queues it if the first line is still
playing. The picks are in `ore_clues.INTERCEPT_PICKS`. To rebuild, run
`ore_clues.build_intercepts(ore_clues.INTERCEPT_PICKS)`. Play test `ore_intercept_trigger_test.py`: 34/34 pass. The
UI shots show the Rifleman portrait with the English subtitle; the test presses the map's "Press To Start" first.

## Buried 1945 armour (sand mounds built 2026-11-18)

Exploration rewards on the large map: Wehrmacht vehicles abandoned in spring 1945, half-buried in sand. A squad uses
the existing scavenging mechanic (the scavengers are consumed, as if taking command) and the vehicle joins the player.
Only the **sand mounds** are built so far; the vehicle actors and the gameplay hookup come later.

- **Vehicles (all early-mission):** StuG III (`Ger_Vehicle_StugIII`), Wespe (`Ger_Vehicle_Wespe`), Flak-Luchs
  (`Ger_Vehicle_PanzerII_Luchs_FlakLuchs`), Pz IV infantry carrier (`Ger_Vehicle_PanzerIV_F1_Assault`: the only Pz IV
  with a cargo component, turret replaced by a troop compartment), Sd.Kfz. 251 mortar (`Ger_ArmoredCar_251_MortarTW`,
  GrW 34 in the bay).
- **Actor recipe (later):** the vehicle's own hull/turret static meshes + weapon skeletal meshes + one sand mesh.
  Tracks and wheels are left out (buried). Sand on the actor root; the parts sit under a `Pose` scene component
  whose relative transform is `pose_component` in `Source/Tools/BuriedArmour/BuriedArmour_poses.json`, each part
  keeping its Blueprint relative transform (`BuriedArmour_vehicle_dump.json`) under it.
- **Style: C3 "Eckversunken"** (user pick 2026-11-18, after option C "overrun" and three pitched refinements): the
  vehicle sank at the rear right corner, 4 deg nose up and 5 deg onto the right side, sunk 8 cm. A dune came from the
  rear right and buries that corner to the casemate/turret top edge (about +1 m over the fender line); the front
  left is the shallowest, with the fender edge just covered. Same rule, scaled to each footprint, on all five.
- **Rules every mound keeps:** sand at the flanks and ends always over the missing tracks; never a thin film on the
  hull (sand stays 6 cm under the hull surface or at least 10 cm over it; on the final meshes 2-4 % of hull
  vertices sit within 4 cm under the sand, all along the contact lines); sand can't climb a wall (no sand poured
  into open bays); sand always 12 cm under the weapon parts, with a sloped notch (they are separate skeletal parts).
- **Assets:** `/Game/RTS_Survival/Blueprints/Environment/OreMountains/BuriedArmour/Meshes/SM_BuriedSand_<Vehicle>`
  (about 30k triangles each, Nanite off, no collision, 4 m vertical skirt so the mound sits on sloped ground) and
  `Materials/MI_BuriedArmour_Sand` (child of `M_FieldTrench_Bank`: landscape colour from the RVT, paler drift sand
  up the slopes from vertex colour R, foot blends into the ground from vertex colour G).
- **Tools:** Blender `CustomMapItems/Environment/BuriedArmour` (brief, concept sheets v01-v02, `Model/build_mounds.py`);
  Unreal `Source/Tools/BuriedArmour` (`dump_vehicles.py`, `ue_buried_armour_import.py`).
- Crew meshes and the Wespe's loose ready rounds are left out (they would sit in the buried rear right corner).
- **Sand colour (decided 2026-11-18):** close to the ground colour, a paler tint only up the mound (vertex R from
  0.15 m, full at 1.25 m; `FreshTint` 1.32/1.18/0.98). A clearly paler sand was tried and dropped: the user prefers
  the version that "better blends with the landscape".
- **Placement (decided 2026-11-22): all 5, one per region.** StuG III near the ridge (early, easy), Pz IV carrier in
  the north foothills, 251 mortar by the viaduct, Flak-Luchs near the radio outpost, Wespe near the battery (late,
  guarded).
- **(open)** the exact spots, collision/nav of the mound, and whether the mound stays after the vehicle drives out
  (under the hull the sand is a flat plateau, not a hollow).

## Mission structure (flow picked 2026-11-22)

The owner picked 26 decisions on 2026-11-22 (six options each). They are written up here on 2026-11-23. Anything
marked *proposal* was not picked by the owner and still needs a yes.

### Win and lose

- **Major objectives:** **destroy Base B** and **recover the Berlin cases**.
- **Bonus objectives:** destroy Base A (loot only), destroy the artillery battery, and take the radio outpost.
- **An escaped convoy never loses the mission.** It makes the rest harder: its cargo strengthens the Soviets (see
  "Escaped cargo" below).
- **Lose:** the HQ is destroyed (*proposal*: the usual `UDontLoseHQMission`).

### Flow

1. **Opening.** Location intro (Erzgebirge + date), then a camera tour of the valley (weighbridge, road, knoll,
   battery), then briefing lines 01 to 03 (recorded: `OreMountains_Briefing_01_Weighbridge`, `_02_DiggingIn`,
   `_03_SpecialConvoy` in `.../AAAGerMissions/OreMountains/Briefing`). Then the faction's start force drives out of
   the adits. The full building kit is available.
2. **Convoy 1, the guided fuel run (scripted start).** An ambush hint marker below the ridge. When the escort dies, the
   "Capturing trucks" field-manual takeover explains the 12 s countdown, the 20 m squad rule and the 60 % health rule,
   and the Advisor coaches. Captured trucks drive to the adit and nothing chases them.
3. **Convoy 2, the weighbridge (scripted start).** The convoy stops about 60 s at the checkpoint, and the guards and
   towers join the fight. Strike early and it floors it forward; miss the window and it drives on.
   - **Light counterattack after convoy 2 is stopped:** infantry in trucks plus light armour hit the ridge. The first
     real pressure on the base. It also comes when convoy 2 escapes (owner pick 2026-11-23), one unit stronger.
4. **Convoy 3, the ammo split (scripted start).** The ammo trucks turn off toward the battery and the rest drive west.
   Each ammo truck that reaches the battery supplies it for about 3 min.
5. **Convoys 4 to 6 (timer).** Each starts about 4 min after the previous one has finished, with a tower or Advisor
   warning first.
   - **Convoy 4, hunters:** infantry dismount at the ambush zone and climb toward the ridge.
   - **Convoy 5, the cases:** an ordinary-looking convoy (the radio outpost manifest or a held tower marks it as "sealed
     cargo") with an **armoured case truck** that is very hard to destroy; the intended way is to kill the escort and
     capture it.
     - **Captured:** it drives to the adit; the cases are recovered on arrival.
     - **Destroyed anyway:** the cases survive in a **sealed cache** at the wreck, which a squad must scavenge (slower,
       under fire).
     - **Escaped:** the cases are **stored at Base B** and recovered when Base B falls (*proposal*: as a sealed cache
       there to scavenge).
     - **Heavy counterattack after convoy 5:** medium and heavy armour with hazmat engineers in support (losing the cases
       makes the Soviets lash out).
   - **Convoy 6, heavy:** a heavy column that reinforces Base B.
6. **Finale: storm Base B.** It is stronger for every convoy that escaped.
7. **Ending:** once Base B is destroyed and the cases have reached the adit, an Advisor line ties the cases to the Forge
   survey at the Berlin crater (the Berlin hook), then the end debrief.

### Running through the mission

- **Escaped cargo, by type:** fuel = more vehicles at the bases; ammo = the battery stays supplied longer or hits
  harder; concrete = new bunkers or AT pits at a base. The effects land at Base B. Tower intel shows the cargo type in
  advance, which is what makes capture-or-destroy and the towers matter. Note: `EConvoyCargoType` has `Fuel`,
  `VehicleParts` and `Ammo`, no concrete.
- **Alert tiers:** each stopped convoy raises the alert. Tier 1 sends patrols into the foothills, tier 2 sends retake
  squads to captured stations and towers, and tier 3 raids the ridge. **Conflict to settle:** the towers were picked to
  stay the player's once taken, and stations are retaken by destroying them. So tier 2 either attacks the stations only,
  or the towers need AI capture (C++).
- **Base B:** the road overwatch and the radio command post (see "Soviet bases").
- **Battery:** the timed supplied state (see "Soviet artillery battery").
- **Radio outpost** (north foothills): the manifest takeover marks convoy 5 and adds "Recover the cases".
- **Lookout towers:** 2 capture-zone towers. **Spoil heaps:** 1 easy heap and 2 capture stations. **Buried armour:** all 5,
  one per region. **Viaduct:** a chokepoint only.

### Missions[] stage plan (*proposal*, 2026-11-23)

This follows the owner's mission Blueprint rules:
- One `UMissionBase` Blueprint (or a Blueprint child of a C++ mission class) per stage.
- A flat `Missions[]` array, with each stage started by its own trigger, usually "Mission Class Completed" on an
  earlier stage.
- The flow lives in the event graph, and waits use `ScheduleSingleCallback`.

See [BerlinMissionScripting.md](BerlinMissionScripting.md) for the framework. Folder:
`/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OreMountains/`, names `Mission_Ore_<Stage>`. Every started stage
shows a plate. A convoy stage **completes whether the convoy is stopped or escapes** (the plate says which), so the next
stage always starts.

| # | Stage | Parent class | Plate | Trigger | Completes when | On start / on complete |
|---|---|---|---|---|---|---|
| 0 | `Opening` | `UMissionBase` | Major: "Survey the valley" | none (begin play) | the graph completes it after briefing line 03 | **Start:** location intro; camera tour; briefing 01-03 chained with `ScheduleSingleCallback`. **Complete:** the faction start force drives out of the adits. |
| 1 | `KeepHQ` | `UDontLoseHQMission` | Major: "Keep the HQ alive" | class completed: Opening | never; **fails = defeat** | |
| 2 | `Convoy1_FuelRun` | `UMissionBase` | Major: "Ambush the fuel convoy" | class completed: Opening | the convoy has finished (`OnConvoyFinished` of the route) | **Start:** hint marker; Advisor line; start convoy 1 on the route. **First scuttle countdown:** "Capturing trucks" field manual. |
| 3 | `Convoy2_Weighbridge` | `UMissionBase` | Major: "Strike at the weighbridge" | class completed: Convoy1_FuelRun | the convoy has finished | **Start:** start convoy 2; it stops 60 s at the weighbridge. |
| 4 | `LightCounterattack` | `USovietCounterattackMission` | Major: "Hold the ridge" | class completed: Convoy2_Weighbridge | the attack is beaten | Infantry in trucks + light armour (own settings asset). |
| 5 | `Convoy3_AmmoSplit` | `UMissionBase` | Major: "Stop the ammo convoy" | class completed: Convoy2_Weighbridge | the convoy has finished (both parts) | **Start:** start convoy 3; its ammo trucks branch to the battery. |
| 6 | `Convoy4_Hunters` | `UMissionBase` | Major: "Raid the convoys" | class completed: Convoy3_AmmoSplit | the convoy has finished | **Start:** `ScheduleSingleCallback(240)`, warning, start convoy 4. |
| 7 | `Convoy5_Cases` | `UMissionBase` | Major: "Raid the convoys" | class completed: Convoy4_Hunters | the convoy has finished | **Start:** 240 s, warning ("sealed cargo" if the outpost or a tower has marked it), start convoy 5. **Complete:** note where the cases are (adit, sealed cache, Base B). |
| 8 | `HeavyCounterattack` | `USovietCounterattackMission` | Major: "Hold the ridge" | class completed: Convoy5_Cases | the attack is beaten | Medium + heavy armour, hazmat engineers. |
| 9 | `Convoy6_Heavy` | `UMissionBase` | Major: "Stop the heavy column" | class completed: Convoy5_Cases | the convoy has finished | **Start:** 240 s, warning, start convoy 6 toward Base B. |
| 10 | `RadioOutpost` | `UKillActorsMission` (outpost guards) | Bonus: "Take the radio outpost" | class completed: Opening | the guards are dead | **Complete:** manifest takeover; convoy 5 is marked. |
| 11 | `RecoverCases` | `UMissionBase` | Major: "Recover the Berlin cases" | class completed: RadioOutpost (owner pick 2026-11-23: also started with convoy 5 if the outpost is not taken by then) | the cases reach the adit, or their sealed cache is scavenged | A stage has one trigger, so the second path needs a second trigger entry or a start call from `Convoy5_Cases`'s graph. |
| 12 | `DestroyBaseB` | `UKillActorsMission` (Base B targets) | Major: "Destroy Base B" | class completed: Opening | every target is dead | If the cases escaped, Base B falling drops their sealed cache. |
| 13 | `BaseA` | `UKillActorsMission` (Base A targets) | Bonus: "Raid the convoy yard" | class completed: Opening | every target is dead | **Complete:** loot. The route's owning building stays unset. |
| 14 | `Battery` | `UKillActorsMission` (the pits) | Bonus: "Silence the battery" | class completed: Convoy3_AmmoSplit | every pit is dead | |
| 15 | `Ending` | `UMissionBase` | none | class completed: DestroyBaseB | the graph ends it | **Start:** if `GetHasMissionClassCompleted(RecoverCases)` is true, play the Advisor Berlin line now; otherwise wait for `BP_OnOtherMissionEnded(RecoverCases)` first. Then victory and debrief. |

### Mission Blueprints built (2026-11-23)

All 16 stages exist and are wired into `Ger_2ndOreMountains`' `BP_MissionManager` (`Missions[0..15]`, map saved; backup
`Saved/OreMountains/stages/backup/Ger_2ndOreMountains_2026-11-23_pre_stages.umap`). Every stage compiles. **No stage
has run in a game** (the owner tests). Where the build differs from the table above:

- **Shared pieces:** `Mission_Ore_Base` (parent of the `UMissionBase` stages) holds `PlayAdvisorLine`,
  `StartConvoyRoute(RouteId)` and `TrackTaggedTargets(Tag, Prefix)`; `BPML_OreMission` holds the macro
  `SwitchConvoyEnd` (Outcome -> Stopped / Escaped).
- **Kill stages track tags, not placed lists** (the sites are not in the map yet): `RadioOutpost` tracks actors tagged
  `ORE_RadioOutpost`, `DestroyBaseB` `ORE_BaseB_Target`, `BaseA` `ORE_BaseA_Target`, `Battery` `ORE_Battery_Pit`.
  Each tracks on the minimap and, when the last one dies: line, wait, complete. With no tagged actor the plate stays up.
- **Bonus triggers:** `BaseA` and `Battery` start when a player unit comes within 90 m of the site (`near`), so their
  plates appear when found; `RadioOutpost` starts after `Opening` until its spot is picked.
- **Convoy stages** start the route whose `RouteId` is `ORE_Convoy1` .. `ORE_Convoy6` (one `AEnemyConvoyRoute` per
  convoy, each with its own definition and `Timing.bStartOnBeginPlay` off). **None is placed yet.** Convoys 4-6 wait
  `GapSeconds` (240) before their convoy; convoy 5 plays `SealedCargoLine` when `RadioOutpost` completed, otherwise
  `UnmarkedLine`.
- **Counterattacks:** `DA_Ore_LightCounterattack` / `DA_Ore_HeavyCounterattack` (copies of `DA_Berlin_ResponseForce`:
  no train, no air drops, no dig-in intro, no cinematic). Road group from the convoy yard (2010, 22500) down the valley
  road to the ridge; light = BT-7/T-26/T-70/BA + Mosin/Okhotnik/Maxim, 2 waves; heavy = T-34/KV-1/SU-85/IS-2 + hazmat
  engineers, hazmat bazookas, PTRS, 4 waves. Wave sizes are starter values.
- **Opening:** "Erzgebirge / 1976 / Ore Mountains, the Czech border" (date is a placeholder), a four-shot tour
  (weighbridge, knoll, battery, our adit), briefing 01-03 with 11 / 22 / 12 s waits, then the start force at
  (-21000, 6400) west of the adit drop-off: 2x `SpawnPlayerLightMediumTank_PZIII_J_Or_PZIV_F1`, 1x
  `SpawnPlayerGerMediumTankDestroyer_StugIII_OrMarder`, 1x `SpawnPlayerGerArmoredCar250_251Mortar` (these pick by
  faction). **The start base (HQ, Gamma Facility, refinery, barracks) is not placed and nothing spawns it.**
- **Slots that wait for content:** `Convoy1.ShowCaptureFieldManual` (the takeover widget is not built; it prints a
  note), `RecoverCases.WatchCases` (waits for the case truck C++), the manifest takeover, every Advisor line except the
  briefing (empty `SoundBase` variables play nothing).
- **Second start for RecoverCases (open Q4):** `UMissionClassCompletedMissionTrigger::AlsoWatchMissionClasses` (new,
  2026-11-23) lets one trigger watch several classes; RecoverCases will watch `RadioOutpost` OR `Convoy4_Hunters`
  (= the moment the convoy 5 stage begins) once the editor DLL with it is loaded.
- **Tools:** `Source/Tools/OreMountainsMission/` (`build_ore_stages.py` rebuilds every stage, `tidy_ore_functions.py`,
  `make_counterattack_settings.py`, `run_wire_ore_stages.py` + `wire_ore_stages.py`). Event graphs are hand laid out
  (rows left to right, each scheduled callback right after its Schedule node); functions are tidied (no reroute knots).

### What has to be built (2026-11-23 check)

**In the map** (none placed yet):
- the player's start base on the ridge;
- the convoy route, including the ammo branch to the battery;
- Base A, Base B, the battery pits, the radio outpost, the weighbridge guards and the weighbridge collision;
- the ambush hint marker;
- the 2 lookout towers (the model is not in Unreal yet);
- the 2 spoil-heap stations;
- the 5 buried-armour vehicle actors (only the sand meshes exist).

The map already holds an actor tagged `CapturedTruckDropOff`; check that it sits at the main adit on the navmesh.

**Assets:**
- the Blueprint child of `ASurrenderingCargoTruck`;
- the armoured case truck;
- the six Ore convoy definitions;
- the two counterattack settings assets;
- the start force per faction;
- the "Capturing trucks" field manual and the manifest takeover.

**C++ not found in the code** (searched by name; unverified):
- a timed convoy stop at a point (convoys only halt behind their escorts today);
- infantry dismounting from convoy trucks;
- Base B's radio speeding convoys up;
- escaped-cargo effects by type (and a concrete cargo type);
- alert tiers;
- the battery's supplied state (range up plus the barrage);
- the case truck's sealed cache and "stored at Base B";
- a payout mapping for captured fuel and ammo trucks;
- AI capture of towers (only if tier 2 retakes towers).

**Voice lines:**
- Recorded: briefing 01-03.
- Still needed, Advisor: convoy 1 coaching, the convoy warnings, the two counterattack warnings, the manifest, the
  cases recovered, and the Berlin ending line.
- Still needed, other: tower and station capture lines.

## Dependencies

The full build list is in "What has to be built" under "Mission structure"; this list is the older summary.

- Ore Mountains terrain: ridge, foothills and open valley, with several elevation tiers. Built (570 m map).
- A large Radixite field and metal deposits placed in the map. Placed; amounts still to tune.
- Wismut-style mine and processing site assets for the Soviet bases and the player's tunnel base. Adit portals and
  rockworks are built; the Soviet sites are still placeholders.
- Soviet truck convoy units and the convoy movement/spawn logic along a road: `AEnemyConvoyRoute` + `AConvoy` exist
  (escort tiers per difficulty, mission or timer start, outcome counts and events).
- The artillery battery: the existing Rus weapon pits, ammo trucks, and a "supplied" state that raises range and starts
  the barrage when a truck arrives. The supplied state is not in code.
- Starting base buildings: HQ (expanded), Gamma Facility, refinery and barracks, placed on the ridge. The Gamma Facility
  is an existing building (`Nomadic_GerGammaFacility`).
- Mission scripting for the starting force (later).

## Open questions

All ten were answered by the owner on 2026-11-23 (six-option dialog); these answers win over older text above.

1. **Alert tier 2 = stations only.** Tier 2 squads go after the 2 spoil-heap stations and try to destroy them. Towers
   stay the player's; no AI capture code.
2. **New `EConvoyCargoType::Concrete`.** Escaped concrete trucks switch on pre-placed hidden defences at Base B; a
   captured concrete truck pays metal.
3. **Captured payout mapping, 2.5x:** fuel to Radixite, ammo to metal, vehicle parts to vehicle parts, concrete to
   metal. A surrender shows a white flag on the truck, one short Advisor line (once), and the minimap icon turns to
   the player's colour.
4. **"Recover the cases" also starts with convoy 5:** if the radio outpost has not been taken when convoy 5 starts, the
   objective appears then (with an Advisor line), only without the early "sealed cargo" mark.
5. **The light counterattack comes after convoy 2 either way;** if convoy 2 escaped it is one unit stronger.
6. **Numbers: starter values that the owner tunes** after play-testing, all in data assets or Blueprint defaults (for
   example 240 s convoy gaps, alert tiers at 1 / 3 / 5 stopped convoys, battery reach to the ridge when supplied).
7. **Spots: proposed, then picked.** A top-down render with 2-3 numbered candidates per item (following the region
   picks); the owner picks in one dialog round.
8. **No cryo rack** at the survey post: stakes, flags, generator and sample cases only. Berlin stays the first cryo look.
9. **Far heaps can be harvested any time** (long hauls home); the station only shortens the trip. No lock.
10. **Czech border = the south edge,** shown by a row of old boundary stones and a broken border post (dressing); no
    side valleys.

Nothing is open beyond the tuning numbers and the spot picks.
