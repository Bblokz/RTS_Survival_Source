# Ore Mountains mission (design draft, working title)

Status: concept only, nothing built. Started 2026-11-13. This is the **second mission** of the campaign.
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
- **Sketch facts (v5):** map 850 x 850 m (units cm, +X north, +Y east), Czech border and ridge on the **south** edge,
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
  of Ger_MarderStugMission).
- **The mission sits along the Czech border.** One edge of the map is the border, with the ridge along it; the valley
  and the convoy road run beside it. **(open)** which edge (the ridge and border side is the south or south-east in the
  real geography, with the corridor running along it toward the north-west) and what the border edge looks like in-game.
- **The three tiers:** the player's **ridge on the south** (Czech border side), the **road in the valley floor**, and the
  **foothills to the north**.
- **(open)** whether it is a single valley or has side valleys.

### Resources

The map has to **contain resources, designed into the layout**: **a quite large Radixite field and metal as well.**
Tuning (amounts, positions, how far they sit from the base) comes later. **(open)**

## The player's base

- The player **starts with a base on the ridge**, built into the old mine tunnels.
- **The base can be expanded.** It starts with an **expanded HQ, a Gamma Facility, a refinery and a barracks.**
- **Starting force:** will be implemented later in mission scripting, not in the design doc. **(open)**
- **(open)** how much of the rest of the base-building kit is available this early, and how the existing base-building
  tutorial (the intro mission) affects this.

## The enemy

### Convoys (the main point of the mission)

- **Count:** about **six convoys** over the mission, each with **4 to 6 trucks**, coming from the **east end of the valley**
  and driving **west** along the valley road below the ridge. Road only, no rail line.
- **Escort:** the first convoys are **lightly escorted** (a car or two). Later ones add **armoured cars** and then a few
  **tanks**. The **last one is a heavy convoy with a real escort**. Escort size **scales with difficulty**.
- **Success:** destroy **enough of them, for example 4 of 6**. Trucks that **escape fill a "Soviet supply delivered"
  meter, and if it fills the mission fails.** That gives a clear loss condition without a hard timer. **(open)** exact
  threshold per difficulty.
- **Bases as a lever:** destroying a base **thins the escorts or removes a convoy source**. The bases are **optional**, but
  they make the mission easier.

### What the convoys carry

- Most are **ordinary supply: fuel, ammo and parts**. This is what the western outposts need.
- **One special convoy carries sealed cases that connect to the Forge survey: sample canisters and documents marked for
  Berlin.** Wiping it out and **recovering the cargo is the Berlin hook**, and it is a good **climax** for the mission.
  **(open)** which convoy is the special one (it could be the heavy last convoy), and how the cargo is recovered.
- The convoy is **not the first time the player sees the Berlin crates.** An early clue comes from the radio outpost (see
  "Radio outpost with the Berlin crates" below), so the special convoy pays off something the player already knows about.

### Capturing convoy trucks (decided 2026-11-16)

This is the core loop of the mission. It answers "why do the convoys matter to *us*": the resistance needs what is on
those trucks, and the base grows from the raids.

- **Kill the escort and the trucks stop and surrender.** A truck that surrenders is no longer a Soviet unit.
- **A captured truck drives itself to the mine adit** (the "mine adit drop-off" by the Gamma Facility in the sketch) and
  **pays out its cargo when it arrives**: fuel, vehicle parts or Radixite, by the convoy's cargo type. The player cannot
  steer it. It pays **only on arrival**, so it can still be lost on the way (Soviet response forces, the battery barrage).
- **The choice on every convoy: a fast kill or slower, riskier loot.**
  - **Destroy:** each dead truck already drops a scavengeable cargo cache (existing convoy behaviour,
    `FConvoyCargoSettings::CacheClass`: about 5 minutes before it is gone, about 12 s for a squad to scavenge). Fast and
    safe, but the player gets a share and has to send squads into the kill zone to pick it up.
  - **Capture:** the full cargo, but the player has to kill the escort **without** killing the trucks, then cover the
    trucks while they drive up to the adit.
- A captured truck counts as **stopped** for the convoy objective and does **not** fill the "Soviet supply delivered"
  meter.
- **The power fantasy grows from the raids.** Keep the starting resources low so that captured cargo is what pays for
  the base expansion and the tanks.

**Implementation requirement: a new derived class for cargo trucks that can surrender.**

- We want a **new C++ class derived from the class the convoy trucks use today** (convoy trucks are `ATankMaster`
  children, spawned from `FConvoyTruckSpec::TruckClass`). Working name `ASurrenderingCargoTruck` **(open)**.
- **Only trucks of this class can surrender.** The Ore Mountains convoy definitions point their `TruckClass` at Blueprint
  children of this class.
- **No other cargo truck in the game gets this feature.** The intro mission's fuel convoy (`DA_Convoy_IntroFuel`), other
  convoys, survival-mode convoys and the supply point's Opel truck (`BP_Ger_OpelSupplyTruck`) keep their current
  behaviour. Do **not** add a switch on `AConvoy` or `UConvoyDefinition` that turns surrender on for every convoy; the
  class is the switch.
- **Hand-off after surrender:** the truck changes owner to the player, and `AConvoy` releases it (it is no longer a
  member, and it does not count as lost or escaped). It then reuses the supply-point truck logic (`USupplyTruckComponent`:
  drive to a target, pay the cargo on arrival), with the mine adit as the target instead of the HQ.
- **(open)** what exactly triggers the surrender (all escorts of that convoy dead, or no living escort within a set
  distance of the truck), whether a damaged truck can still surrender, whether the Soviets try to destroy or retake a
  captured truck, what shows the surrender (white flag, voice line, minimap icon change), the payout per truck, and the
  truck's route up the main ramp to the adit.

### Convoy line-up (first three decided 2026-11-16)

Each convoy is a different problem, not the same ambush with a bigger escort.

1. **Light fuel run: a guided first ambush.** A few fuel trucks and a light escort (a car or two). The mission guides the
   player through it: where to set the ambush, and that killing the escort makes the trucks surrender. This is where
   capture versus destroy is taught.
2. **The weighbridge convoy: a set-piece window.** This convoy **stops at the weighbridge checkpoint** for an inspection,
   which gives the player a fixed time and place to strike. The checkpoint guards and towers join the fight. **(open)**
   how long it stops, and what happens if the player strikes early or late. Note: the weighbridge, booth and towers are
   still placeholders on the map, and the checkpoint has no collision yet.
3. **The ammo convoy that splits: two targets, pick one.** Part of the column (the **ammo trucks**) turns off toward the
   **artillery battery** in the north-east, while the rest drives on west. The player chooses: stop the ammo trucks and
   deny the barrage, or stop the supply trucks and protect the meter. **(open)** whether the battery still gets other
   ammo trucks on their own route as well, and whether captured ammo trucks pay out something the player can use.

Convoys 4 to 6 are **(open)**. The last one is still the heavy convoy (see "Escort" above).

### Radio outpost with the Berlin crates (decided 2026-11-16)

A **small Soviet outpost with a radio antenna** that holds **some crates marked for Berlin**. It is the **early clue** for the
Berlin hook: the player finds the crates before the special convoy, so the climax pays off something already set up.

- **Guards: only a few infantry.** A few **Mosin squads** (`Squad_Rus_Mosin`) and **one bazooka hazmat squad**
  (`Squad_Rus_HazmatBazooka`), **and that is it**: no vehicles, no guns, no bunkers. It is meant as a small, early side
  target, not a third base.
- **(open)** where it sits on the map, what the player gets from the crates (a manifest, a voice line, a takeover), and
  whether the radio does anything in play (for example warning the convoys while it stands).
- Check before building: the survival-mode base docs say `Squad_Rus_HazmatBazooka` is **not** in that mode's base spawner
  map. Confirm that the placement actor used here (for example `AEnemyStrongpoint`) can spawn it.

### Soviet bases (secondary targets)

**Two small bases**, in old Wismut mine and processing sites, as depots or storage that feed the convoys.

- **Base A, at the east end of the valley:** this is where the convoys originate. **Light defences**: MG nests, a couple
  of infantry squads and two light tanks. It is close to the start, so it can be hit early.
- **Base B, on the foothills across the valley from the player's ridge:** it overlooks the road and is **better defended**,
  with AT guns and some infantry on the high ground. It forces a **flanking approach** and uses the elevation tiers.
- **(open)** whether each base thins escorts or removes a convoy source, and the exact defences.

### Soviet artillery battery (top right)

Added 2026-11-13. A Soviet **artillery battery in the top right (north-east) of the map**, built from the **various
weapon pits the game already has** (for example the Rus pits: 53-K, BS-3, 120 mm mortar, DShK and Ampulomet).

- **Ammo trucks drive to the battery.** Separate from the supply convoys on the valley road.
- **Supplied state:** whenever an ammo truck **arrives**, the battery gets **increased range** and **starts barraging the
  player**. Unsupplied, it has its normal (shorter) range and does not reach the player's base.
- **Counterplay:** the player can intercept the ammo trucks to deny the barrage, or destroy the battery itself.
- **(open)** the ammo truck route (where they start, whether they use the valley road for part of the way, how many, how
  often), how long the supplied state lasts after a truck arrives, the barrage's target selection (the base, the player's
  units, or both), and what each pit type contributes. Range numbers need tuning against the pits' real ranges.
- **(open)** whether the battery counts as one of the "two small bases" or is a third site, and how it is defended.
- **(open)** whether ammo trucks count toward the "Soviet supply delivered" meter.

### Landmarks and set dressing

Added 2026-11-13, so the map is not all nature. Each has a lore job.

- **Weighbridge / checkpoint on the valley road.** The convoys pass through it. It is a natural ambush spot with a clear
  sight line (from the ridge). **(open)** how it is staffed, and whether convoys stop at it.
- **Broken viaduct or overpass from the old A4 line.** Gives the road some history. **(open)** where it crosses the road,
  and whether its collapse blocks or only narrows the road.
- **Small Forge survey post** near the Radixite field: sampling stakes with flags, a cryo canister rack and a generator.
  It matches the Forge survey station in Berlin and **foreshadows it**, so the Berlin mission feels earned when the
  special convoy's cargo turns up. **(open)** whether it has a guard, and whether it is a target or only dressing.

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

Triggers placed and play-tested 2026-11-16 (`ore_intercept_trigger_test.py`, 19/19), still without lines. Three takes
per spot are in `VoiceLines/VoiceChanger/Output/OreInterceptAudition` for the owner to pick; then
`ore_clues.build_intercepts({spot: SoundWave})` sets them. **(open)** the picks.

## Mission structure

**(open)** not yet designed beyond the beats above. Likely: hold the ridge, ambush the first lightly escorted convoys,
discover and hit the bases, then escalate to the heavy last convoy and the special cargo.

## Dependencies

- Ore Mountains terrain: ridge, foothills and open valley, with several elevation tiers. **(open)**
- A large Radixite field and metal deposits placed in the map. **(open)**
- Wismut-style mine and processing site assets for the Soviet bases and the player's tunnel base. **(open)**
- Soviet truck convoy units and the convoy movement/spawn logic along a road. **(open)** check what exists.
- The artillery battery: the existing Rus weapon pits, ammo trucks, and a "supplied" state that raises range and starts
  the barrage when a truck arrives. **(open)** check whether the pits' ammo/range can be driven by mission scripting.
- Escort scaling per difficulty and a "supply delivered" meter with a fail threshold. **(open)** check what exists.
- Starting base buildings: HQ (expanded), Gamma Facility, refinery and barracks, placed on the ridge. The Gamma Facility
  is an existing building (`Nomadic_GerGammaFacility`).
- Mission scripting for the starting force (later).

## Open questions

1. Which edge is the Czech border, and whether there are side valleys.
2. Exact convoy success threshold per difficulty, and which convoy carries the special cargo.
3. Exactly how each base affects the convoys, and the exact defences.
4. Resource field sizes and positions (tuning later).
5. How much of the rest of the base-building kit is available this early.
6. The artillery battery: ammo truck route and frequency, how long the supplied state lasts, what the barrage targets,
   whether it is a third site, and how it is defended.
7. Truck surrender: the exact trigger, how it is shown, the payout per truck, and whether the Soviets go after captured
   trucks.
8. Convoys 4 to 6 (beyond "the last one is heavy").
9. The radio outpost: location, and what the Berlin crates give the player.
10. Whether the survey post keeps its cryo rack as unexplained dressing or drops it.
