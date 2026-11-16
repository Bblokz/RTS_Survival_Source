# Ger_FirstMission: lore and life pass (proposal)

Status: proposal only, 2026-11-16. Nothing on the map or in the mission Blueprints has changed.

Basis:
- an actor dump of `Ger_FirstMission` (1,239 actors, read headless);
- the mission Blueprints in `Blueprints/Missions/GerCampaign/ClearRoadMission1/` and the voice line transcripts;
- `Source/DesignTexts/Lore.txt` and the "What the player may and may not learn" table in `IntroCinematicScript.md`;
- the October screenshots in `Saved/Screenshots/CodexLighting/FirstMission_*`.

There was no fresh visual pass. Map: `FirstMission_lore_pass_map.png`. Game north (-Y) is up and east (+X) is right;
the voice lines use the same compass ("the depot to the north"). Coordinates are in cm, and the map codes match the
sections below.

## The mission as it stands

| Beat | Where (cm) | What happens now |
|---|---|---|
| Start | `RTS_PLayerStart` (-31190, 32545), south-west corner | Advisor: "today marks the resistance uprising"; Pz 38(t) drops; Scavengers. |
| Approach | crater plain (-31000..-20000, 15000..30000) | Old craters and a dry wash. Little is placed: only the mission squad points `SP_SquadMission_1/2` and the patrol points. |
| Fuel depot | (-20000..-13000, 9400..14000) | Stealth fails. The alarm system is the depot's `Rus_Bxp_Megaphone` (`BP_Mission1_AttackDepot` casts to it and starts its siren). Then come the Hazmat reinforcement call, the combat train with a KV-2 turret at (-26089, -10316), and a bomber strike on the train. |
| Bridge | (-9000..-1700, 12000..17000) | Hedgehogs, a dug-in heavy, a vehicle on the ridge. Old Pz III, T-34 and Pz 38(t) wrecks, a Tiger scavenge object, Schreck and StG pickups at (-9500, 18800). |
| Reactor + Radixite | (-12000..-4000, -5000..6000) | Two `BP_Dest_PostApo_Nuclear`, a large Radixite growth at (-10213, 5726), the crystal cinematic. Scavenger finds: `BP_ScavObj_RadixiteTank`, `_RadixiteDump`, `_Project24`. |
| Garrisons | high-rises (-4600, -4800), (-1400, -5700) | Flame panzers burn out the garrisoned Soviets. |
| Patrol base (optional) | (-6000..3800, -27000..-15000) | Walled base with cooling towers, barracks and 50 hedgehogs. Patrols roam the middle of the map. |
| Final road | (9000..14000, -33000..-24000) | Red Hammer, T-26s and flame T-70 waves. Tank destroyers arrive by dropship. |
| Ending | `LS_Mission1_FInalCinematic` at the depot | "The convoy is advancing ... today, we begin building something new." |

The map's lore so far comes almost entirely from voice lines and the Radixite finds. Between fights the ground tells
little. The longest stretch with nothing in it is the crater plain the player crosses first.

## What mission 1 may tell

| Theme | Safe now | Seed only (your call) | Never here |
|---|---|---|---|
| Survival | 30 years as nomads; scavenging; heat; the land is a desert | underground survivors exist (the Burrowers, unnamed) | — |
| Soviets | a thin garrison of outposts; the Directorate, a council, runs it (Director of Shield); they think the West is dead | the Directorate also controls food (Harvest) | Stalin, the mutiny |
| Old war | 1945-47 wrecks of both sides; Western forces fought here too | — | a straight line of strikes, glass |
| Water | rivers shrank, the water left is contaminated | — | — |
| Radixite | hot, seems alive, grows, eats metal, dangerous to infantry | — | anything purple near crystals; crystals on or in a reactor or warhead |
| Purple Death | a western bio-weapon (from the intro); quarantine warnings | a fenced-off quarantine zone, with no growth shown | wheat, crops or silos as targets; "Phytophage", "Demeter" |

## Questions about lines already in the mission

All of these are imported under `Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/FirstMission/`. Keep, soften or
cut each one; it is your call.

Wiring checked: `BP_Mission1_Nuclear` plays `s_RadixiteFind_Biological`, `_Project24` and `_Tank` and
`MarkingRemainingObjects`; `LS_FirstMission_Crystals` plays `InvestigateRadixite`.

1. **`RadixFind/..._Biological`**: "Radixite does not only affect metal, biological material is being consumed as
   well." It moves towards the Act 3 reveal (Radixite is the bio-weapon). It is fine as "it eats things". A softer
   version: "It is eating more than steel."
2. **`BridgeSecuredNuclearAhead`**: "nuclear facility ahead ... interference ... linked to the radixite crystals". This
   puts "nuclear" and Radixite together. Act 4 (fission ignites Radixite) is the reveal to protect. Options:
   - call it an "old power plant";
   - or keep the line, but keep the crystals visibly away from the reactor buildings. The placed large growth is
     about 57 m from Nuclear2. The mission also spawns `Resource_Radixite_Crystal_LargeWithGrowthFast` and the three
     finds at runtime, and their positions are not in the dump.
3. **Project 24 wreck**: the `Lore.txt` Project 24 section is marked "not yet revised for the CSD setting". If Project
   24 is still canon, this is a fine early seed: it is unnamed and "it failed". If it is not, the find needs another
   subject.
4. **Scale words.** "Today we unite against Eurasia's occupation" and "slow down the Soviet war machine" do not fit
   "scattered outposts; they think we are dead". Suggestions:
   - "the occupation of the West";
   - "the Soviet garrisons".
5. **Which mission is day one?** `FirstMission_ClearRoad_1` says "today marks the resistance uprising". The
   `Ger_IntroMission` briefing ("Thirty years ze Soviets have hidden...") also reads as the start. If the intro mission
   comes first, ClearRoad_1 needs a different opening.

## Proposals by area

Tags:
- **Reveal:** what the player learns.
- **Assets:** *exists, unplaced*, *exists in the Intro mission* or *needs making*.
- **Cost:** this is one of the three perf-audit maps, so the proposals favour single-draw systems (ISM rows, bird
  flocks, decals) and audio.

### S: Start and the crater plain

- **S1. The nomad camp the player leaves.**
  - What: 4-6 old cars loaded with bundles and tarps, jerrycans and a cold fire, under the existing `NS_Smoking4`
    (-29912, 29439). It sits next to the start.
  - Reveal: the resistance are nomads who have lived on the move for decades (safe).
  - Assets: `BP_OldCarRow` / `BP_OldCar` exist, unplaced; jerrycans `Hangar/Models/SM_JerryCan_01`; bundles and tarps
    need making or kitbashing.
  - Cost: one ISM row.
  - It pays off in F1, where the same cars roll in at the end.
- **S2. A wreck field in the crater plain.**
  - What: a loose battle line of half-buried 1945 hulks, German and Soviet facing each other, with sand drifted against
    their sides. Rusted hedgehogs with sand drifted over them. Two ravens circling.
  - Reveal: the "unrecorded war" was huge, and 30 years have passed (safe).
  - Assets: `BP_Env_Destroyed*` (Pz III, T-34, IS-3, KV-1E, Pz 38(t)) exist; `BP_BirdFlock_InkRaven` / `_Crow`
    exist, unplaced. Sand-drift decals need checking: on the Intro mission's landscape, `MI_PathDecal_DustSpill` drew
    as opaque squares.
  - Cost: low, with one flock = one draw.
  - Optional: a short advisor line as the player crosses, for example: "Panzers from ze last war. Thirty years, und ze
    sand still has not finished burying them."

### D: Fuel depot

- **D1. A working depot before the shooting.** The truck is on the open road, so the player can shoot it before the
  surprise cinematic. Either accept that as an early strike or make it untargetable until the depot fight starts.
  - What: a Zil fuel truck arrives on the road while the player approaches. A crane swings barrels off it; Soviet
    squads march their rounds.
  - Reveal: the depot is alive and supplies the sector, which makes "strike where it hurts" visible (safe).
  - Assets: exist in the Intro mission (`AEnemyConvoyRoute` / `AConvoy` with `DA_Convoy_IntroFuel`,
    `BP_MissionFuelCrane`, `MarchToLocation`).
  - Cost: moderate scripting; all the systems are proven.
- **D2. The Directorate's face.**
  - What: propaganda banners on the gate and tents. The megaphone at (-14663, 12268) is also the alarm system. Before
    the player is spotted it loops Russian broadcasts with English subtitles. When the alarm goes, the broadcast cuts
    out and the existing siren takes over, so destroying it still stops the reinforcements. Example lines:
    - "Western Sector garrison: the West is silent. Discipline is survival."
    - "By order of the Directorate, fuel ration for Sector 7 is reduced."
  - Reveal: a council runs the Soviets now, and they believe the West is empty (safe).
  - Assets:
    - exist: one generic `MI_PropagandaBanner_NotMoving` (a Directorate or Shield emblem would need making), the siren
      cues, and the `S_SovietPropaganda` loop used by `Rus_Bxp_Megaphone_Propaganda1`;
    - `Propaganda1` is not a child of `Rus_Bxp_Megaphone`, so the mission's cast would fail on it. Add the broadcast
      to the placed megaphone instead;
    - need making: the new broadcast lines (Vlasov "Broken transmitter" chain).
  - Cost: audio only.
- **D3. Intel case.**
  - What: after the depot falls, a scavengeable Soviet dispatch case opens an archive entry: a requisition from the
    "Directorate of Shield, Western Sector".
  - Reveal: safe. A seed if it also mentions ration cuts "on the authority of the Directorate of Harvest".
  - Assets: the archive custom content (`AddNewCustomArchiveItem`) exists; the case mesh needs choosing.

### B: Bridge and river

- **B1. The river that shrank.**
  - What: dark high-water stains on the bridge piers, metres above the water. A cracked mud margin. One or two boats
    stranded high on it.
  - Reveal: heat waves dried the water up (safe, from the intro).
  - Assets:
    - need making: the stain decals;
    - the only boats found are the `Greek_island` ones, which probably clash with this style.
  - Cost: decals.
- **B2. Bad water.** Brown-green foam on the shore and a Soviet "unfit to drink" sign.
  - Reveal: the water left is contaminated (safe).
  - Assets:
    - exist: `M_RTS_Water_Shore`;
    - need making: the sign text (the `DC3Wreckage` warning sign mesh can carry it).
- **B3. An old cache.**
  - What: put the Schreck and StG pickups in a dug-in 1945 German position, with sandbags, a crate and a chalk mark,
    so they read as hidden resistance stock rather than loot lying on the ground.
  - Reveal: safe.
  - Assets: exist; the chalk mark comes from T2.

### T: Ruined town and garrisons

- **T1. Layered history.**
  - What: faded German shop and street signs, with a Soviet stencil painted over them ("СЕКТОР ЗАПАД").
  - Reveal: Germany, then the occupation, then 30 years of nothing (safe).
  - Assets:
    - exist: the `Underground/.../M_Decal_Graffiti` master;
    - need making: the sign and stencil textures.
- **T2. Nomad chalk signs. Recommended as a campaign-wide motif.**
  - What: a small decal atlas of about 8 symbols: water, Soviets here, safe cellar, radiation, cache, turn back. Place
    them at the camp (S1), the cache (B3), the town and the measuring post (R2).
  - Reveal: shows how scattered survivors talk to each other, and makes the advisor's "I have marked the path" part of
    the world.
  - Assets: needs making (one texture atlas).
  - Cost: decals.
- **T3. Something lives below (seed).**
  - What: a sealed cellar or bunker door with a vent pipe breathing a thin smoke, and a chalk "friend?" sign. No unit,
    no name.
  - Reveal: seeds the Burrowers.
  - Assets: `RBP_WW1_Trench/SM_Bunker_Door` and a smoke Niagara exist.

### R: Reactor and Radixite

- **R1. Heat.**
  - What: heat shimmer and rising thermals over the crystal field (-18000, -32000) and the large growth. The ground is
    killed around the crystals.
  - Reveal: Radixite heats the world (safe).
  - Assets: the Radixite blight subsystem exists. Check that this map's landscape material has the `LS_RadixiteBlight`
    switch on. There is no heat-shimmer asset yet: it needs making (a distortion card or Niagara).
- **R2. Abandoned Soviet measuring post.**
  - What: a container hut with a mast, set well clear of the reactor, with Geiger clicks on approach. Scavenging it
    gives a logbook entry: "Specimen 4 has grown 30 cm since the last reading. Post relocated."
  - Reveal: Radixite seems alive (safe), and even the Soviets are wary of it.
  - Assets:
    - exist: `Rus_Bxp_ContainerHut`, `Sounds/Cinematics/Logo/LogoGeiger`;
    - need making: the archive text.
- **R3. Keep the reactor cold.** No crystals growing out of, or into, the reactor buildings, and no purple anywhere on
  the map near Radixite. This protects Acts 3 and 4.

### P: Airfield and patrol base

- **P1. Sand-buried Luftwaffe apron** at the airfield hangars (-27300, -14000..-32000).
  - What: one crashed Stuka half under sand.
  - Reveal: Germany fought to the end; it is the counterpart to the panzer field in S2.
  - Assets: the Stuka crash geometry collection exists (`GC_AircraftCrash_SK_Stuka_VTO`). It would need a static, wrecked
    pose.
- **P2. Patrol base life.** Banners, a roll-call march, and the depot's siren (`Cue_SirenLoop`) reused here when the
  player attacks.
- **P3. Quarantine fence (seed).**
  - What: one fenced-off house in the southern ruins, around (-2950, -10780), with a Soviet quarantine sign and no
    growth shown.
  - Reveal: the Purple Death is still out there.
  - Assets: `Underground/Meshes/Props/SM_Quarantine_Sign` exists (it needs Russian text); fences exist.
  - Keep it far from every crystal. The dead Radixite trees are about 120 m away.

### F: Final road and ending

- **F1. Show the people.**
  - What: in `LS_Mission1_FInalCinematic`, the convoy is the S1 cars and Opel trucks, loaded with belongings, with
    scavengers walking alongside.
  - Reveal: gives "our people can finally move in" a picture (safe).
  - Assets: exist (old cars, Opel BPs).
  - Cost: cinematic only.
- **F2. Something passing overhead (low priority).**
  - What: once, a high Soviet transport crosses east, far up, and does not stop.
  - Reveal: they still pass over the West, but they do not stay.
  - Assets: the aircraft exist; it needs an ambient flyover actor.

### A: Audio across the map

- **A1. Soviet radio intercepts. Recommended.**
  - What: two or three short Russian chatter lines with subtitles, in quiet moments, on the `SovietMosin` /
    `RusHazmat` portraits. Examples:
    - "Nobody out there since Svarog. Ghosts and scrap-rats."
    - A hazmat trooper reading his dosimeter aloud.
  - Reveal: sets up the uprising as a surprise ("they think we are dead") and introduces the paranoid hazmat "Canary".
  - Assets: the voice chain exists (`VoiceLines/AGENTS.md`); the lines need making.
- **A2. Advisor reactions** at the wreck field, the shrunken river and the measuring post. One sentence each.

## Shortlist (most lore for the least cost)

1. A1 radio intercepts and D2 megaphone broadcasts: audio only, and the biggest lore yield.
2. T2 chalk-sign atlas: one texture, used again across the campaign.
3. D1 working depot: reuses the Intro mission's convoy and crane.
4. S1 + F1 bookends: the nomad camp at the start and the same cars arriving at the end.
5. S2 wreck field and ravens: fills the empty first stretch.
6. R2 measuring post: safe "Radixite is alive" storytelling.
7. B1 high-water stains on the bridge piers.

The open questions (1-5 above) should be decided before any new line is written, because A1 and D2 depend on the
"scattered outposts" tone.
