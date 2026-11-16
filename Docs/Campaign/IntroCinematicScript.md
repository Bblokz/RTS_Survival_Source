# Intro cinematic: section breakdown and script

Status: built as an in-engine level and rendered (second pass 2026-11-15: Make assets, four adversarial rounds,
cinematic pass; see the Build ledger at the end for what is real, what is a placeholder and what deviates). Script started 2026-11-14. This replaces the old intro cinematic
completely. Only its assets may be reused.

Lore reference: `Source/DesignTexts/Lore.txt`:
- 2.0 timeline
- 2.6 what the player knows
- 2.8 voice-over
- 2.9 archival wording

Every item marked **(open)** is undecided.

This document is the build brief for the agent that builds the cinematic level (Fable 5.2). Every shot is rendered in
the engine (Sequencer and Movie Render Queue) from game assets, or from assets made for this level. No AI-generated
video or images.

## Decided

- **Radixite origin.** The narrator must **not** reveal where Radixite comes from. The line that links Radixite to the
  western weapon is removed. He says only that atomic fire caused a chemical reaction.
- **The Directorate.** It gets a section of its own (section 7). This is an addition to Lore 2.8.
- **Intel stamps** stay in, using the mild set (see Visual language).
- **Timing** is left to the builder (see Timing and pacing). The total length is decided later.
- **The weapon's name is the Purple Death.** Lore line 34 ("green death") is the odd one out.
- **Narrator portrait: top middle of the screen, like the in-game portrait,** for the whole cinematic. The narrator
  is the resistance briefing officer, voiced like the existing German announcer.
  - The portrait shows whenever he speaks, with the subtitles under it, as in game.
  - Keep cards (lower left) and stamps clear of the top-middle area.
- **Ending: reuse the old logo animation** `LS_RTS_Logo` (`/Game/RTS_Survival/Cinematics/LS_RTS_Logo`) as the
  title card in shot 9.7.
- **Place names on screen** (approved 2026-11-14). They are not in the lore, but they match real history:
   - **Peenemünde** (shot 2.2). The German Army Research Centre (Heeresversuchsanstalt) on the Baltic coast, where the
     V2 was developed and test-fired. The lore says the spies got into "classified testing facilities", and this was
     the main one. In reality the Red Army captured Peenemünde in May 1945, so a Soviet theft there fits the history.
     The first V2s fired at Britain (September 1944) were launched from the Netherlands, not from Peenemünde. So the
     card goes on the theft shot, not on the launch shot.
   - **Los Alamos, New Mexico** (shot 4.2). The secret laboratory where the atomic bombs were designed and built. It
     was the heart of the Manhattan Project.
   - **Oak Ridge, Tennessee** (shot 4.2). The site of the huge plants that enriched uranium for the bombs.
   - **Hanford, Washington** (shot 4.2). The site of the reactors that made plutonium for the bombs.

   Together, the three tags tell the viewer that the Soviets destroyed the whole American bomb programme, not just one
   building.

- **Burn pins on other German cities** (shot 3.6): unnamed pins, no city labels. The lore says only "many followed".
- **The Full narration block is the source of truth for the spoken words.** The shot tables follow it. If they ever
  disagree, the narration block wins.

## Open decisions

1. **(open) Closing lines.** The last paragraph of the Full narration is unfinished ("...Zhey think we are dead. over
   time most of the"). Until it is finished, shots 9.5 and 9.6 keep the old closing line as a placeholder.

## Timing and pacing

**No shot has a fixed duration.** The builder decides how long each shot runs, from what the viewer must see, read and
understand. Every section has a **Must land** list. Those lists are the brief. The shot tables are suggestions.

The builder may:
- make a shot as long or as short as it needs to be;
- merge, split, reorder, add or drop shots inside a section, as long as the Must land list is still met;
- let a voice line run across a cut, or leave a beat with music only;
- give a section with a strong image (the Berlin flash, the Ural Shield gates closing) more room than a section that is
  only information.

The section order stays as written: it follows the timeline.

Guidelines, so that nothing is lost. They are not exact minimums: a shot may come in a little under them when the
cut works, and a shot may run longer than it strictly needs when that is good for the art (a held wide, a slow
reveal, a beat of silence).
- A shot with a voice line usually lasts about as long as the line, plus a breath. A line may also run on over the
  next shot.
- A date or place card stays on screen long enough to read comfortably.
- A stamp stays long enough after it lands for the word to be read.
- Map table moves are slow enough that the viewer can read the labels the camera passes.
- Avoid starting a new card or stamp while the previous one is still being read.

There is no length target yet; the total length is decided later. For reference, the narration alone is about 3
minutes at a slow briefing pace. If the cut needs to be shorter, the first places to trim are the beats without voice
(shots 3.3, 3.4 and 5.5) and the map pins (shot 3.6).

## What the player may and may not learn

The narrator is wrong on purpose (Lore 2.6). Check every shot **and every word on screen** against this table, not only
the voice-over.

| Topic | Player is told / shown | Must NOT be shown or written |
|---|---|---|
| Purple Death | a western bio-weapon, presumed for genocide, that killed millions | wheat, crops or farms as targets; grain silos; "Phytophage", "Demeter", "defoliant" |
| Operation Svarog | the Soviets nuked large parts of Eurasia, including territory they had captured themselves, presumably in a war with the West | a straight line of strikes; the spread stopping at the strikes; the glass wall |
| Ural Shield | the Soviets withdrew east behind it | why they withdrew (the firebreak) |
| Radixite | born from atomic fire by a chemical reaction; powerful, seems alive, unstable | **any link to the western weapon**: in speech, in text, or in the image. No purple near the crystals, and no purple mould turning into orange crystal (the Act 3 reveal). |
| No more nukes | "for reasons unknown" | any link between Radixite and nukes (the Act 4 answer), for example crystals on a rocket or near a warhead |
| CSD | a council of three replaced the single leader | Stalin, Svetlana, the mutiny |

Sections 5 and 8 sit next to each other, so keep them visually apart. The Purple Death is purple, wet and organic.
Radixite is orange, hard and crystalline. Do not use a transition that dissolves one into the other.

## Visual language (applies to all sections)

- **Two grades.**
  - *Archive grade* for 1944-1975. It has these qualities:
    - desaturated and warm brown
    - film grain and gate weave
    - light scratches
    - a slight vignette

    It reads as a reconstruction from fragments.
  - *Present grade* for 1976: the normal game look (heat haze, orange Radixite light). The switch to full colour in
    shot 9.2 is the emotional turn.
  - Both grades are post-process materials on the cinematic cameras. **Make.**
- **The map table.** A resistance command bunker in 1976: concrete, one oil lamp, and a large table with a physical
  map of Eurasia and the Atlantic.
  - The table carries every "big picture" moment (Manhattan, the spread, Berlin's followers). So we never need assets
    we do not have, such as American cities or Allied aircraft.
  - The map is built from these `EarthWorldMap` country meshes, laid flat with a paper and ink material, plus pins,
    red string and burn marks (**Have**, in `/Game/EarthWorldMap/Meshes/Countries/8k/`):
    - `SM_Germany`
    - `SM_Russia`
    - `SM_United_States_of_America`
    - `SM_Ukraine`
    - `SM_Poland`
    - `SM_Belarus`
    - `SM_United_Kingdom`
    - `SM_Kazakhstan`
  - **Make:** the bunker set, the paper material, and the animated stain mask for the Purple Death spread.
- **Intel stamps.** Rubber-stamp text slammed onto the screen or onto the map, next to a claim.
  - They show, without saying it, that the briefing is made of fragments.
  - The mild set is used:
    - `CONFIRMED` on the blueprints (2.2), Berlin (3.5) and Manhattan (4.2)
    - `UNKNOWN` on "stopped using nukes" (9.1)
  - Do not stamp the narrator's guesses (genocide, the reason for Svarog). That would point at the later reveals.
  - **Make** as a UMG widget. The InfoTakeover "Rotsignal" look (black Frontstahl, dull red) is a good base.
- **Date and place cards.** Typewriter text, lower left, on two lines: the date in large type, then the place in small
  caps (for example `JANUARY 1945` / `BERLIN`).
  - `W_Cinematic_CenterText` exists (**Have**, `/Game/RTS_Survival/Cinematics/W_Cinematic_CenterText`).
  - Check whether it can take this layout before making a new widget.
- **Subtitles.** These are the spoken words without the accent spelling, always in English.
  - Use the existing transcript pipeline: `DT_CinematicTranscripts` (**Have**,
    `/Game/RTS_Survival/Sounds/VoiceLines/DT_CinematicTranscripts`) and the portrait subtitle widget.

## Asset status key

- **Have:** seen in the project on 2026-11-14 (path given).
- **Have?:** known from earlier work, but not re-checked for this doc.
- **Not located:** expected to exist, but a file search for this doc did not find it.
- **Make:** must be built for this level.

---

## Section 0: Cold open

**Must land:** the history that follows is pieced together from fragments.

| # | Visual | Spoken | On screen |
|---|---|---|---|
| 0.1 | Black. Faint radio static and a slow Geiger tick. | none | The archival notice fades in, typewriter style, centred (text below). |
| 0.2 | The notice burns away from the centre, like old film catching on a hot projector lamp. Lamplight on concrete shows through the hole (leads into 1.1). | none | none |

Archival notice text (Lore 2.9, shortened):

> ARCHIVAL NOTICE
> Thirty years of static, ash and, well, the nukes, left huge blackouts in our history.
> What follows is compiled from the limited data available.

## Section 1: The briefing

**Must land:**
- It is 1976.
- The player is the Kommandant of the German resistance.
- This is a briefing from one of his officers, not an objective history.

Present grade.

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 1.1 | Bunker interior. The oil lamp swings slightly and dust falls from the ceiling. Slow push along the map table, past an MP40, a radio set and a mug. | "Kommandant, it is good to have you back." | Kommandant, it is good to have you back. | Card: `1976` / `RESISTANCE COMMAND BUNKER`. Narrator portrait appears top middle and stays for the whole cinematic. |
| 1.2 | Top-down over the map. A hand-drawn date line runs along the table edge from 1944 to 1976. The camera tilts down onto the 1944 end and the image drains to the archive grade. | "It is my job to brief you on what we know today." | It is my job to brief you on what we know today. | none |

Assets:
- Bunker set: **Make.** Reuse concrete and props from the Breakthrough and flak bunker kits where possible.
- Map table: **Make** (see Visual language).
- Radio and small props: **Have?** (FieldSupply props).
- Narrator portrait: **Not located** as a separate asset. Memory says the GermanAnnouncer portrait lives in
  `W_BP_Portrait`.

## Section 2: October 1944, the stolen blueprints

**Must land:**
- Germany had the V2 and used it against Britain.
- Soviet spies stole the plans.
- The Soviets built their own copy.

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 2.1 | Dusk, a forest launch site. A German V2 lifts off its launch table. The camera tilts with it into the clouds. | "In October of 1944, a month after we successfully launched our first V2 rocket against ze British," | In October of 1944, a month after we successfully launched our first V2 rocket against the British, | none |
| 2.2 | Night, a drafting room. A torch beam crosses a V2 cutaway blueprint. A gloved hand photographs it, flash by flash. Only the coat sleeve and a red-star cap badge show in the torch spill. | "Soviet spies managed to capture our blueprints" | Soviet spies managed to capture our blueprints | Card: `OCTOBER 1944` / `HEERESVERSUCHSANSTALT PEENEMÜNDE`. The blueprint prop carries `GEHEIME KOMMANDOSACHE`. Stamp: `CONFIRMED`. |
| 2.3 | A Soviet hangar. Under work lights, a V2 painted Soviet green sits on a transport cradle, with workers on scaffolds. | "und created their own rocket program." | and created their own rocket program. | none |

Assets:
- V2 launch: **Have** `/Game/RTS_Survival/Cinematics/V2Scene/BP_Scene_V2`.
- Blueprint prop: **Make.** Use an orthographic in-engine render of the V2 mesh as a paper texture.
- Spy arm and hand: **Have?** A Soviet infantry mesh from `/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Rus/RusBasic/`.
- Soviet V2 and hangar (**Have**):
  - `/Game/RTS_Survival/Cinematics/V2Scene/BP_Scene_RusV2`
  - `/Game/RTS_Survival/Cinematics/V2Scene/V2materials/MI_V2_RussianGreen`
  - `/Game/RTS_Survival/Cinematics/V2Russian/SM_SM_Russian_V2Hangar`

## Section 3: January 1945, Berlin

**Must land:**
- The Soviets got the atom bomb first and put it on the rocket.
- Berlin was destroyed by a single nuclear strike.
- More strikes followed.

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 3.1 | Snow. A Soviet launch plateau. The green rocket rises off its stand and the crew shield their faces. | "1945... ze year it all came down. Ve were told ze atomic bomb was decades away." | 1945... the year it all came down. We were told the atomic bomb was decades away. | none |
| 3.2 | Close on the rocket nose cone in flight, at high altitude, with the curve of the earth behind it. A Soviet marking is hand-painted on the warhead casing. | "But God, were we wrong. Ze Soviets managed to win ze race for ze atom und zhey used our rocket technology to carry it." | But God, were we wrong. The Soviets managed to win the race for the atom and they used our rocket technology to carry it. | none |
| 3.3 | The Berlin skyline at dawn, seen from far outside the city in silhouette only, so the ruin-kit meshes read as an intact city. Church spires and the Brandenburg Gate shape. | none (the music drops) | none | Card: `JANUARY 1945` / `BERLIN` |
| 3.4 | A white-out flash. Hold on white, then the mushroom cloud rises over the silhouette. The sound cuts to a low hum. | none | none | none |
| 3.5 | Present-day Ger_Berlin ruins in the archive grade: a glassed street, the Brandenburg Gate ruin, falling ash, slow dolly. Berlin stays clean, with glass and ruin only: no purple and no Radixite (see `BerlinMission.md`). | "Berlin was ze first target" | Berlin was the first target | Stamp: `CONFIRMED` |
| 3.6 | Map table. Pins on other German cities get a burnt ring one after another. The pins have no city labels. Each one makes the lamp flicker and dust fall. | "und many followed." | and many followed. | none |

Assets:
- Soviet launch (**Have**, in `/Game/RTS_Survival/Cinematics/V2Russian/`):
  - `BP_Scene_RusLaunchManager`
  - `SM_Scene_Rus4_V2Plateau_Stairs`
- Nuke: **Have** `/Game/RTS_Survival/Cinematics/V2Russian/BP_Scene_Nuke`. The V2 rocket ability effects are a second
  source (**Have?**).
- Berlin (**Have**):
  - `/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin`
  - `/Game/RTS_Survival/Blueprints/Environment/Landmarks/BrandenburgGate/BP_BrandenburgGateRuin`
- Skyline silhouette: **Make.** Use ruin-kit pieces against a bright dawn sky.
- Earth-curve shot: **Make.** Use a sky sphere and a high-altitude backdrop.

## Section 4: May 1945, the Manhattan strike

**Must land:**
- The Soviets also struck America, at the Manhattan Project sites.
- The narrator believes they did it out of fear of a western bomb.
- After this, the Soviets dominate.

No American assets exist (only a test Sherman), so this section takes place on the map table.

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 4.1 | Map table. The camera travels west across the Atlantic along a red string pinned from eastern Europe. | "But as intel shows us now, ze Third Reich wasn't zheir only target." | But as intel shows us now, the Third Reich wasn't their only target. | none |
| 4.2 | The string ends at the USA, at three pins with paper tags. The lamp is lowered and each tag chars and curls, one after another. | "Und in May 1945 zhey struck across ze ocean, nuking ze facilities of ze Manhattan Project;" | And in May 1945 they struck across the ocean, nuking the facilities of the Manhattan Project; | Card: `MAY 1945` / `MANHATTAN PROJECT SITES, USA`. Tags: `LOS ALAMOS`, `OAK RIDGE`, `HANFORD`. Stamp: `CONFIRMED`. |
| 4.3 | Wide on the table: three burn rings in the USA, and a Soviet star pin pressed into the middle of Europe (the "absolute Soviet hegemony" from Lore 2.0). | "clearly afraid of ze western coalition developing nuclear technology of zheir own!" | clearly afraid of the western coalition developing nuclear technology of their own! | none |

Assets: the map table (**Make**) and the country meshes (**Have**, see Visual language).

## Section 5: August 1945, the Purple Death

**Must land:**
- The West answered with a silent, biological weapon.
- It killed millions.
- The narrator believes it was meant for genocide.
- It spread across Eurasia.
- It is purple.

Show only the effect on people and places, never the target: no farms and no wheat. The weapon looks the way the
narrator believes it was, a weapon for genocide.

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 5.1 | Night. High clouds lit purple from inside. A single unmarked canister falls through the frame (no aircraft shown). | "Ze western powers could not accept defeat und in zheir dying breath zhey unleashed something else," | The western powers could not accept defeat and in their dying breath they unleashed something else, | Card: `AUGUST 1945` / `LOCATION UNKNOWN` |
| 5.2 | Dawn in a 1945 trench line: helmets, a field telephone, a crate of rations. Fine purple dust settles over everything. | "not a bomb but a silent weapon of some biological material." | not a bomb but a silent weapon of some biological material. | none |
| 5.3 | Time-lapse in the same trench. The purple mould grows and rounds off the trench corners. Veins wrap the crates and a helmet. Bodies lie under it. | "Whatever ze hell it was, it worked too well. Millions died from direct contact with it." | Whatever the hell it was, it worked too well. Millions died from direct contact with it. | none |
| 5.4 | Map table. A purple stain bleeds out from several points across Eurasia at once, so no single start point reads as a farming region. Cut away **before** it reaches the Urals: never show where it stops. | "As far as we know it was some kind of bio weapon for genocide." | As far as we know it was some kind of bio-weapon for genocide. | Red pencil label on the map: `THE PURPLE DEATH` |
| 5.5 | A bog and dead forest at dusk, in the archive grade: smoking peat and purple algae on a river. | none (music only) | none | none |

Assets:
- Phytophage materials (**Have**, in `/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/`):
  - `MI_Phytophage_AshMauve`
  - `MI_Phytophage_BlackPlum`
- The "Suffocator" mats, veins and pustules (Lore 2.7): **Make.** Drive the growth with a material mask parameter
  from Sequencer.
- Trench: **Have** `/Game/RTS_Survival/Blueprints/Environment/Trenches/FieldTrench/BP_FieldTrenchSpline`.
- Bodies: **Have** the corpse pose data assets in `/Game/RTS_Survival/Blueprints/Squads/SquadUnits/CorpsePoses/`.
  Check which poses suit 1945 soldiers.
- Canister: **Make.** Give it no label and no text, because a label would give away "Demeter".
- Bog: **Have?** `/Game/RTS_Survival/Maps/GerMissions/Ger_PripyatMe410` (peat smoke, purple algae).

## Section 6: 1946-1947, Operation Svarog and the Ural Shield

**Must land:**
- A wider war followed.
- The Soviets nuked large parts of Eurasia. The narrator thinks it was part of that war.
- Most of Europe is destroyed today, because the Soviets used the bombs on territory they had captured themselves.
- The Soviets withdrew east behind the Ural Shield: a massive wall of iron and concrete that runs from Russia's
  northern border to its southern border.

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 6.1 | A Soviet tank column drives through dead, blackened forest in falling ash. Artillery flashes beyond the ridge. Wrecked vehicles lie at the roadside. | "Things escalated quickly after zhis" | Things escalated quickly after this | Card: `1946 - 1947` / `OPERATION SVAROG` |
| 6.2 | From a hilltop: several nuclear flashes far apart, on different horizons. They are scattered, **not** in a line. The column halts and the crews watch. | "und we think ze Soviets nuked most parts of Eurasia as part of a larger war zhey fought against ze western coalition." | and we think the Soviets nuked most parts of Eurasia as part of a larger war they fought against the western coalition. | none |
| 6.2b | Map table or a high wide shot: burn rings and craters across Soviet-held Europe, behind the Soviet front line (the strikes fall on their own side). Ruined towns in the archive grade. | "Most of Europe today is destroyed due to ze Soviets using ze weapons on zheir own captured territories." | Most of Europe today is destroyed due to the Soviets using the weapons on their own captured territories. | none |
| 6.3 | The column drives east toward a vast wall of iron and concrete across a mountain pass. Huge iron gates part, the column enters, and the gates close. Dust settles. | "After zhis zhey moved far eastward und hid behind ze Ural Shield." | After this they moved far eastward and hid behind the Ural Shield. | Card: `THE URAL SHIELD` |
| 6.4 | Pull back high and wide: the wall runs north and south out of sight, over mountains, to both horizons. Or the same on the map table: one line drawn down the length of Russia. | "Ze Ural Shield is a massive wall of iron und concrete, spanning from ze northern to ze southern Russian border." | The Ural Shield is a massive wall of iron and concrete, spanning from the northern to the southern Russian border. | none |

Assets:
- Dead forest and ridge: **Have?** `/Game/RTS_Survival/Maps/GerMissions/Ger_2ndOreMountains`.
- Soviet tanks: **Have?** The T-34 family game units.
- Wrecks: **Have**
  `/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Military/DestTanks/BP_Env_DestroyedT34`.
- Nukes: **Have** `BP_Scene_Nuke` (as in section 3), placed far away and scaled.
- Ural Shield wall and gates: **Make.** A massive wall of iron and concrete (concrete body, riveted iron plate and
  iron gates). It must read as continuous from horizon to horizon.
  - Reference for the open and close motion: the existing gate BPs
    `/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/BP_RusWallGateA` and `BP_RusWallGateC` (VAT
    animation).

## Section 7: 1948-1975, the Directorate

An addition to Lore 2.8, based on Lore 2.9 "A Revolution".

**Must land:**
- Behind the wall, the Soviet state changed.
- There is no single leader any more. A council of three rules: the Central Soviet Directorate.
- The resistance knows this only from incomplete intercepts.

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 7.1 | Behind the gate, a dim hall. Portraits of a single leader are taken down, and three banners unroll in their place. | "Behind zat wall, something changed. Our intercepts no longer speak of one leader," | Behind that wall, something changed. Our intercepts no longer speak of one leader, | Card: `1948 - 1975` / `REPORTS INCOMPLETE` |
| 7.2 | Close on the three banners, lit one by one: Shield, Forge, Harvest. | "but of a council of three. Ze Central Soviet Directorate." | but of a council of three. The Central Soviet Directorate. | none |

Assets:
- Director textures: **Have** in `/Game/RTS_Survival/Textures/Directoriate/` (`DirectorOfShield`, `DirectorOfForge`,
  `DirectorOfHarvest`, with PBR sets). Check whether these are emblems or portraits before using them.
- Hall and banners: **Make.** Use cloth banners as skeletal or WPO meshes.
- Old leader portrait: show the frame only from behind or blurred. No face and no name.

## Section 8: Radixite and the desert

**Must land:**
- The atomic fire caused a reaction deep in the ground.
- Radixite came out of it: a powerful resource that seems alive and is unstable.
- The heat dried up the water and turned the land into desert.

**Must not land:** where Radixite comes from. The narrator does not know, and the image must not suggest it (see the
table at the top).

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 8.1 | A deep crater at night. Cracks in the floor glow orange from below. | "Ze atomic fire sank deep into ze bedrock" | The atomic fire sank deep into the bedrock | none |
| 8.2 | Orange Radixite crystals push out of the cracks, then slowly grow, pulse and hum. Blighted dead ground spreads around them. No purple anywhere in the shot. | "und caused a chemical reaction we now call Radixite." | and caused a chemical reaction we now call Radixite. | Label: `RADIXITE` |
| 8.3 | Close on a crystal. A gloved hand reaches toward it. The crystal pulses brighter and the hand pulls back. | "Radixite is powerful as a resource, but it seems alive und very unstable." | Radixite is powerful as a resource, but it seems alive and very unstable. | none |
| 8.4 | Time-lapse: a lake shrinks to cracked mud and a river bed bakes dry. Then a wide desert with heat shimmer, dust devils and half-buried 1945 wrecks. | "Zhese chemical reactions have caused massive heat waves, which dried up most bodies of water und turned many parts of ze landscape into a scorching desert." | These chemical reactions have caused massive heat waves, which dried up most bodies of water and turned many parts of the landscape into a scorching desert. | none |

Assets:
- Crystals: **Have**, in `/Game/RTS_Survival/Blueprints/Resources/ChildBps/Radixite/` (they already grow and make
  sound):
  - `Resource_Radixite_Crystal`
  - `Resource_Radixite_Crystal_Large`
  - `Resource_Radixite_Crystal_LargePoint`
- Blight: **Have?** The Radixite blight MPC, subsystem and material function (`Source/Docs/Environment/RadixiteBlight.md`).
- Desert (**Have**):
  - `/Game/RTS_Survival/SurvivalMode/Maps/SM_Desert_01`
  - `/Game/RTS_Survival/Blueprints/Environment/AmbientFx/NS_AmbientDustDevils`
- Drying lake: **Make.** Animate the water plane height and blend a wet-to-dry material. Base: **Have?** the
  shore-foam pond on Ger_IntroMission.
- Crater: **Make.** Or dress a crater on an existing landscape.

## Section 9: 1976, what survived the fire

**Must land:**
- The Soviets stopped using nukes, and nobody knows why.
- Most Soviet outposts in the West are abandoned. A few are still held.
- Thirty years have passed, and the German survivors live as nomads in the desert.
- The Soviets think the Germans are dead.
- The resistance is going on the attack.

The grade returns to full colour in shot 9.2. From there on, everything is the present.

| # | Visual | Spoken | Subtitle | On screen |
|---|---|---|---|---|
| 9.1 | Archive grade. An abandoned Soviet rocket hangar: a green V2 on its cradle, rust streaks, and sand drifted against it. **No crystals** (see the table at the top). | "For reasons unknown, ze Soviets have stopped using nukes ever since. Und most of zheir outposts are now abandoned." | For reasons unknown, the Soviets have stopped using nukes ever since. And most of their outposts are now abandoned. | Stamp: `UNKNOWN` (on the nukes line, before the outposts line) |
| 9.2 | Back in the bunker. The lamp flares and colour returns. The camera rises from the 1976 end of the date line on the map table. | "It has been thirty years." | It has been thirty years. | Card: `1976` / `THIRTY YEARS LATER` |
| 9.3 | Montage in the desert, present grade: scavengers prise plates off a wreck, a nomadic HQ unfolds (VAT deploy), and a Mammut glider comes in low over the dunes. | "Our survivors have adapted to ze heated landscape." | Our survivors have adapted to the heated landscape. | none |
| 9.4 | A Soviet outpost at dusk: concrete walls and a guard tower, with a Soviet sentry scanning an empty horizon through binoculars. This shot makes clear who "they" are. | "Zhey think zhey sterilised ze earth. Zhey think we are dead." | They think they sterilised the earth. They think we are dead. | none |
| 9.5 | Reverse angle: beyond the dune, German tank hulls crest the ridge one by one, with dust in the low sun. | "It's time to show zhem" | It's time to show them | none |
| 9.6 | The lead tank's gun fires at the outpost. Cut to black on the muzzle flash. | "what survived ze fire!" | what survived the fire! | none |
| 9.7 | Cut from black into the existing logo animation `LS_RTS_Logo` (reused from the old cinematic). | none | none | The logo, as animated in `LS_RTS_Logo`. |

Assets:
- Hangar and Soviet V2: **Have** (see section 2).
- Scavengers: **Have**
  `/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers_Cinematic`.
- Nomadic HQ VAT deploy: **Not located** by file search for this doc. Memory says it is committed on the
  `nomadic-vat-deploy` branch.
- Mammut glider: **Have** `/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut/BP_GliderTankDelivery_Mammut`.
- German tanks: **Have?** Game units.
- Soviet sentry: **Not located** by file search (I looked for a hazmat squad). A Rus infantry unit from
  `/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Rus/RusBasic/` works as a stand-in.
- Outpost: **Have**:
  - `/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Rus_Wall_Concrete_Long`
  - `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_GuardTowerMG`
  - Place them on `SM_Desert_01` or a new desert set.
- Logo animation: **Have** `/Game/RTS_Survival/Cinematics/LS_RTS_Logo` (with its Geiger sound
  `/Game/RTS_Survival/Sounds/Cinematics/Logo/LogoGeiger`). It is bound to the old `RTS_IntroCinematic` map and its
  `BP_V2Scene_Manager`, so check that it plays in the new level, or rebuild the same animation there.

---

## Full narration (for voice recording)

The spelling below is how the line is spoken, for the voice and text-to-speech pass. The subtitles use the same words
without the accent spelling.

> Kommandant, it is good to have you back. It is my job to brief you on what we know today.
>
> In October of 1944, a month after we successfully launched our first V2 rocket against ze British, Soviet spies
> managed to capture our blueprints und created their own rocket program.
>
> 1945... ze year it all came down. Ve were told ze atomic bomb was decades away. But God, were we
> wrong. Ze Soviets managed to win ze race for ze atom und zhey used our rocket technology to carry it.
>
> Berlin was ze first target und many followed.
>
> But as intel shows us now, ze Third Reich wasn't zheir only target. Und in May 1945 zhey struck across ze ocean,
> nuking ze facilities of ze Manhattan Project; clearly afraid of ze western coalition developing nuclear technology
> of zheir own!
>
>  ze western powers could not accept defeat und in zheir dying breath zhey unleashed something else, not a
> bomb but a silent weapon of some biological material. Whatever ze hell it was, it worked too well. Millions died
> from direct contact with it. As far as we know it was some kind of bio weapon for genocide.
>
> Things escalated quickly after zhis und we think ze Soviets nuked most parts of Eurasia as part of a larger war zhey
> fought against ze western coalition.
> Most of europe today is destroyed due to the soviets using the weapons on their own captured territories.
> After zhis zhey moved far eastward und hid behind ze Ural Shield.
> ze ural shield is a massive wall of iron und concrete spanning from the northen to the southern Russian border
>
> Behind zat wall, something changed. Our intercepts no longer speak of one leader, but of a council of three. Ze
> Central Soviet Directorate.
>
> Ze atomic fire sank deep into ze bedrock und caused a chemical reaction we now call Radixite. Radixite is powerful
> as a resource, but it seems alive und very unstable.
>
> Zhese chemical reactions have caused massive heat waves, which dried up most bodies of water und turned many parts
> of ze landscape into a scorching desert.
>
> For reasons unknown, ze Soviets have stopped using nukes ever since. And most of their outposts are now abandoned.
>
> It has been thirty years. Our survivors have adapted to ze heated landscape. Zhey think zhey sterilised ze earth.
> Zhey think we are dead. over time most of the

### Changes from Lore 2.8

Lore.txt itself is not edited. The narration keeps the 2.8 wording, with these changes only:

1. **Accent spelling made consistent.** 2.8 mixes the two forms: "the" and "ze", "and" and "und", "they" and "zhey".
   The narration uses "ze", "zhey", "zhis", "und" everywhere. The meaning does not change.
2. "Millions died **to** direct contact with it" became "Millions died **from** direct contact with it" (grammar).
3. "as part of a larger war **which they thought against** the western coalition" became "as part of a larger war
   **zhey fought against** ze western coalition". The original sentence was broken.
4. **Radixite origin removed.** "...und the radioactive material fused with the biological parts of what we assume to
   be the western bio weapon. This caused a chemical reaction we now call Radixite" became "...und caused a chemical
   reaction we now call Radixite". The narrator must not reveal the origin.
5. "For reasons unknown **now** the Soviets have stopped using nukes ever since" became "For reasons unknown, ze
   Soviets have stopped using nukes ever since". The word "now" contradicted "ever since".
6. The Directorate paragraph (section 7) is new.
7. The user's own edits (2026-11-14):
   - the "1945... ze year it all came down" opening of section 3;
   - "Clearly" dropped from the start of section 5;
   - the Europe line and the Ural Shield line in section 6;
   - "Und most of zheir outposts are now abandoned" in section 9;
   - the closing paragraph, which is still being rewritten.

Lore 2.8 still contains the origin line. Update it there too if 2.8 should match the cinematic.

## Build notes for the level agent

- **Map.** Make one new map, for example `/Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976`, with a
  sublevel or streaming set per section.
  - Shoot in the large existing maps (Ger_Berlin, SM_Desert_01, Ger_2ndOreMountains, Ger_PripyatMe410) with their
    own shot sequences. Do not copy their landscapes.
- **Sequence.** Make one master Level Sequence with a shot track: one subsequence per shot. Then a single shot can be
  rendered again on its own.
- **Voice first.** Record one wave file per subtitle row, so the timing can move without recording again. Lay the
  voice track down before cutting, so shot lengths follow the recorded takes. Fill `DT_CinematicTranscripts` from the
  subtitle column.
- **Render.** Render with Movie Render Queue, and render the audio separately (headless MRQ renders mute 3D audio).

---

## Build ledger (kept by the build agent)

Status 2026-11-14: pipeline spike done, nothing final built yet.

### Architecture (proven by a headless spike, 2026-11-14)

- **Map:** `/Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976`. The mission maps are **streaming sublevels**
  (`LevelStreamingDynamic`, loaded, not visible) and each shot turns its location on with a **Level Visibility track**.
  None of the four maps uses World Partition. A two-map test (Ger_Berlin then SM_Desert_01) rendered with each map's own
  sun and sky and a clean switch.
- **Sets made for the film** (bunker and map table, hangar, hall, space) go in small sublevels under
  `/Game/RTS_Survival/Maps/Cinematics/Intro1976/`, also switched by visibility tracks, so their lights never mix.
- **Cinematic-only actors on mission maps** are spawnables in the shot sequences or live in cinematic-owned overlay
  sublevels. The mission maps are never saved.
- **Render:** Movie Render Queue from the command line in a headless `-game` run
  (`-LevelSequence=... -MoviePipelineConfig=...`), preset `/Game/RTS_Survival/Cinematics/Intro1976/Render/MRQ_Preview`.
- **Fog of war:** the mission maps' `BP_FogOfWar_Manager` blacks the whole frame through a post-process material. It must
  be switched off for the film (the spike used `ShowFlag.PostProcessMaterial 0`, which would also kill the grades).
- **RTS actors under a non-RTS game mode** log about 1,800 "failed cast to CPPGameState" errors per map. They are
  editor-only dialogs (auto-answered with `-unattended`) and do not stop the render.
- **Tools:** `Source/Tools/IntroCinematic/` (`intro_editor_server.py` + `intro_cmd.py` drive a throwaway editor;
  `intro_lib.py` has `safe_save`, which refuses any package outside the cinematic folders).

### Incident: Ger_Berlin.umap was written by the spike (2026-11-14 22:38 and 22:44)

`EditorLevelUtils.add_level_to_world` makes the added sublevel the current level. A test `LevelSequenceActor` and a
`save_current_level` therefore went into Ger_Berlin. The actor was removed and the map saved again. Export-table diff
against the last commit: all placed actors are intact; the **navmesh storage changed** (4 `RecastNavMesh` actors, 28.9 MB
-> 0 actors + 4 `RecastNavMeshDataChunk`, 57 MB; file 62 MB -> 90 MB). **Open: needs `RebuildNavigation` + save with
the map opened on its own. Waiting for the user's go-ahead.** Copy of the current file:
`Saved/IntroCinematic/backup/Ger_Berlin_2244_after_my_saves.umap`.

### Asset audit (2026-11-14, paths checked on disk)

Corrections to the status marks above:
- Narrator portrait: **Have** `/Game/RTS_Survival/Textures/GameUI/Announcer/SB_Portrait_AnnouncerScope`
  (+ `M_UI_AnnouncerScope`, `T_Portrait_AnnouncerScope`); `W_BP_Portrait` is in `/Game/RTS_Survival/Blueprints/GameUI/Announcer/`.
- Nomadic HQ VAT deploy: **Have** on main: `/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/HQ/`
  (`DA_HQ_Deploy`, `SM_HQ_Deploy`, VAT textures, `MI_HQ_Deploy_00..13`).
- Soviet infantry meshes: `/Game/RTS_Survival/Blueprints/Squads/SquadUnits/SkeletonsRus/RusSoldier/SK_RussianSoldier`
  (the `Rus/RusBasic` folder holds unit Blueprints only). Hazmat squads exist (`.../Rus/RusHazmat/`, `.../Rus/T2Hazmat/`).
  No binoculars animation exists.
- FieldSupply: only merged building meshes are in UE. Bunker props to use instead: `/Game/UVB80/Assets/UVB80/Items/`
  (`SM_Radio`, `SM_Cup`, `SM_OldPhone`, `SM_Typewriter`, `SM_WoodTable`, `Paper/`), MP40
  `/Game/RTS_Survival/Blueprints/Squads/InfantryWeapons/German/MP40/Mesh/SM_Ger_InfWeapon_MP40`, interior kit
  `/Game/Underground/Meshes/` (`SM_Wall_200u`, `SM_Ceiling_200u`, `SM_Floor_200u`). No oil lamp mesh exists (Make).
- V2 mesh: `/Game/RTS_Survival/Meshes/Weapons/V2/SM_V2Rocket`. `BP_Scene_Nuke` uses `/Game/BigExplosions/Niagara/NS_Nuke_*`
  and has no sound; no dedicated nuke sound exists. Do not use `NS_RadixiteNuke` (green).
- Phytophage kit already has mats, veins, pustules, wraps and curtains:
  `/Game/RTS_Survival/Blueprints/Environment/Prypyat/Phytophage/` (`SM_Phytophage_*`, `M_Phytophage_Solid/Web`).
- Corpse poses for 1945 soldiers: `DA_CorpsePoses_GerSoldierVarA`, `DA_CorpsePoses_Mosin`
  (meshes `.../CorpsePoses/Poses/SM_CorpsePose_<Type>_NN`).
- Radixite: only `Resource_Radixite_Crystal_Large` has the loop sound; the growing ones are
  `Resource_Radixite_Crystal_LargeWithGrowth`, `_LargeWithGrowthFast`, `_SmDecal_Growth`.
- Directorate: `DirectorOf*` textures are PBR emblem plates, not portraits. Banner emblem materials exist:
  `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietFlags/Mat/Emblems/MI_Emblem_DirectorOf{Shield,Forge,Harvest}`.
- EarthWorldMap: all 8 meshes confirmed; 207 countries available, plus `Meshes/Support/SM_WaterPlane`. No globe mesh.
- Logo: `LS_RTS_Logo` (35 s at 60 fps) binds a `CineCameraActor` and `BP_V2Scene_Manager` of the old map; logo mesh
  `/Game/RTS_Survival/Meshes/Logo/RadiantRuins`. It must be rebound or rebuilt for the new map.
- Fonts: `/Game/RTS_Survival/Fonts/` (`RR_mainFont`, `NotoSerif`). No typewriter font exists (Make or pick).
- Grain: `/Game/Brushify/Materials/PostFX/Grain/M_Grain`. No vignette, scratch or sepia material exists (Make).
- **Keep out of frame in Berlin (3.5):** `SM_Berlin_DemeterPod` with its `T_DP_DemeterLabel`.

### Progress 2026-11-15 (greybox stage)

Built so far (all under `/Game/RTS_Survival/Cinematics/Intro1976/` and `/Game/RTS_Survival/Maps/Cinematics/Intro1976/`):
- **Film:** master `LS_Intro1976_Master` with 41 shot sequences (`Sequences/LS_Intro_<id>`), about 5 min 48 s
  including the 35 s logo. Built from data: `Source/Tools/IntroCinematic/intro_manifest.py` (see the tools README).
- **Voice:** scratch narration only, free offline Windows TTS (`Audio/VO_Scratch/VO_Intro_<row>`, 34 rows, 199 s).
  Rows 9_5 and 9_6 are the PLACEHOLDER closing line. Subtitles come from `Data/DT_IntroTranscripts` (cinematic-owned;
  `DT_CinematicTranscripts` is untouched until the final lines exist).
- **Screen layer (C++):** `Source/RTS_Survival/Cinematics/IntroCinematic/` (subsystem, HUD widget, game mode);
  cue rows in `Data/DT_IntroCues`.
- **Sets:** `Intro_Bunker` (map table from the EarthWorldMap country meshes, pins, strings, tags, burn rings, stain),
  `Intro_Draft`, `Intro_Space`, `Intro_Skyline`, `Intro_Clouds`, `Intro_Trench`, `Intro_Hall`, `Intro_BerlinSky`
  (sky overlay for Ger_Berlin), `Intro_Ural`.
- **Modelled in Blender and imported:** `UralShield/` (segment, gate frame, two leaves, far piece) and `Props/`
  (map table, oil lamp, canister, pins, paper tags, burn ring, banner, picture frame). Sources and READMEs in
  `Saved/IntroCinematic/import/`, scripts in `Source/Tools/IntroCinematic/blender/`.
- **Materials:** `Materials/` (MPC_Intro, M_Intro_Grade for both grades, map paper, burn ring, stain, text planes).

Decisions and deviations (for the final report):
- The old `RTS_IntroCinematic` map is streamed in as a set (V2 pad, Soviet pad, hangar, logo); its cameras, sequences
  and timing are not used, and its own sequence actors are stopped by the subsystem.
- The Ural Shield stands on a plain of its own (`Intro_Ural`), not on Ger_2ndOreMountains: that map is 720 m across
  and terraced, too small for a wall of 60 m modules running to both horizons.
- Shot 6.4 shows the real wall only. The map table carries no Ural line, so nothing next to the stain hints at a barrier.
- Shot 2.1 uses the old map's open launch plain (no forest set exists); 3.1 has no snow.
- Gameplay actors placed in the streamed maps (units, squads, mission markers) are destroyed for the play session
  only; the map assets are not changed.

### State 2026-11-15: first complete version rendered (superseded where the next section says so)

**Deliverables**
- Level: `/Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976` (plays by itself: its sequence actor auto-plays
  the master). Master: `/Game/RTS_Survival/Cinematics/Intro1976/LS_Intro1976_Master`, 41 shots, 329.1 s.
- Final render: `Saved/IntroCinematic/Intro1976_1080p.mp4` (1920x1080, 30 fps, H.264 + AAC, audio and subtitles);
  frames in `Saved/IntroCinematic/Render/MRQ_Final/` (9,872 JPEG); audio mix `Saved/IntroCinematic/audio/intro_mix.wav`.
- Contact sheets per section: `Saved/IntroCinematic/sheets/section_<n>.jpg`; cut check: `Saved/IntroCinematic/cuts.jpg`.
- Headless end-to-end play: `Source/Tools/IntroCinematic/game_run.sh` (plain `-game`, exits by itself at the end).
- Rebuild everything: see `Source/Tools/IntroCinematic/README.md`.

**Placeholders (to replace)**
- All narration is scratch text-to-speech (Windows voice). Final lines: `VoiceLines/VoiceChanger` after the closing
  paragraph is finished and the spend is approved. Rows 9_5 / 9_6 ("It's time to show them / what survived the
  fire!") are the marked placeholder closing line.
- Subtitles read `Data/DT_IntroTranscripts` (word times estimated from the scratch takes). The subtitle class
  `UW_IntroCinematicSubtitles` points at that table in C++; when the final lines are in `DT_CinematicTranscripts`,
  remove that override. `DT_CinematicTranscripts` has not been touched.
- Typewriter face: the engine's DroidSansMono (no typewriter font exists in the project).

**Deviations from the shot tables**
- 0.2 is merged into 0.1 (the notice burns away onto the bunker).
- 2.1: open launch plain of the old intro map, no forest, no dusk. 2.2: torch on the blueprint, no hand or camera
  (no arm rig); a figure stands at the board but stays in the dark. 2.3 / 9.1: the old map's hangar from outside,
  no workers, no rust or drifted sand. 3.1: no snow.
- 3.2: procedural earth, no hand-painted marking on the warhead.
- 3.3: the silhouette is made of ruin-kit meshes and reads as partly ruined before the flash.
- 5.2 / 5.3: the trench is a simple built set (the field trench kit needs a landscape to sit in); the growth uses
  the existing `SM_Phytophage_*` meshes. Weakest section visually.
- 6.3 / 6.4: the Ural Shield stands on its own plain (`Intro_Ural`); 6.4 is the real wall, not a line on the map.
- 8.1: no modelled crater (glow on the desert floor). 8.3: no reaching hand (the scavenger mesh carries lettering
  that could not be checked against the table, so the figure was removed). 8.4: the oasis lake does not shrink;
  the cut from oasis to dry plain carries it.
- 9.4: the sentry uses the artillery-spotter pose (no binoculars prop or animation exists).
- 9.7: `LS_RTS_Logo` is used unchanged, but the shot ends at 15.4 s: at about 16 s its event track makes the old
  scene manager open the menu map.
- Audio: the offline mix lacks the tank engine loop (`S_Medium_Engine_Rus` could not be exported); it plays in game.

**Things the owner should rule on**
- Section 7: `MI_Emblem_DirectorOfHarvest` shows red crystal-like shards on its banner, before Radixite is
  introduced in section 8.
- The archive grade keeps purple at full strength (so "it is purple" lands in 1945); everything else is drained.
- The map stain and the Svarog burn rings never share a shot, and the stain is not on the map in 6.2b.

**Technical notes**
- New C++ (`Source/RTS_Survival/Cinematics/IntroCinematic/`) is in the Development editor DLL only.
- Cinematic runs switch the infantry cover search off on the command line (`bM_EnableCoverSearch=False`): with it on,
  `URTSCoverFinderWorldSubsystem::TryMeasureTrunk` crashed once when a streamed map was hidden mid-scan. This is a
  gameplay-code issue to fix before the intro is played inside the real game flow.
- Streamed mission maps are never saved by the tools. **Open: `Ger_Berlin.umap` navmesh (see the incident above).**

### State 2026-11-15, second pass: Make assets, four adversarial rounds, cinematic pass

This section replaces the numbers and the deviations above where they differ.

**Deliverables now**
- Master: 42 shots, 330.8 s (3.4 is split into 3.4a, the flash over the whole city, and 3.4b, the ruin under the cloud).
- Final render: `Saved/IntroCinematic/Intro1976_1080p.mp4` (1920x1080, 30 fps, audio and subtitles), frames in
  `Saved/IntroCinematic/Render/MRQ_Final/`, sheets in `Saved/IntroCinematic/sheets/`, cut check `cuts.jpg`.
- Headless `-game` run (`game_run.sh`) plays 0 to the logo and exits by itself.

**Built since the first version**
- Blender kits (scripts in `Source/Tools/IntroCinematic/blender/`, assets in `Intro1976/Props`, `/UralShield`, `/Sets`):
  map-table props, the Ural Shield kit, a 1945 trench with props and six growing mould meshes, a strike crater with
  modelled cracks.
- Sets: `Intro_Trench` (rebuilt on the kit), `Intro_Crater` (night crater for 8.1-8.3, own fixed exposure),
  `Intro_SkylineRuin` (the same place as `Intro_Skyline` after the strike), `Intro_Clouds` (four procedural cloud
  sheets), `Intro_Space` (the planet is drawn analytically by the sky dome material, no faceted limb), `Intro_Hall`
  (lamps, beams, steps), `Intro_Ural` (world-aligned soil, yard behind the gate).
- 3.3 now reads whole before the flash (plain intact silhouettes) and ruined after it (3.4b, with a cloud that rises).
- 5.2 / 5.3: real trench; the growth is the kit's own mould (no `SM_Phytophage_*` mesh is used any more); 5.3 goes in
  low on the fire step where the growth takes the helmets.
- 8.1-8.3: modelled crater at night, cracks glow ember orange with the MPC scalar `Crystal`, crystals grow in 8.2.
- 8.4a: the oasis lake drains in the shot (Sequencer lowers the map's water actor for the shot only; the map is not saved).
- 9.3b: shot from the sunlit side. The baked HQ deploy does not visibly unfold (see open points).
- 9.4: sentries on a firing step behind the outpost wall, looking out over the empty desert.
- 9.6: ground blast and light at the shot, then the burn-out; the portrait stays through it and cuts with the shot.
- Svarog rings (6.2b): seven more strikes far west, north, south and east, so they form no belt.
- C++ additions (same folder, Development DLL only): `MaterialRamp` cue (runs a scalar material parameter over a
  cue), mesh-component hide rule (instanced scatter such as `SM_DeadWillow` in the Pripyat map), stamps and the
  portrait end with a cut, the film waits for shader compiles.

**Cinematic pass**
- Two-key camera moves run at constant speed (in motion at both cuts); `"move": "ease"` keeps a cubic move;
  `"handheld": <degrees>` lays a small seeded drift over a move (3.1, 5.2, 6.1, 9.3a, 9.5, 9.6).
- Moves changed: push-in on Berlin that the flash interrupts (3.3 into 3.4a), tilt-up with the cloud (3.4b), wider
  whip pan between the Svarog strikes (6.2), crane up as the gate leaves close (6.3), faster rising pull-back (6.4),
  one descending push across the 8.1 / 8.2 cut, pan that holds the glider from the first frame (9.3c).
- Weather is drawn by the grade (`M_Intro_Grade`, MPC scalars `Rain` and `Sand`): sleet over ruined Berlin (3.5),
  rain on the dead swamp (5.5), dust across the Ural plain (6.4), wind-driven sand in 8.4b, 9.4, 9.5 and 9.6.

**Adversarial rounds** (four fresh reviewers on the full render, 2026-11-15). Hard-rule points they raised, all
fixed and checked by measurement on the final frames: the 9.6 white-out washed over the portrait (the portrait now
stays at full strength through flash and black and cuts with the shot); lilac slabs beside the crystals in 8.x (no
violet pixel left); the Svarog rings formed one belt (outliers added, all on Soviet-held land from Germany
eastward); the stain looked like slowing (5.4 now cuts while its area still grows). No reviewer has seen the film
after the last fixes and the cinematic pass: the owner reviews it next. Picture complaints NOT fixed:
- 9.3a: the scavenger figures and the wreck's tarp read flat orange (game materials as they are in this light).
- 9.3b: the HQ does not unfold; its deploy materials fail to compile for one shader permutation in the render process.
- 2.3 / 9.1: the same dark hangar twice; no lit Soviet rocket in 2.3, no sand drifts or rust in 9.1.
- 3.3: the intact city is simple cut-out blocks. 6.4: the wall crosses a bare plain, no ridges.
- 3.1: no snow. 2.2: no hand or camera flash (two attempts at a flash read as fog and were removed).
- 5.4: the pencil label and strings read pale brown-pink in the archive grade, not red.
- 8.x: the cracks are a regular plate pattern (Voronoi), not branching fissures.

**Lessons for the tools** (also in the README)
- A base material whose texture parameter default does not match its sampler type compiles in the editor but fails in
  the `-game` / render process: every instance then draws as the grey default material, with only one warning in the
  render log. `render.sh` users: grep the log for `Failed to compile Material` under `Intro1976`.
- `M_Intro_Grade` runs after tone mapping: bright emissive clips to white before the grade sees it. Keep glows low
  and lock exposure in dark sets (`fixed_exposure` in `intro_sets2.py`).
- Game Niagara rain and sandstorm systems did not show in these shots; the grade draws the weather instead.
- Never replace the scalar list of `MPC_Intro`: collection nodes find their scalar by id, so every material built
  before breaks in the render process ("CollectionParameter has invalid parameter"). `build_mpc` only appends.
  `render.sh` now prints the cinematic materials that failed to compile; the line must be empty.

### State 2026-11-16: owner's fixes, then two adversarial rounds

The owner asked for four fixes, then two rounds of a fresh adversarial reviewer with fixes after each.

**Owner's fixes**
- **Fire on the strike markers** (3.6 Germany, 4.2 USA, 6.2b Svarog). Each burn ring now catches fire when its clock
  reaches it: two crossed upright cards with procedural flames (domain-warped noise, licks that tear off at the top),
  sparks and a smoke plume, an additive light pool that flickers on the paper, and the struck city's pin glows
  red-hot and chars black. Flames stay inside the archive grade's orange window (blue near zero, bright), so they
  keep their colour in the 1945 look. All of it runs off the existing ring clocks (`intro_fire.py`). The clocks run
  on through 4.1 (Germany) and 4.3 (USA) and are parked at 60 s in 5.4 and 9.2, so no map shot shows a stale fire.
- **The Purple Death** (5.2, 5.3, 5.4). New Blender veins (`blender/sets/build_veins.py`): branching networks from
  seven places where it took hold, over the floor and creeping along both walls, 1.8 cm trunks forking down to 3 mm
  capillaries with nodules, half sunk into the earth. New material `M_Intro_PurpleDeath` on the mats, pustules and
  veins: necrotic plum-black flesh, bruised violet, dry spore crust, a livid magenta vein network that beats, light
  pumping outward along the veins. The growth is revealed by MPC `Mould` (a ragged front that spreads from each
  pivot, veins that crawl out along their length) instead of scaling the meshes. The map stain (5.4) grows vein
  tendrils ahead of its front.
- **Ammunition on the table** (1.1): three blued MP40 magazines, an opened 9 mm box with rounds standing in it, and
  loose rounds (Blender prop `SM_Intro_AmmoSpread`), next to the MP40.
- **T-34s no longer float** (6.1, also 6.2, 6.3). Cause: the column drove across a steep side slope beside the valley
  road, level, seated by one centre trace (one track up to 1.5 m higher than the other). Now every tank key is
  seated on the ground under its four track corners (height, pitch and roll; hull and turret follow the full
  rotation), the column follows the valley road itself, and the tracks drive (forward animation and panning track
  material). 6.1 looks down the road toward the ruined arches, a wreck at the roadside. Resource pickups of the Ore
  map (glassy bubbles beside the road) are hidden.

**Adversarial round 1** (fresh reviewer on the full render, 2026-11-16). Fixed:
- Hard rule: a scrap of Ger_Berlin's blowing street litter read as a purple speck in the 3.5 sky. New MPC scalar
  `PurpleKeep` (appended; 1 by default): the archive grade keeps purple only where it is 1. 3.5 sets it to 0.
- Purple Death: less glow (only thick veins, faintly), low specular, the vein tubes read as continuous vessels with
  a soft pulse; helmets and mess tins in painted field grey (`MI_Intro_FieldGrey`) instead of polished steel.
  (A subsurface shading model was tried and dropped: the low sun shone through and turned the flesh hot pink.)
- Map fires: flames about 40 % lower, card bottoms fade into the paper (depth fade), burn rings are textured char
  with a dull ember edge instead of a white "sparkler" ring, the burnt tags cast no detached shadow.
- 1.1: the phone and MP40 no longer cut through the radio and each other (phone moved past the ammunition).
- 4.1: the strings are thin twine (0.09 cm), not rods.
- 5.4: the stain is opaque, its veins darker than the body (no pale veil); framed west so its eastern edge is never
  in frame.
- 6.1: lower camera (1.65 m), ash on the wind (grade `Sand`), three distant artillery flashes beyond the arches.
- Archive grade: scratches rarer, of varied width and strength.
Not taken (bigger builds): Berlin skyline and cloud remodel (3.3/3.4), Ural Shield remodel and gate motion
(6.3/6.4), 6.2 nuke shockwave dome, hangar re-dress (2.3/9.1), hand and camera flash (2.2), 9.3b HQ materials.

**Adversarial round 2** (second fresh reviewer, 2026-11-16). Fixed:
- Hard rule (sections 5 and 8 apart): the mould, its veins and the map stain used the same Voronoi cell pattern as
  the Radixite crater floor. The shared vein function (`PD_VEINS` in `intro_materials.py`) is now a domain-warped
  ridged multifractal: long branching vessels, no cells.
- Hard rule risks: the 5.4 stain's north-eastern front was in frame (framed further west now, and the stain runs on
  to 0.9 at the cut); four eastern Svarog rings stood in a north-south row (three moved east or west).
- Map fires: flames rise out of the char (depth fade 2.5 cm: the flame dissolves wherever the paper is just behind
  it, so its base follows the scorch, no straight cut); the light pool is smaller and weaker; each scorch is
  turned and sized differently; the red-hot pins glow dull, not orange lollipops.
- Soviet star pin pressed in (head a few mm over the paper), no hard shadow (it read as a second star).
- Trench helmets: field-grey paint at 0.02 albedo (this set is exposed for very dark surfaces; 0.07 still read as
  porcelain in the sun).
- 6.1: the Ore map's parked civilian cars are hidden (one read as a van driving in the column); tanks 20 m apart.
- Archive grade scratches weave from frame to frame and break up along their length.
Not taken: tag burn and curl (needs WPO, which python cannot reach), bunker re-dress and lamp swing, overlay
re-framing, 4.1 graticule, 0.2 film-burn rework, 3.4b cloud swap, 9.4 sentry pose, 8.4b dressing, 5.1 canister fins.

**Confirmation review** (a third fresh reviewer judged the render against the owner's four requests only). Fixed:
- T-34s at the Ural gate (6.3) hovered: they now use the same four-corner ground fit as 6.1 (traces start 6 m up,
  under the gateway lintel), and the set suns cast contact shadows (`contact_shadow_length` 0.06 in `sun()`): far
  from the world origin the shadow bias alone left a lit strip under the tracks.
- Operation Svarog (6.2b) strikes had no marker: each strike now has a pin that burns and chars like the German
  cities; it only appears on the map half a second before its strike (masked pin material), so 1945 shots show none.
- US tags: the burn front sweeps across each tag from one corner with a dull ember rim and ash holes behind it,
  and the tag is replaced by the kit's curled tag (charred) the moment its burn takes it (`MI_Intro_Tag_*` VanishAt,
  `tags_curl` spawnables in 4.2, 4.3, 9.2).
- 5.2 growth without the bright "fresh front" tint; 5.4 stain bruise-dark (not pastel) with the coastlines showing
  through.

**Final-frame review** (a fresh reviewer graded the final render's frames against the owner's requests). Fixed:
- 6.3 tanks still read as hovering: the track geometry sits on the ground (measured), but the sun's shadow map,
  10 km from the world origin, starts the shadow well behind the tracks and the contact shadows did not close it.
  Tanks with `"contact": True` get a soft translucent black blob on the ground under the hull
  (`M_Intro_ContactShadow`, a plane about 2 m past the hull and 1.8 m past each track) that bridges the lit strip
  into the real shadow.
- US tags popped: the flat tag vanished half a second before the curled tag spawned, which then smeared with spawn
  motion blur. The curled tag now exists for the whole shot and the two materials hand over on the same burn clock
  (`MI_Intro_Tag_*` VanishAt 0.545 hides the flat tag, `MI_Intro_TagCurled_*` VanishAt -0.545 shows the curled
  one); the curled tag keeps more of its charred paper (`AshShift`) so the curl still reads.
- Purple Death veins were flat neon ribbons: the tubes are now plum-black wet vessels (albedo ~0.02, wetter
  specular) with a thin glowing core where they face the camera, light pumping along it; the vessel lines on the
  mats are thinner and sharper; the growth front is less ragged so no island of mould appears ahead of the body.
Weak passes left as they are: scorch rings are all the same shape, the MP 40 magazines are plain blocks, 9.2 does
not show the US map.
