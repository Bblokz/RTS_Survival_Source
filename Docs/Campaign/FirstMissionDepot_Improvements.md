# Ger_FirstMission: improvements (picked 2026-11-16)

Status:

- **Built (2026-11-16):** the two megaphones M1 (patrol base) and M2 (final-road camp), with 10 Russian lines in total.
  They loop random lines until destroyed. They are placed on the map and the map is saved. See
  `Source/Docs/Missions/MegaphoneBroadcast.md`.
- **Not built yet:** everything else in this doc:
  - the radio chatter C1-C3;
  - the megaphones' first-approach portrait line with subtitles;
  - the depot truck and crane;
  - the measuring post.

The user picked these three ideas from `FirstMissionLorePass.md`:

1. Soviet radio chatter and megaphone broadcasts, placed **outside the fuel depot**.
2. A working fuel depot, shown **during the depot cinematic takeover**.
3. An abandoned Soviet measuring post near the Radixite crystals.

Coordinates are Unreal units (cm). Game north is -Y and east is +X, which matches the voice lines ("the depot to the
north of your position"). The positions come from an actor dump of the saved map (2026-11-16). Every new actor still
needs a placement check in the editor: ground height, navmesh, and clearance from the BXPs. Do not seat anything by
actor bounds; several BXPs report huge bounds.

The reveal rules are in the "What the player may and may not learn" table in `IntroCinematicScript.md`. All three
ideas stay inside "safe now".

---

## 1. Soviet radio chatter and megaphone broadcasts

### Goal

The Soviets in the West think nobody is left out there. They are bored, hot and sure of themselves. Hearing that
before each fight makes the uprising land as a surprise and backs the intro line "Zhey think we are dead".

### Rules

- **Not at the depot.** The depot megaphone `Rus_Bxp_Megaphone` (-14663, 12268) stays the alarm system that
  `BP_Mission1_AttackDepot` casts to (`PlaySiren`). Nothing in this section touches it.
- **Voice.**
  - Russian, with English subtitles; the pipeline is in `VoiceLines/AGENTS.md`.
  - Radio chatter: Vlasov through the "Broken transmitter" chain.
  - Megaphones: the same voice through a loudspeaker or PA-style chain.
- **Portraits.** Radio chatter plays on the existing `SovietMosin` and `RusHazmat` portraits.
- **At most one chatter exchange per mission beat.** It queues behind the advisor's lines and never interrupts them.
- **Megaphones play in two layers.**
  1. The first time a player unit comes within about 40 m of a megaphone, its current line plays once as a portrait
     line, so it gets subtitles.
  2. After that, the megaphone loops its broadcast as positional audio (`Att_Propaganda`), without subtitles, until
     it is destroyed.
- **Megaphone actor.** The new megaphones are not alarms, so they can use `Rus_Bxp_Megaphone_Propaganda1`, which
  already plays the `S_SovietPropaganda` loop. Its sound becomes a list of the new lines.
- **Megaphone loop (user, 2026-11-16).**
  - Every megaphone has many lines, all in Russian.
  - As long as it is not destroyed, it keeps playing them, picking a random line each time, never the same line twice
    in a row, with a pause of a few seconds between lines.
  - The loop stops when the megaphone is destroyed.
- **Megaphone voice (2026-11-16).**
  - Vlasov, Russian, eleven_v4, stability 0.5, through a loudspeaker treatment.
  - Three treatments are being auditioned in `VoiceLines/VoiceChanger/Output/MegaphoneAudition` (A Tannoy close,
    B Across the yard, C Worn horn). The pick is used for both megaphones.
  - After the pick: 8 more Soviet lines in the same way.
  - **Done 2026-11-16.**
    - The user picked B, "Across the yard".
    - All 10 lines (the 2 audition lines and 8 new ones) are in
      `/Game/RTS_Survival/Sounds/VoiceLines/RusFaction/Propaganda/Megaphone`.
    - The final M1/M2 lines are the table in `Source/Docs/Missions/MegaphoneBroadcast.md`; it replaces the draft M1/M2
      tables below.
    - M1 = `Rus_Bxp_Megaphone_Broadcast` at (500, -17700).
    - M2 = `Rus_Bxp_Megaphone_Broadcast_FuelRation` at (12400, -30000). It was moved west of the tents, because the
      draft spot (13600, -30500) sat in a 1 m gap between two tents.

### Places

| # | Where | Who / what | When it plays |
|---|---|---|---|
| C1 | Bridge: the far bank and the MG bunker `Rus_BXP_MGBunkerOneCamoed` (-6118, 7945) on the ridge | Two bridge guards on the radio (`SovietMosin`) | At the start of `BP_MIssion1_Bridge`, queued after `GoodworkDestDepo_NowClearRoad` |
| C2 | Beyond the bridge: the dug-in `Rus_Squad_HazmatEngineers` (-1737, 10799) in the Soviet field trench | A Hazmat trooper muttering to himself, the "Canary" (`RusHazmat`) | When the first player unit gets within about 35 m of the squad (trigger box or distance poll) |
| C3 | Ruined town: the garrisoned high-rises `Garrison_PostApo_HighriseLarge` (-4628, -4812) and (-1398, -5715) | The men inside, on their radio (`SovietMosin`) | In `BP_Mission1_Nuclear`, just before the `BurnOutSoviets` line, so it sets that line up |
| M1 | Patrol base, inside the gate `Rus_Building_GateWired2` (-464, -18444). Candidate spot about (500, -17700), between the gate and the barracks. | New megaphone, Directorate notices | Loops from map start; first approach plays the portrait line |
| M2 | Final-road camp: the tents and fuel tank around (13300..14300, -31300..-29800). Candidate spot about (13600, -30500). | New megaphone, fuel ration notice | From the start of `BP_Mission1_FinalClearRoad` |

### Lines

The Russian is a first draft; have a native speaker check it before recording. Keep each line under about 6 s.

**C1. Bridge guards.** They heard the depot's alarm, but they don't believe it.

| Speaker | Russian | English subtitle |
|---|---|---|
| Guard 1 | Склад докладывает о нападении. | The depot reports an attack. |
| Guard 2 | О нападении? Кто нападает, призраки? | An attack? By whom, ghosts? |
| Guard 2 | С самого Сварога там никого. Небось опять цистерна рванула. | Nobody out there since Svarog. Probably a tank blew again. |

**C2. Hazmat trooper.** He reads his dosimeter and checks his seals. He is afraid of the ground, not of us.

| Russian | English subtitle |
|---|---|
| Двести десять... двести двадцать. Выше, чем на прошлой неделе. | Two hundred ten... two hundred twenty. Higher than last week. |
| Проверить клапаны. Ещё раз проверить клапаны. | Check the seals. Check the seals again. |

**C3. High-rise garrison.** They are complacent and hot. This is the line from the proposal.

| Speaker | Russian | English subtitle |
|---|---|---|
| Soldier 1 | Ну и жара. Хоть на этаже тень. | This heat. At least there's shade up here. |
| Soldier 2 | Сиди тихо. Говорят, у склада стреляли. | Keep quiet. They say there was shooting at the depot. |
| Soldier 1 | С самого Сварога тут никого. Одни призраки да крысы-старьёвщики. | Nobody out there since Svarog. Ghosts and scrap-rats. |

**M1. Patrol base broadcasts** (looped). They show the Directorate, a council, and its rules.

| Russian | English subtitle |
|---|---|
| Внимание, гарнизон Западного сектора. Директорат напоминает: Запад молчит. | Attention, Western Sector garrison. The Directorate reminds you: the West is silent. |
| Дисциплина — это жизнь. Щит держит. | Discipline is life. The Shield holds. |
| Смена караула в восемнадцать ноль-ноль. Дозиметры сдать дежурному. | Guard change at eighteen hundred. Hand your dosimeters to the duty officer. |

**M2. Final-road camp broadcast.** It pays off the depot: the player hears the result of their first strike.

| Russian | English subtitle |
|---|---|
| Колонна снабжения задерживается. | The supply column is delayed. |
| Норма топлива сокращена до особого распоряжения. | Fuel ration cut until further notice. |

### Reveal check

- Safe now:
  - the Directorate exists;
  - "the Shield", which can be read as the Ural Shield or the Director of Shield;
  - the Soviets think the West is empty;
  - the ground is dangerous.
- "Svarog" is named only as a moment in time ("since Svarog"). Nothing says what it was for.
- The Hazmat lines say nothing about where Radixite comes from.

### Assets

| What | Status |
|---|---|
| `SovietMosin`, `RusHazmat` portraits; subtitle path | exist |
| `Rus_Bxp_Megaphone_Propaganda1`, `Att_Propaganda`, `S_SovietPropaganda` | exist |
| Voice lines C1-C3, M1, M2 (13 lines) | need making (ElevenLabs + VoiceChanger) |
| A loudspeaker voice preset | needs making, or reuse "Broken transmitter" with a PA EQ |
| 2 megaphone placements (M1, M2) | need placing and a site check |
| A "first approach plays the portrait line once" hook | needs making: a small mission function or a trigger actor |

---

## 2. A working depot, shown in the depot cinematic takeover

### Goal

When the depot is introduced, the player sees why it matters: a fuel truck pulls in and a crane lifts its barrels into
the depot, while `S_DepotCinematic_1` says the depot supplies the Soviets "throughout Eastern Germany".

### It happens during the cinematic takeover (required)

- The truck and the crane run **inside the depot cinematic takeover**: `LS_FirstMission_FuelDepot`, 37.3 s, with a fade
  track and an audio track, played from `BP_Mission1_AttackDepot` (`DepotCinematic` -> `EndDepotCInematic`).
- The truck **spawns when the takeover starts**, out of shot. It must not exist, or be attackable, before the takeover.
  That keeps the "surprise is our greatest weapon" setup intact.
- The truck reaches its parking spot and the crane lifts **at least one pallet on camera before the takeover ends**.
  - One crane cycle is about 14 s, and the first lift starts about 3 s after `StartUnloading`.
  - So the truck must be parked by about **t = 22 s**. Build the timing back from the camera shot that shows the crane.
- If the truck is late or stuck, the shot must still work: call `StartUnloading(nullptr)` at the shot time. The crane
  then lifts from its own pallet pile (`AMissionFuelCrane` falls back to `PickupPointLocation`). When the truck does
  park, call `StartUnloading(Truck)` to switch it to the truck's cargo.
- **Fog of war.** Enemy units under fog are hidden at actor level, so the truck, the crane's load and the depot guards
  could be invisible in the shot.
  - Reveal the depot for the length of the takeover: `RevealAreaForSeconds`, as `Mission_Intro_ClearSovietRig` does
    for its camera tour.
  - Check what the current cinematic shows today; it may already be missing the guards.

### How (reuse from the Intro mission)

- **Truck.**
  - Use `AEnemyConvoyRoute` / `AConvoy::StartConvoy` with a new `DA_Convoy_DepotFuel`: one
    `Rus_ArmoredCar_ZilFuel` and no escort, so nothing fights during the takeover.
  - Alternatively, spawn one ZIL and give it a move order, as `USecureBaseMission` does.
- **Route.** The road from the west map edge (`BP_RoadSpline2/3`, along y ≈ 12000) into the depot.
  - Spawn the truck a short way out of shot on that road, about (-25000, 12300).
  - Do not spawn it at the map edge: that is about 200 m away and would take too long.
- **Crane.**
  - Place one `BP_MissionFuelCrane` inside the wire fence around the fuel tanks `DepotFuel_1/2/3` (-16643..-15863,
    9776..10582). It lifts the barrels over the fence onto a stack beside the tanks.
  - The truck parks outside the fence, beside the crane.
- **Parking (lessons from the Intro mission).**
  - The truck must stop **parallel** to the fence, within about 1 m of the spot the crane can reach.
  - In the Intro mission, a truck that arrived at an angle stopped nose-in and out of reach. The fix was a straight
    final run along the wall, and the truck stopped about 1.2 m short of its target, so the target was set 1.2 m long.
- **Site survey needed.** Pick the crane and parking spots, and the crane's `PreferredSlewViaYaw`, so that:
  - the jib clears `Rus_Building_FuelStorageLarge`, the pipes and the guard tower;
  - the parking spot is on the navmesh;
  - the spot is in the cinematic camera's view.
- **After the takeover.**
  - The truck and the crane stay. The crane keeps cycling until the alarm goes or it is destroyed. It is an
    `ADestructableEnvActor` and dies in 3 Pz IV shots.
  - The truck has the Intro mission's hitbox swap and sink death (`FMissionPropSinkDeath`).
  - Recommended: neither counts toward the objective (`DepotFuel_1/2/3` stays the goal); they are targets of
    opportunity.
  - Optional: once its cargo is gone, the truck drives back west and leaves the map, so a quick player can catch it.

### Camera

The current sequence has two bindings and a single camera, `CineCameraActor` at (-19952, 13747, 5687), high above the
depot. The truck and crane need to be seen:

- either add a cut to a lower shot of the fence line while the crane lifts;
- or move the existing camera so it frames the fuel tanks during the second line (`S_DepotCinematic_2`, about the
  alarm system).

Keep the alarm megaphone in a shot too, since the line talks about it.

### Test

A headless test in a throwaway DebugGame editor (pattern: `secure_line_headless_test.py`,
`crane_truck_death_test.py`). It should check:

- the truck spawns at takeover start;
- the truck is parked parallel and within reach by t = 22 s;
- the crane has lifted at least one pallet by the end of the takeover;
- no Soviet unit was alerted by the truck;
- the truck and crane are visible (fog reveal) in the camera shot.

### Assets

| What | Status |
|---|---|
| `BP_MissionFuelCrane`, `Rus_ArmoredCar_ZilFuel` (+ `SM_Hull_Zil_Hitbox`), convoy route/spawn code, `RevealAreaForSeconds`, prop sink death | exist (Intro mission) |
| `DA_Convoy_DepotFuel` | needs making (copy of `DA_Convoy_IntroFuel`) |
| Crane + parking placement, camera change in `LS_FirstMission_FuelDepot` | needs making |
| `BP_Mission1_AttackDepot` hook at takeover start / at the shot time | needs making |

---

## 3. Abandoned Soviet measuring post

### Goal

The player finds a Soviet survey post that was measuring a Radixite crystal. Its logbook shows the crystal kept
growing, getting hotter and humming, until the Soviets gave up and left. The message is that Radixite seems alive and
that even the Soviets are wary of it. It does not reveal where Radixite comes from.

### Where

- **Primary.** The east edge of the Radixite field, about (-14800, -30200).
  - The field is on a raised plateau (crystals at z ≈ 875, with `Resource_Radixite_Crystal_Large` at
    (-16407, -30430)). The `Env_Dest_KV-5` wreck (-12039, -31268) sits below it.
  - The post looks across the field at one named crystal, "Specimen 4" (the large one at -16407, -30430).
  - Check that the plateau can be reached by scavengers, and keep the post at least 15-20 m from any crystal and its
    blight so it does not interfere with harvesting.
- **Alternative.** By the crystal cinematic site (`LS_FirstMission_Crystals`, about (-4300, -1900)), which is on the
  main route.
  - Keep it at least 50 m from both `BP_Dest_PostApo_Nuclear` buildings: no Radixite next to the reactor, which is
    the Act 4 rule.
  - The runtime crystal `Resource_Radixite_Crystal_LargeWithGrowthFast` and the three scavenge finds are spawned by
    `BP_Mission1_Nuclear`; check their positions before choosing this spot.

### What it is

- A `Rus_Bxp_ContainerHut` with a thin radio mast or pole, a fallen tripod instrument, and a cable run towards the
  crystal.
- **Survey pole (the key visual).** A striped measuring pole driven into the ground beside "Specimen 4", with the
  crystal grown up past its painted marks; the lowest marks are already swallowed. The crystal grows; it does not
  bend metal or show anything organic.
- **Sound.** Geiger clicks that speed up as a unit nears the pole: `Sounds/Cinematics/Logo/LogoGeiger` as a
  distance-driven loop.
- **Gameplay.** A scavenge object, `BP_ScavObj_MeasuringPost`, a child of `BP_ScavengeableObject`. Recommended: make it
  a fourth tracked find in `BP_Mission1_Nuclear`'s "investigate the anomalies" objective (minimap icon,
  `HasPendingTrackingRadixitescavObjects`), so the player is sent there rather than having to stumble on it.

### Logbook (archive entry)

The text shows when the post is scavenged, as an archive item with custom content (`AddNewCustomArchiveItem`). It is
written as a translated Soviet log.

> **Measuring Post 7, Western Sector**
> Directorate of Shield, Survey Section. Object: crystal formation "Specimen 4".
>
> Day 112. Height 2.31 m. Surface temperature +61 °C.
> Day 140. Height 2.58 m. +64 °C. A low hum, audible at night.
> Day 171. Height 2.90 m. A second spur at the base. Dosimeter off the scale within 5 m.
> Day 172. Request to relocate the post: denied.
> Day 180. Post relocated. Instruments left in place.

Optional advisor line after the scavenge (English, `GermanAnnouncer`):
"Zhey were measuring it, Kommandant. Every week, zhe same crystal... und zhen zhey left in a hurry."

### Reveal check

- Safe now:
  - Radixite grows;
  - it is hot and hums (it already grows and makes sound in game);
  - it is radioactive;
  - the Soviets fear it.
- Never:
  - purple, mould or organic matter near the post;
  - any mention of the western weapon;
  - any mention of nukes or fission next to the crystals;
  - the post near the reactor.
- "Directorate of Shield" names one of the three directors and nothing more.

### Assets

| What | Status |
|---|---|
| `Rus_Bxp_ContainerHut`, `LogoGeiger`, `BP_ScavengeableObject`, archive custom content | exist |
| Striped survey pole mesh (marks + numbers), fallen tripod instrument | need making (small Blender props) |
| `BP_ScavObj_MeasuringPost` + hook into `BP_Mission1_Nuclear` tracking | needs making |
| Logbook text, optional advisor line | needs making |

---

## Open points

1. Russian lines: have a native speaker check them before recording.
2. C3 assumes the garrison has heard about the depot. If the depot alarm never fired (the player destroyed it in
   time), "they say there was shooting at the depot" still works: the depot itself was attacked.
3. Measuring post: the primary (Radixite field) or the alternative (crystal cinematic site) spot?
4. Should the depot truck and crane count toward the depot objective? The recommendation is no.
