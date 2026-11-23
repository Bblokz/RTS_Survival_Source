# Frozen Lake mission (design, working title "Cold Vein")

Status (2026-11-22): **design complete, ready for mission scripting.** Every earlier **(open)** item was decided by the
owner on 2026-11-22. See "Scripting decisions" for the 26 picks, "Mission stages" for the `Missions[]` plan, and "Code
TODOs" for what has to be built first.
- Built so far: the map `Ger_FrozenLake` (first pass, see [FrozenLakeMap.md](FrozenLakeMap.md)), the ice look
  ([FrozenLakeIce.md](FrozenLakeIce.md)) and the class-based ice breaking in C++ (`UFrozenLakeIceSubsystem`, DebugGame
  only).
- No mission scripting exists yet.
- Started 2026-11-16. Opener of **Act 3**, mid-campaign.

Lore reference: `Source/DesignTexts/Lore.txt`. Related: [BerlinMission.md](BerlinMission.md) (seeds the cryo tech),
[PripyatMission.md](PripyatMission.md) (the mission before, last Act 1 map).
Where an older section says something that a later decision changed, the section has a pointer, and the decision wins.

## Where the files are

- **This doc:** `Source/Docs/Campaign/FrozenLakeMission.md` (Source is a submodule; commit from inside `Source`).
- **Layout sketch (variant A, picked 2026-11-16):** `MapDesigner/FrozenLake/frozen_A.layout.json` (outer repo root).
  Renders in `MapDesigner/FrozenLake/out_A/`. Change the layout in the generator
  `MapDesigner/FrozenLake/generators/gen_frozen_A.py` (zone polygons, pumps and roads are baked from the lake outline),
  not in the JSON by hand. Regenerate from `MapDesigner/`:
  `python FrozenLake/generators/gen_frozen_A.py` then `python md.py all FrozenLake/frozen_A.layout.json --out FrozenLake/out_A`.
- Ice is drawn with colour overrides on `ground` regions (MapDesigner has no ice kind): white thick, grey thin, near-black
  vein, blue open water.

## One-line pitch

A frozen lake in a world that should have none. The Forge keeps a whole lake frozen with cryo pumps to sedate and mine
the Radixite vein under it. The ice is the short route, but every vehicle, every shell and every pump the player breaks
changes where the ice holds. Heavy tanks stop being the default.

## Place in the campaign

- **Opener of Act 3 ("The Beating Heart").** After Pripyat (last Act 1 map) and around the Svarog Line (Act 2).
- **Why mid-campaign:** by now the player's army is heavy out of habit. The mission only bites when "bring the Tigers"
  is the reflex it breaks. Early on the player has fewer heavies and no habit to break.
- **Why Act 3:** the Act 3 truth is that Radixite is a living root network, and that the CSD sedates it with cryo-tech
  before harvesting. Berlin showed "the first look at cryo-sedation tech" at a small survey station. This mission is the
  industrial version, and the place where the player *sees* the network alive.
- Location: **north Russia / Karelia**, on the Phytophage (west) side of the Svarog Line. **Decided 2026-11-17: Lake Vygozero** (see
  "Decisions and recommendations"). Ideas considered:
  - **Lake Onega** region: large, recognisable, Karelian.
  - **Vygozero** on the White Sea-Baltic canal (Belomorkanal): a Soviet forced-labour canal with locks and dams, which
    could be reused as the Forge's pump and dam stations. Strong Soviet-industry flavour.
  - A fictional lake with a Karelian name.

## The lore: why the lake is frozen

The world in 1976 is a scorched desert: Radixite heat boils off the water tables. A frozen lake should not exist, and the
player should ask why.

- **It is frozen on purpose, by the Director of Forge.** A large Radixite vein runs under the lakebed. It grew from a
  nearby Svarog firebreak strike. Decided 2026-11-22 (21): the crater shows as a horizon backdrop beyond the north dam.
- The Forge keeps the whole lake frozen with **cryo pumps** so it can sedate the vein and mine it through the ice.
- **Without the pumps the lake would not stay a lake.** The vein's heat would thaw it and then steam it away, like every
  other body of water in the region. The lake is alive only because the Forge keeps it cold. Decided 2026-11-22 (20):
  open water steams, and flooded zones glow amber from the exposed vein.
- **The cold is local.** Snow and frost only reach as far as the pumps do. Beyond that, the ground goes back to the dry,
  scorched land of the rest of the game. On the map this reads as a white disc in a brown world.

### The reveal shot

Through clear black ice near the vein, the player sees the root network **glowing and pulsing below**. This is Act 3's
reveal ("Radixite is a living root network") shown, not told. It should be close to the start or on the intro camera
tour, so the player sees it before the fighting starts.

### The thaw is the vein waking up

- Lore.txt (Act 3): crystals glow, pulse and grow toward heat when heavy weapons fire nearby.
- So the fighting itself wakes the vein. **The harder you fight on or near the lake, the faster the ice thins.**
  (Lore only. Decision 12 has no combat-heat mechanic.)
- ~~A slow base thaw plus combat heat~~. Decided 2026-11-22 (12): no base timer and no combat heat. The thaw is
  stage-driven: when the rig falls, black ice spreads along the vein over 3 minutes.

### The lake is full of the past

Tanks from both sides are frozen into the ice: wrecks from the "massive, mostly unrecorded war" of 1946-47. This is
environmental storytelling for the Act 2 truth (the war that happened across Europe), and it is also salvage (see below).

## Core mechanics

### 1. Load, not just class

**Changed 2026-11-22 (decision 12):** there is no total-weight or recoil model. What is kept: the class rule, bunching
(three or more medium-or-heavier vehicles within 15 m on grey ice count as a heavy) and a 20 s stand-still stress on
grey ice. The bullets below are the original idea.

- Each patch of ice carries the **total weight of everything standing on it**, not just the heaviest unit.
- On grey ice, light vehicles are safe, mediums crack it and heavies break through. Black ice holds infantry only.
- **Bunching up breaks it too**, so spacing matters. A blob of mediums can do what one heavy does.
- **Standing still adds stress over time**, and firing from the ice adds more (recoil). Tank destroyers cannot camp on the
  lake; the ice is for moving across, not holding.
- Infantry is effectively weightless on the ice, but exposed (no cover except pressure ridges and frozen wrecks).
- Tuning seed (not used; decision 12 picked class + bunching, no tonnage model): real ice capacity follows roughly `allowed load = A x h^2` (thickness h in cm, A about 3.5 to
  7 kg/cm^2, Gold's rule). With A = 5, a 30 t medium needs about 75 cm and a 57 t Tiger about 105 cm. This gives a
  believable starting point for the thickness bands below. Arcade and Realistic balance may use different numbers.

### 2. Readable thickness

The player plans routes at a glance, and sees the thaw spread:

| Colour | Meaning | Who can cross |
|---|---|---|
| White | Thick (heavy ice) | Everything: heavy, medium and light tanks and infantry, with spacing for heavies |
| Grey | Thin (medium ice) | Medium and light tanks and infantry |
| Black | Dangerous (infantry ice) | Infantry only |
| Dark wet cracks with a pale cyan pulse | About to open | Nobody should stop here |
| Open water | Hole | Nobody (vehicles sink) |

Decided 2026-11-17: these are the three ice types the shaders and the nav areas use (black = infantry only, so light
tanks need grey or white ice). The "about to open" warning is a cold cyan pulse, not orange, so it is never mistaken for
the amber Radixite vein under the black ice. See [FrozenLakeIce.md](FrozenLakeIce.md).

Decided 2026-11-22 (19): stress shows separately, as a spreading crack decal under the vehicle plus a creaking sound.

### 3. Artillery shapes the ground

- Large HE shells punch **open-water holes**. Howitzers can cut a moat in front of an enemy column, or sink an ice road.
- The enemy can do the same to the player.
- Small calibres chip the ice and add stress but do not open holes. Decided 2026-11-22 (13): HE of 75 mm and up opens holes. The existing landscape
  impact decals already use a > 15 mm cut-off for craters, which may be the natural place to hook this.
- Holes refreeze in 90 s while their pump zone is running (decided 2026-11-22, 13).

### 4. Cryo pumps are the main choice

- The lake is split into **pump zones**. Each zone is kept frozen by a Forge pump station.
- **Destroying a pump floods its zone over time.** The player can drown an enemy crossing or seal a flank.
- The cost: the player loses that short route too, and the vein wakes faster there (more heat for the whole lake, but also
  more Radixite exposed to harvest).
  - Changed 2026-11-22: the faster-waking vein is not a mechanic (decision 12). A flood shows the vein as an amber
    glow under the water (decision 20). Floods never melt the two rig roads (decision 24).
- This is the mission's main strategic decision: which zones to keep frozen for yourself, and which to break.
- **Pumps can only be destroyed, never captured** (decided 2026-11-17); a flood is permanent.
- Flood speed (decided): a 10 s warning plus a 40 s melt front (see "Decisions and recommendations").

### 5. Forge cryo-troopers and player lane preparation

- **Forge cryo-troopers** (cryo-suits, as in the lore) refreeze holes and can build **ice bridges** across open water so
  their heavies can cross. Killing the engineers first is the counter.
- **Player counter-tool:** engineers can **prepare a lane** by laying timber or flooding it to thicken the ice (real
  ice-road practice). A prepared lane lets heavies cross.
- Heavies are therefore not banned. **They cost planning**: a prepared lane is slow to build, visible, and a target.
- Decided 2026-11-22 (11): German engineers, with a "Prepare ice lane" ability granted for this mission only (no
  campaign unlock).

### 6. Going through

- A vehicle that breaks through **sinks**. The crew bails out onto the ice (survivors as a small infantry group).
- The hull stays visible under the ice as a dark shape.
- **Burning wrecks melt through and sink** too, which removes the cover they gave.
- Decided 2026-11-22 (18): no. The hole is hull-sized and refreezes like a shell hole.

### 7. Salvage

- Sunken hulls and the frozen 1946-47 wrecks can be salvaged for **metal** (Scavenger theme).
- **Cutting a wreck out of the ice weakens the ice around it.** Greed thins your own routes.
- **Replaced 2026-11-22 (16): no salvage.** Metal comes from shore nodes. Wrecks and sunk hulls are scenery and cover
  only, so the two points above no longer apply.

## Mission setup (agreed with the owner, 2026-11-17)

These parts are kept. The full flow below is still a draft around them.

### Early game

1. **Arrival.** A camera tour crosses the lake. It ends on the reveal: the glowing Radixite root network under clear
   black ice, close to the player's shore. The player lands on the south-west shore, on a flat area with a build radius
   of about 66 m.
   - Objective that runs the whole mission: **keep the HQ alive**.
2. **Establish the shore.** First objective: reach the ice edge, which plays the reveal.
   - Starting resources: a Radixite seep on the player's shore and a small scrap yard for metal, which runs out.
     Changed 2026-11-22 (decision 16): shore metal nodes carry the economy, and the wrecks are cover only.
3. **Learn the ice.** A scripted Forge heavy tank breaks through the ice near the shore in front of the player. This
   teaches the rule without a tutorial box. Early Forge patrols and light vehicles move on the ice.

### Available from the start

- **The heavy ice road.** One narrow road (10 m, one heavy wide) of Forge-thickened ice runs from the player's landing
  shore to the Shield HQ. It is the only place where heavies can cross the lake without lane preparation (still
  spaced, under the load rule).
  - It is not straight. It leaves the shore at the King Tiger, threads a gap between two pressure ridges in zone 2,
    passes 65-90 m west and north of the rig (inside the rig's guns), crosses the NW vein finger on thickened ice in
    zone 3, and joins the Forge ice road in zone 4 up to the HQ gate. About 623 m long; the land road round is
    about 844 m.
  - Lore: the Forge's own heavy-transport road from its garrison to the old landing shore, the shore the player now
    holds.
  - **Both sides use it.** Shield heavies come down it at the player too.
  - It passes through zones 2, 3 and 4. Flooding zone 3 or 4 cuts it beyond the rig spur. **Changed 2026-11-22
    (decision 24): the zone 2 leg from the player's shore to the rig spur never floods.** Big shells can hole it.
  - Core haul: the rig's W face is about 40 m of thin ice from the road. Since 2026-11-22 a **permanent heavy-ice
    spur** links the road to the rig there (decision 24). Together with the road's zone 2 leg, it is the alternative to
    a zone 1 prepared lane.
- **Optional: the frozen King Tiger.** A 1946-47 King Tiger is frozen into the shore ice at the head of the heavy
  road, about 95 m from the HQ (112 m by path), in sight from the start. **Scavengers free it with their scavenge
  ability; it takes 180 s.** Then it is the player's. This replaces the earlier Tiger II in the wreck bay.
  - Decided 2026-11-22 (10): an `AScavengeableObject` with a new vehicle-reward hook. It always takes 180 s, and it
    has a bonus plate.
- **Point of interest: the burnt Forge convoy** in the south-east, with intel on the pump zones.

### Constant enemy pressure

The AI builds out from the Shield HQ on the north-east shore, which has its own Radixite and metal. Its columns enter
from three directions:

- **N:** heavies with cryo-troopers, down the frozen spillway through the Forge dam.
- **E:** heavies, across the wreck bay.
- **S:** along the south road to the blown bridge, held by a Shield picket. From there the column must cross about
  200 m of zone 1 ice to reach you (about 94 s).

### The Forge answers

Shield heavies arrive with cryo-troopers who build ice bridges, and there are artillery duels over the ice. The ice
keeps thinning from a slow base thaw plus the heat of the fighting. (Changed 2026-11-22, decision 12: there is no base
thaw and no combat heat. Only the vein thaw starts, when the rig falls.)

- **Many of these reinforcements come by Soviet cargo planes** that land on a **landing field behind the Shield HQ**
  (east edge, 90 x 18 m strip, about 45 m from the HQ yard). The planes land, unload Shield heavies and
  cryo-troopers, and fly off; the units then move out by the heavy road, the Forge ice road or the columns' routes.
- Existing system: `AEnemyAirDrop` with delivery **CargoPlane (lands)**. It already has landing points, per-difficulty
  units and a landing order.
- Counterplay (decided 2026-11-22, 6): an airfield radio mast beside the strip, linked with the air drop's
  **OnlyWhileAlive** rule, so destroying it stops new landings (planes already in the air still deliver). Reaching it
  means a raid deep past the HQ.
- Decided 2026-11-22 (6): ground columns from about minute 4; cargo planes only after the rig falls. All numbers
  live in a per-difficulty wave data asset.

## Mission flow (first draft, superseded by "Mission stages")

The agreed parts are in "Mission setup" above. The rest is a proposal to react to:

1. **Arrival on the shore.** Intro camera tour across the lake, ending on the clear black ice and the glowing network
   below (the reveal shot). The player sets up on the near shore.
2. **Learn the ice.** Early Forge patrols and light vehicles on the ice. A small heavy crossing goes wrong in front of the
   player (an enemy tank breaks through), which teaches the rule without a tutorial box. Decided (8): yes, a "Thin Ice"
   field-manual takeover.
3. **The pumps.** The player finds the pump stations and the zones they hold. Breaking or keeping them is the main
   decision for the rest of the mission.
4. **The Forge answers.** Shield heavies arrive with cryo-troopers building ice bridges. Artillery duels over the ice.
5. **The objective (decided 2026-11-22, 1: A, take the core).** Candidates were:
   - **A. Take the core (proposal).** Capture the Forge drill rig over the vein and haul a cryo-sedated Radixite core
     back to the shore. The core carrier is **heavy**, so the player has to prepare a lane and keep the right pump zones
     running to bring it home. Ties every mechanic together in the finale, and the sedated core is the Act 3 evidence.
   - B. Destroy the sedation plant.
   - C. Hold a shore base while Forge ice-road convoys cross; sink them instead of shooting them.
6. **Final objective (decided 2026-11-17): destroy the AI's base.** After the core is home, the last objective is to
   destroy the Shield HQ. The sketch chains it after the core haul (`destroy_ai_base`, 537 m from the HQ to the AI
   base by the tool's route). Likely attack routes: the heavy ice road (needs zones 2, 3 and 4 frozen), the Forge ice
   road from the rig (zone 4), or the long west and north shore road over the canal bridge.
   - Decided 2026-11-22 (2): the placed Shield HQ building only. The question was what counts as "destroyed" for a
     base the AI builds over the mission: only the Shield HQ building, or
     every AI production building. `USecureBaseMission` ("destroy a set of base buildings", with a camera tour) is
     the closest existing class, but it works on buildings placed in the map, not ones the AI builds later.
7. Unlock (decided 2026-11-22, 14): a core tech card on the world map.

## Decisions and recommendations (2026-11-17)

- **Final objective: destroy the AI's base** (decided). See flow step 6.
- **Pumps can only be destroyed, never captured** (decided). A dead pump cannot be repaired, so every flood is
  permanent. That makes each pump a one-way decision.
- **Player engineers can repair the blown south bridge** (decided). The consequence has to be clear to the player:
  repairing it reopens the south road **both ways**. You get a land route south (to the burnt convoy and pump 1), but
  the S column gets its land route back into your base (about 65 s instead of about 94 s over the ice). Only player
  engineers can repair it; the AI does not, and its cryo-troopers only build ice bridges. Decided 2026-11-22 (17):
  shells of 150 mm and up, or bombs, from either side can drop the repaired bridge again.
- **Flood speed (decided 2026-11-17): a 10 s warning, then a melt front that takes about 40 s.**
  - **Warning:** the pump explodes, a voice line plays, the ice cracks, and a cold-cyan warning glow spreads under the zone
    (cyan, not orange, per the ice-look decision).
  - **Melt front:** the ice turns thin, then black, then open water, moving out from the pump at about 5 m/s.
  - **Why:** that is about vehicle speed. A unit driving away from the pump gets out; a unit driving toward it, or a
    heavy standing still, sinks. You can time a pump kill to catch an enemy column in the middle of the zone, and your
    own units still get a fair chance to leave.
  - Instant flooding would sink units with no warning. A flood over several minutes would never catch anyone.
  - Holes in a flooded zone never refreeze (no pump).
- **The lake (decided 2026-11-17): Lake Vygozero, Karelia**, on the White Sea-Baltic canal (Belomorkanal). A real
  name players may know.
  - Vygozero is about 1,250 km², far too big for four pumps. Framing (owner OK 2026-11-22, decision 22): the map is **a
    bay of Vygozero that the Forge cut off with its dam**. The pumps keep only the bay frozen. Beyond the dam the rest of
    the lake has steamed away to a cracked, dry lakebed, like every other lake in 1976. That fits the lore ("without
    the pumps the lake would not stay a lake") and the dam and spillway already on the sketch.
  - The Forge dam, the spillway canal and the pump stations can read as the canal's old dams and locks, reused by the
    Forge.
  - The Belomorkanal was a Soviet forced-labour canal. Handle this with care in text and voice lines; it is real
    history.

## Scripting decisions (owner, 2026-11-22)

Picked from six options each, to close the gaps before mission scripting.

1. **Middle objective: Take the core.** Capture the drill rig, then a heavy carrier hauls a cryo-sedated core to the
   player's HQ, over a prepared lane or the heavy ice road. Flow candidates B and C are dropped.
2. **"AI base destroyed" = the Shield HQ building only.** The placed Shield HQ is the one target, so the final stage
   is a plain `UKillActorsMission` (it already works on placed actors). Buildings the AI builds later do not count.
3. **Rig capture: kill the garrison, then reach the deck.** A `UKillActorsMission` for the placed rig garrison and its
   deck guns, then any player unit entering the deck completes it (the new reach-area stage).
4. **Carrier loss: recut the core and retry.** If the carrier is destroyed or sinks, the rig cuts a new core in 120 s
   and a new carrier spawns at the rig. Losing the carrier never loses the mission; only losing the HQ does. The cost
   is time, while the vein thaw (decision 12) spreads and the waves keep coming.
5. **Pumps: one bonus objective, "destroy all four pumps".** The owner's own option. One green bonus plate counts the
   four pumps down (`UKillActorsMission` on the placed pumps). Completing it gives the player a supply drop of
   **Radixite, metal and vehicle parts** (amounts are tuning values). Breaking single pumps stays a free tactical
   choice; only all four complete the bonus.
   - All four pumps gone floods the whole lake **except the rig's roads** (decision 24). The core haul therefore stays
     possible, and the bonus can run from early in the mission.
6. **Enemy pressure: escalate after the rig falls.**
   - Ground columns start at about minute 4 and come every ~3 minutes, rotating N, E and S (`AEnemyWaveSpawner`).
   - Cargo planes (`AEnemyAirDrop`, CargoPlane) start landing behind the Shield HQ once the rig is captured.
   - An **airfield radio mast** beside the strip is linked with the air drop's `OnlyWhileAlive` rule: destroying it
     stops new landings (planes already in the air still deliver).
   - **The waves are data, not Blueprint values (owner):** a data asset holds every wave and air delivery, editable
     per difficulty: start time, interval, direction or entry, units per difficulty, and the air deliveries' units.
     The wave spawners and the air drop read from it.
7. **Weak ice: high cost, but allowed.** Pathing avoids ice too weak for a unit's class, but the player can still put
   it there with a move order. The ice then warns and breaks (`UFrozenLakeIceSubsystem` already breaks by class). This
   gives real "my mistake" moments.
   - Change: in `BP_NavArea_FL_IceNoHeavy` and `BP_NavArea_FL_IceInfantryOnly`, the blocked agents' slot becomes a
     high-cost area instead of Null. Leads stay `NavArea_Null`.
   - Watch out: with a cost instead of a block, the pathfinder will cross weak ice when no other route exists. That
     includes long player orders, so check that ordinary orders across the lake still keep to safe ice. **AI units
     must not take weak ice by themselves** (they would sink themselves), so the AI needs a query filter that treats
     these areas as Null.
8. **Ice lesson: a live sinking plus a field manual.** After the shore stage, a scripted Shield heavy drives a fixed
   route onto grey ice in view of the player's shore and sinks. A voice line explains, then a **"Thin Ice" field-manual
   info takeover** with a clip plays. The scripted tank needs a forced route, because AI pathing keeps off weak ice.
9. **Cryo-troopers: reskinned Hazmat Engineers.** A new squad subtype built on `Squad_Rus_HazmatEngineers`, with a
   cryo-suit material and an ability that refreezes holes and builds ice bridges.
   - The ability writes the same three things the ice uses: the state mask, the hole collision and the nav boxes.
   - Killing the squad stops a bridge that is still being built.
10. **Frozen King Tiger: a fixed 180 s scavenge, with a bonus plate.**
    - It is a scavengeable object. It always takes 180 s, however many squads work on it.
    - When it finishes, the shore collar swaps from C01 to C02 and a player King Tiger spawns
      (`AsyncSpawnMissionVehicle`).
    - Bonus plate: "Free the King Tiger".
    - `AScavengeableObject` gives resources only today, so it needs a small hook for the vehicle reward (an
      on-scavenged event the bonus stage listens to).
11. **Lane preparation: a German engineer ability for this mission only.** The mission grants it; the engineers do
    not keep it afterwards, and it is not a campaign unlock (owner: "only in this mission").
    - "Prepare ice lane": mark a start and an end point. The engineers work about 20 s per 10 m.
    - The result is a 10 m strip of white (heavy) ice.
    - The work is visible, slow and a target for the enemy.
12. **Ice model: class rule, plus bunching and a stage-driven thaw.** This also settles the Gold's-rule tuning seed:
    there is no per-tonnage model.
    - **Class rule** (built): ice breaks under any class it does not hold.
    - **Bunching:** three or more medium-or-heavier vehicles within 15 m of each other on grey ice count as a heavy.
    - **Standing still:** a vehicle that stands on grey ice for more than 20 s cracks it.
    - **Stage thaw:** when the rig falls, the drill stops sedating the vein, and black ice spreads along the vein over
      3 minutes. There is no base timer and no combat heat.
    - The vein runs from the rig straight at the player's shore, so this thaw closes the direct zone 1 haul just as
      the carrier sets out. The player has to pick the zone 2 / heavy-road route or prepare a lane.
13. **Shell holes: HE of 75 mm and up, refreezing in 90 s.**
    - Hole size scales with calibre: about 3 m at 75 mm, about 6 m at 150 mm.
    - Smaller calibres only chip the ice and add stress.
    - A hole refreezes in 90 s while its zone's pump is running. In a flooded zone it never refreezes.
14. **Mission unlock: a core tech card.** The sedated core becomes a world-map tech card (for example a Radixite
    research or income boost; the exact effect is open in the card system). It is also the Act 3 evidence in the
    debrief.
15. **Voice cast: the announcer, the Scavenger and intercepted Forge radio.**
    - **Announcer:** the objectives.
    - **Scavenger:** reacts to the lake, the wrecks and the King Tiger.
    - **Forge radio:** intercepted Forge radio explains the pumps and the vein from the enemy side, using the hazmat
      portrait with a broken-transmitter voice.
16. **Metal comes from the shore.** Normal metal nodes on the shores carry the economy. The 1946-47 wrecks and the
    sunk hulls are scenery and cover only; they cannot be salvaged.
    - This replaces core mechanic 7 ("Salvage") and the v2 idea that the wreck bay is the player's metal.
    - Zone 4's stake shrinks with it. Flooding zone 4 now costs only the heavy ice road's AI-end leg and the wrecks'
      cover. The Forge ice road stays frozen (decision 24), and there is no metal to lose.
17. **The repaired bridge can be dropped again by heavy artillery.** Shells of 150 mm and up, or bombs, from either side
    destroy it again. There is no demolition ability. Engineers can repair it again (this follows from the earlier
    repair decision).
18. **Sinking holes are hull-sized, as built.**
    - A sinking hole refreezes like a shell hole: in 90 s while its pump runs.
    - The ice around it is not affected, and there are no chain sinkings.
    - Burning wrecks still melt through and sink.
19. **Stress feedback: a crack decal and a creaking sound.** While a vehicle stresses grey ice (bunched, or standing
    still), a crack decal spreads under it over the 20 s, with a creaking ice sound. Then comes the cold-cyan warning
    pulse, then the hole.
20. **Open water: steam, with the vein glowing below.** Leads, holes and flooded zones steam (Niagara wisps of warm
    vein water). In flooded zones an amber Radixite glow shows under the water, because the vein is exposed. The water
    level stays fixed: flooded zones never drain to land.
21. **Svarog crater: a horizon backdrop.** A glassed, scorched crater rim beyond the north dam, on the map's horizon
    skirt (see the map-horizon-skirt tool). It cannot be played, but the camera tour and the north shore show it.
22. **Lake framing: a bay of Vygozero, cut off by the Forge's dam.** The pumps keep only the bay frozen. Beyond the dam,
    the rest of the lake has steamed away to a cracked, dry lakebed (backdrop). This confirms the framing proposed on
    2026-11-17.
23. **Core carrier: a Bergepanther-style recovery vehicle.** A turretless heavy recovery tank on the existing Panther
    hull, carrying the glowing core in a cryo cradle on its deck.
    - It must use the **HeavyTank** nav agent and the heavy ice class, even if the Panther itself is classed medium.
    - Player-controlled. It spawns at the rig (`AsyncSpawnMissionVehicle`).
24. **There is always an ice road (owner: no soft-lock).** The rig's own cryo plant keeps two heavy-ice roads frozen
    whatever happens to the pumps. Owner's words: there will always be a road over the ice from the player start to
    the rig, and from the rig to the enemy base.
    - **Player shore -> rig:** the heavy ice road's zone 2 leg plus a fixed spur to the rig's west face (the former
      "short prepared stub" becomes permanent heavy ice).
    - **Rig -> Shield HQ:** the Forge ice road through zone 4.
    - What this changes:
      - A flood melts everything in its zone **except these two roads**. Beyond the rig spur, the heavy ice road
        (its zone 3 vein crossing and its zone 4 leg) can still be cut by flooding zone 3 or 4.
      - The vein thaw (decision 12) does not touch the roads either.
      - Shell holes can still open in the roads. Assumed, not owner-stated: they refreeze in 90 s, because the rig
        plant counts as their pump.
      - So the pump bonus (decision 5) can start early: with all four pumps gone, the lake is open water around the
        two 10 m roads, which both sides must use.
    - **Side effect, together with decision 16:** flooding zone 2 or zone 4 now costs the player very little (zone 2
      keeps the road to the rig, zone 4 keeps the Forge ice road, and there is no wreck metal). Zones 1 and 3 keep
      their stakes. The carrier can always use the road, so lane preparation is optional for the haul. Rebalanced by
      decisions 25 and 26.
25. **Floods cost something again (owner picked two options: assets in the ice, and Forge repair crews).**
    - **Assets in the ice.**
      - The King Tiger's collar sits in zone 2 shore ice. Flooding zone 2 before the King Tiger is freed sinks it
        for good, and the "Free the King Tiger" bonus fails.
      - Zone 4 gets a **second Radixite upwelling**: a north-east vein branch, which sinks when zone 4 floods. It
        needs adding to the sketch, the ice grid and the map. Put it on grey ice or better, so mediums can harvest it.
    - **The Forge sends repair crews.** Every destroyed pump triggers a cargo-plane drop of cryo-troopers with an
      escort. The crew drives to the flooded zone and tries to bridge it, so any flood costs a fight.
      - Existing system: one `AEnemyAirDrop` per pump, with link rule `StartWhenDestroyed` on that pump, delivery
        CargoPlane, and repeat Once.
      - Not owner-stated: these planes land on the same airfield, so whether the radio mast should stop them too is
        open. The link rule holds one actor, so the pump link cannot also carry the mast.
26. **Lane preparation stays worth using: the road is contested.** The road works, but it means a fight. A prepared
    lane is the flanking route.
    - A Shield `AEnemyStrongpoint` holds the road near the rig spur.
    - Forge artillery is pre-sighted on the road.
    - This is placement only. Check that the enemy artillery fires at units on the road by itself.

## Static mesh work

Prompts for delegated modelling, with what can start now and what waits: [FrozenLakeMeshPrompts.md](FrozenLakeMeshPrompts.md).
Work folder: `D:\Blender\BlenderProjects\BlenderTanks\AA_LevelSpecific\FrozenLake` (one subfolder per kit).

## Mission stages (the `Missions[]` plan, 2026-11-22)

Built to the owner's mission Blueprint rules: one `UMissionBase` Blueprint (or a Blueprint child of a C++ mission
class) per stage, a flat `Missions[]` array, each stage started by its own trigger (usually "Mission Class Completed"
on the stage before it), the flow in the event graph, and waits through `ScheduleSingleCallback`. See
[BerlinMissionScripting.md](BerlinMissionScripting.md) for the framework facts.
- Folder: `/Game/RTS_Survival/Blueprints/Missions/GerCampaign/FrozenLake/`, names `Mission_FL_<Stage>`.
- Every started stage shows a plate.

| # | Stage | Parent class | Plate | Trigger | Completes when | On start / on complete |
|---|---|---|---|---|---|---|
| 0 | `Arrival` | `UMissionBase` | Major: "Land on the shore of Vygozero" | none (begin play) | the camera tour ends (not text-only: a text duration shorter than the tour would start the next stages early) | **Start:** location intro (Lake Vygozero, Karelia); camera tour across the lake, past the Svarog crater rim, to the player's shore; set up the enemy forces that are spawned (Shield picket at the south bridge, early patrols). The rig garrison and deck guns are placed in the map. The cargo-plane air drop is placed with `Timing.bStartOnBeginPlay` off. |
| 1 | `KeepHQ` | `UDontLoseHQMission` | Major: "Keep the HQ alive" | class completed: Arrival | never; **fails = defeat** | **Start:** `ScheduleSingleCallback(240)` starts the ground wave spawners (N/E/S rotation, about every 3 min), read from the wave data asset. |
| 2 | `ReachIce` | `UMissionBase` + trigger area | Major: "Reach the ice edge" | class completed: Arrival | a player unit enters the trigger area at the reveal spot | **Complete:** the reveal (camera move over the black ice and the glowing network), a Scavenger line. |
| 3 | `ThinIce` | `UMissionBase` (a normal stage, not text-only, so it cannot complete before the takeover) | Major: "The ice will not hold them" | class completed: ReachIce | the graph completes it when the takeover closes | **Start:** spawn the scripted Shield heavy; it drives its forced route onto grey ice and sinks. **On sunk:** announcer line, the "Thin Ice" field-manual takeover, then grant "Prepare ice lane" to German engineers. |
| 4 | `FreeKingTiger` | `UMissionBase` | Bonus: "Free the King Tiger" | class completed: ReachIce | the 180 s scavenge finishes; **fails** if zone 2 floods first (decision 25) | **Complete:** collar C01 -> C02; spawn the player King Tiger (`AsyncSpawnMissionVehicle`). |
| 5 | `DestroyPumps` | `UKillActorsMission` (4 placed pumps) | Bonus: "Destroy the four cryo pumps" (count) | class completed: ThinIce | all four pumps are dead | **Start:** an intercepted Forge radio line explains the pumps and zones; a trigger area at the burnt convoy plays an intel line. **Each pump death:** its linked air drop sends a repair crew (no graph work; `StartWhenDestroyed`). **Complete:** supply drop of Radixite, metal and vehicle parts. |
| 6 | `RigGarrison` | `UKillActorsMission` (garrison + deck guns placed in the map, set as `TargetActors`) | Major: "Destroy the rig garrison" | class completed: ThinIce | every target is dead | |
| 7 | `BoardRig` | `UMissionBase` + trigger area | Major: "Board the drill rig" | class completed: RigGarrison | a player unit enters the deck area | **Complete:** start the vein thaw (black ice spreads along the vein over 3 min); `FindEnemyAirDrop` + `StartAirDrop` on the cargo-plane drop (its `OnlyWhileAlive` link to the radio mast still applies); Forge radio line. |
| 8 | `HaulCore` | `UMissionBase` + trigger area at the HQ | Major: "Bring the core home" | class completed: BoardRig | the carrier enters the HQ area | **Start:** `ScheduleSingleCallback(120)` cuts the core, then spawns the carrier at the rig. **Carrier dies or sinks:** a 120 s callback, then a new carrier. Never fails. |
| 9 | `DestroyShieldHQ` | `UKillActorsMission` (placed Shield HQ) | Major: "Destroy the Shield HQ" | class completed: HaulCore | the Shield HQ is dead | **Complete:** victory, debrief (the core is the Act 3 evidence), the core tech card. |

- **Not stages:** the bridge repair, single pump kills and the airfield radio mast are free actions. None was given a
  plate. The mast is linked to the air drop with `OnlyWhileAlive`.
- **Tuning values, not decisions:** the first core cut (120 s, the same as the recut), wave timings and units (wave
  data asset), the supply-drop amounts, and the line texts.
- **Lines:** announcer for the objectives, Scavenger for the lake and the King Tiger, Forge radio (hazmat portrait)
  for the pumps and the vein.

### Build status (2026-11-23)

**All ten stage Blueprints are built, compile without errors, are saved, and are wired into `Ger_FrozenLake`.**
- `BP_MissionManager.Missions[0-9]` holds them in the table's order, with "Mission Class Completed" triggers. The map
  had no missions before.
- Wired and saved on 2026-11-23 in a headless editor (`run_wire_fl_stages.py`); the map went from 28.94 MB to 29.00 MB.
  Backup: `Saved/FrozenLake/backup/Ger_FrozenLake_2026-11-23_prewire.umap`.
- Tags added: `FL_CryoPump` on `FL_Kit_pump_s/w/n/e`, and `FL_KingTigerCollarFrozen` on `FL_Kit_tiger_collar`.
- **The pumps can be destroyed (2026-11-23).** `BP_FL_CryoPump_B_Intact` and `_Pump3Dam` were reparented from `Actor`
  to `ADestructableEnvActor`, like the industrial oil tanks (tool: `make_pumps_destructible.py` + `pump_graph_tidy.py`).
  - **Health** is 1660, which is 8 medium-tank shots: the Soviet research building's count (221.4 damage per 75 mm
    shot; (8 - 0.5) x 221.4).
  - The **construction script** runs `SetupComponentCollisions` on the pump mesh, so weapon traces hit it.
  - **When it dies:** `PlayPumpDeathExplosions` (a gas explosion, two medium explosions and a scorch decal), then
    `CollapseIntoRuin` swaps it to `SM_FL_CryoPump_B_Destroyed`. The dam pump uses the same ruin, which is about 3 m
    wider; the edge goes into the dam bank.
  - The collapse broadcasts `OnDestructibleCollapse`. DestroyPumps now counts pump collapses
    (`TrackDestructablesCollapse`) instead of destroyed actors, because the ruin stays in the world.
  - **Not built:** a dead pump does not start its flood yet (the ice subsystem's flood TODO).
  - **Checked** in a fresh headless load of the map: the four placed pumps are destructible with Health 1660 and keep
    their `FL_CryoPump` tag.
  - **Not tested:** units shooting a pump in a game. Backups of the two Blueprints from before the change are in
    `Saved/FrozenLake/backup/`.
They are in `/Game/RTS_Survival/Blueprints/Missions/GerCampaign/FrozenLake/`:
- `Main/` holds the phases in order of play: `Mission_FL_01_Arrival`, `02_KeepHQ` (a `UDontLoseHQMission` child),
  `03_ReachIce`, `04_ThinIce`, `05_RigGarrison`, `06_BoardRig`, `07_HaulCore` and `08_DestroyShieldHQ`.
- `Bonus/` holds `Mission_FL_Bonus_FreeKingTiger` and `Mission_FL_Bonus_DestroyPumps`.

**Tools** (`Source/Tools/FrozenLakeMap/mission/`, re-runnable):
- `fl_stages.py`: the spec of every stage (variables, plate, functions, event graph);
- `build_fl_stages.py <port>`: create or rebuild the stages over UECP;
- `relayout.py <port>`: re-lay the event graphs only;
- `wire_fl_stages.py` + `run_wire_fl_stages.py`: put the stages in `BP_MissionManager.Missions` with their triggers
  and tag the placed pumps (`FL_CryoPump`) and the King Tiger collar C01 (`FL_KingTigerCollarFrozen`);
- graph pictures in `Saved/FrozenLake/graphs/`.

**How the graphs are laid out:**
- one row per flow, read left to right, with one comment box per flow;
- each scheduled wait's custom event sits right after its `ScheduleSingleCallback`;
- getters sit under the pin they feed.

Every stage has a `PlayLine(Line, Speaker)` function, which plays nothing while the line is empty.

**Changes from the table above:**
- **RigGarrison, DestroyShieldHQ and DestroyPumps** are `UMissionBase` stages that track every actor with a tag
  (`TrackActorsWithMiniMapProgress`, with `bCompleteMissionWhenTrackingDone` off), not `UKillActorsMission`. The
  owner tags the actors, and the stages keep their "line, wait, complete" flow in the graph.
- **A tag nobody uses yet** shows a placeholder plate text. RigGarrison also completes itself after 5 s, so the main
  chain can be tested before a garrison is placed.
- **ThinIce** spawns the Shield IS-2 straight onto the grey ice (`CreateSingleAttackMoveDifficultyConditional`), so the
  existing ice subsystem sinks it. No forced route is needed. A 14 s wait covers the sinking.
- **HaulCore**:
  - The carrier is a Panther D placeholder (`CarrierUnit`).
  - A sinking counts as a death, because the ice subsystem kills a sunk tank. So `BP_OnCallBackTankDies` handles both
    a destroyed and a sunk carrier.
  - The HQ circle has unlimited callbacks and only reacts to the carrier.

**Placeholders.** These steps exist as functions that print `[FrozenLake] ... not built yet` on screen, so a test
shows where they will run:
- the Thin Ice field manual;
- Prepare ice lane;
- the vein thaw;
- the pump-bonus supply drop.

`FreeKingTiger.OnZone2Flooded` is a custom event for the pump flood to call later.

**What the owner places or names in the map:**
- wave spawners with SpawnerId `FL_Wave_N`, `FL_Wave_E` and `FL_Wave_S` (list in `KeepHQ.GroundWaveSpawnerIds`);
- the cargo-plane air drop with AirDropId `FL_RigAirDrop` (`bStartOnBeginPlay` off, `OnlyWhileAlive` on the mast);
- tags: `FL_RigGarrison` on the garrison units and deck guns, `FL_ShieldHQ` on the Shield HQ, `FL_FrozenKingTiger` on
  the King Tiger scavengeable object, and `FL_KingTigerCollarFreed` on a hidden C02 collar;
- whether the kit pumps can be destroyed at all (DestroyPumps counts destroyed actors).

**Not done:**
- a fresh-load compile check;
- any test in a game (the owner tests).

## Code TODOs

Everything that has to exist before or while the stages are scripted. Checked against the code on 2026-11-22.

**Mission framework**
- [ ] **Reach area: no new class needed.** `UMissionBase::CreateTriggerAreaSphere/Rectangle` plus
  `BP_OnTriggerAreaCallback` covers ReachIce, BoardRig and HaulCore. Check that the callback can filter to player
  units (and to the carrier only, for HaulCore).
- [ ] **King Tiger vehicle reward:** `AScavengeableObject` has a C++ `OnScavenged` delegate and resource rewards, but
  no Blueprint-assignable event. Expose it (or bind it in a small C++ helper) so FreeKingTiger can react. The scavenge
  duration is squad-specific (speed modifiers such as `TE_ScavengingSpeed`); check whether it must ignore them to
  stay a fixed 180 s, and turn off the resource reward.
- [ ] **Carrier loss events:** HaulCore needs "carrier died" and "carrier sank" (the ice subsystem's
  `OnVehicleStateChanged` is C++ only). Expose both to Blueprint.
- [ ] **Shield HQ target:** `UKillActorsMission::TargetActors` takes placed actors, which is enough for decision 2.
- [x] **Air drop start and mast link:** `AEnemyAirDrop` already supports both. Place it with `Timing.bStartOnBeginPlay` off,
  start it from BoardRig with `StartAirDrop`, and set the link rule `OnlyWhileAlive` to the radio mast.
- [ ] **Wave data asset** (per difficulty): the wave entries (start, interval, entry, units) and the air deliveries.
  `AEnemyWaveSpawner` and `AEnemyAirDrop` have per-difficulty unit lists on each placed actor, but no shared asset;
  make them read from one.
- [ ] **Enemy AI on this map:** check that the AI trains from the Shield HQ yard here (trainer components, base
  points). The economy sim found the strategic AI trains nothing without trainer components.

**Ice (`UFrozenLakeIceSubsystem`; today class-based breaking only, DebugGame build only)**
- [ ] Build and test in Development.
- [ ] Bunching (3+ medium-or-heavier within 15 m on grey ice counts as heavy) and the 20 s stand-still stress, with a
  crack decal and a creak sound.
- [ ] Pump flood: a 10 s cyan warning, then a 5 m/s melt front. A destructible pump Blueprint starts it on death.
  Holes in a flooded zone never refreeze.
- [ ] Shell holes from HE of 75 mm and up, sized by calibre, refreezing in 90 s while the zone's pump runs.
  Sink holes use the same refreeze.
- [ ] Burning wrecks melt through.
- [ ] Vein thaw: a Blueprint-callable `StartVeinThaw` (black ice along the vein over 3 min).
- [ ] **Rig roads** (decision 24): the heavy ice road's zone 2 leg, the rig's west spur and the Forge ice road are
  exempt from floods and the thaw. Their holes refreeze in 90 s. The rig's west spur needs painting as heavy ice in the sketch,
  `fl_ice_gen.py` and the map.
- [ ] A write API for lane preparation and cryo bridges/refreeze: thicken a strip or disc in the state mask, the hole
  collision and the nav boxes.
- [ ] Nav: the weak-ice meta areas become high-cost for player agents. Give the AI a query filter that treats them as
  Null. Raise `DefaultMaxSearchNodes` or otherwise fix the partial heavy HQ -> rig path.
- [ ] A forced route for the ThinIce tank (a scripted mover or a direct move without pathing).
- [ ] Zone 2 flood sinks the frozen King Tiger (decision 25): the melt front reaching its collar destroys the
  scavengeable object and fails FreeKingTiger.

**Units and abilities**
- [ ] Cryo-troopers: a `Squad_Rus_HazmatEngineers`-based subtype with a cryo-suit material and the refreeze /
  ice-bridge ability.
- [ ] "Prepare ice lane": a German engineer ability granted by ThinIce on this map only (about 20 s per 10 m, a 10 m
  white strip).
- [ ] Core carrier: a Bergepanther-style turretless Panther with a core cradle, on the HeavyTank nav agent and the heavy
  ice class.
- [ ] Repairable south bridge: blown B swaps to repaired B through engineer repair; 150 mm and up, or bombs, drop it
  again; nav update.

**Content**
- [ ] The "Thin Ice" field-manual takeover with a recorded clip.
- [ ] Voice lines: announcer, Scavenger, Forge radio (hazmat portrait, broken transmitter).
- [ ] Steam Niagara on open water; amber vein glow in flooded zones.
- [ ] The Svarog crater rim and the dry lakebed beyond the dam on the horizon skirt.
- [ ] Shore metal nodes in place of the scrap yard / wreck-bay metal. Wrecks become cover only.
- [ ] Zone 4 Radixite upwelling (decision 25): a north-east vein branch and a crystal heave (C05 style) in the sketch,
  `fl_ice_gen.py` and the map.
- [ ] Four repair-crew air drops, one linked to each pump (`StartWhenDestroyed`), with cryo-troopers and an escort.
- [ ] Road defence (decision 26): a Shield strongpoint by the rig spur and a Forge battery pre-sighted on the road.
- [ ] The core tech card in the card system (its effect is still to be chosen).
- [ ] Meshes not delivered: the Forge dam, road kit, Shield HQ, airfield props and radio mast, wreck hulls.
- [ ] Owner look picks still pending (they need images): pump, rig, convoy and ice-kit variants (only Gorge Bridge B
  is picked).

## Enemy

- **Director of Forge** runs the site: cryo-troopers, hazmat crews, pump stations, the drill rig.
- **Director of Shield** provides the guard force and the heavy response.
- **Enemy AI start (2026-11-16):** the enemy AI build system runs this mission. The Shield HQ on the far shore is the
  AI's start location: a flat yard (about 56-62 m build radius in the sketches) with its own Radixite and metal, from
  which the AI builds out over the mission. The sketches mark it `ai_start` on the Shield `enemy_base`.
- Cryo-troopers did not exist (checked 2026-11-22). Decided (9): a reskinned `Squad_Rus_HazmatEngineers` subtype
  with a refreeze / ice-bridge ability.
- The enemy AI must respect the ice too. If AI heavies drive onto black ice and sink by themselves, it looks stupid and
  makes the mission too easy. Decided 2026-11-22 (7): the per-class nav areas become high-cost for the player, and the AI gets a query filter
  that treats them as Null.

## Possible hooks in existing systems (unverified)

These are likely starting points, not checked implementations:

- **Weight classes:** crushable rocks (by medium/heavy) and Berlin's heavy-only breaches suggest a unit weight-class
  concept already exists. Ice needs a weight value per vehicle, not just a class, for the load rule.
- **Per-vehicle-class navmeshes** (mentioned in the Pripyat doc) and runtime nav area overrides (used for the Berlin wall
  breaches; they need a nav update) for holes and thin ice.
- **Landscape impact decals** (shell craters > 15 mm) as the trigger for ice holes.
- **Render-target ground maps** (tank track ruts in DryCastle) as a model for an ice thickness map read by the material.
- **Material parameter collections** (unexplored fog reveal, Radixite blight) for the thaw state.
- **Vehicle fire / engine fire death** for burning wrecks melting through.
- **AConvoy** for Forge ice-road convoys.
- **Scavenger salvage** for wreck harvesting.

## Risks

- **AI pathing on changing ice** is the biggest technical risk (see Enemy).
- **Navmesh updates** for many dynamic holes could be costly. A cell grid with a few states may be cheaper than free-form
  holes.
- **Readability:** the thickness colours have to work under fog of war, at night/dark-sky lighting, and with snow on top.
- **Too punishing:** losing a heavy to the ice must feel like the player's mistake, never a surprise. Warnings (cyan
  glow, cracks, a voice line) before anything sinks.

## Map (layout A picked 2026-11-16, built 2026-11-17)

### What the mechanics need from the map

- A **short route over the ice** and **longer routes around it on land**, so crossing the ice is a choice.
- **Three or four pump zones**, each with a pump station the player can reach, so breaking one is a real decision.
- The **frost edge**: snow only inside pump range, scorched brown ground at the map edges.
- **Clear black ice over the vein** near the player's start, for the reveal shot.
- A **wreck graveyard** (1946-47) in a shallow bay: cover, salvage, storytelling.
- **Pressure ridges** on the ice as the only cover lines for infantry.
- Size: 586 x 586 m (decided with layout A).

### Layout: the Drill Rig, variant A "round lake, four pumps" (picked 2026-11-16)

Concept picked: **the Drill Rig** (the Forge rig stands on the ice in the middle of the lake; pumps ring the shore).
Three sketches were made (A round lake with four pumps, B peninsula, C skerries); the owner picked **A** and the other
two were deleted.

Sketch facts (v1):

- Map **586 x 586 m** (15% larger, linear, than Ger_MarderStugMission at 510 m). Small on purpose: lots of action, the
  objective is always obvious.
- A broadly round lake fills the centre (about 39% of the map). The rig stands on a steel platform over the vein in the
  middle. The black-ice vein runs from the rig straight at the player's shore (the reveal), so the core haul home must
  pick zone 1 (open ice, direct) or zone 2 (pressure ridges, covered).
- **Player start** on the south-west shore. **Enemy AI start** (Shield HQ, builds out) on the north-east shore, with
  its own Radixite and metal.
- **Four pumps** at uneven bearings (185, 275, 340, 82 degrees), one zone each:

| Zone | Character | Enemy approach | Player stake |
|---|---|---|---|
| 1 (S) | wide open ice | S column -> rig; E column -> player | direct core haul, the S column's only route to you (no salvage since decision 16) |
| 2 (W) | pressure-ridge field | N column -> player | the ridge field's cover; the frozen King Tiger sinks if zone 2 floods before it is freed (decision 25) |
| 3 (N) | frozen spillway through the Forge dam | N column -> rig (its only way in) | the rich Radixite upwelling, heavy road's vein crossing |
| 4 (E) | wreck bay (1946-47 wrecks) | E column + the AI's Forge ice road -> rig | the second Radixite upwelling (decision 25), the heavy road's AI-end leg, wreck cover (no metal, decision 16; the Forge ice road never floods, decision 24) |

Updated 2026-11-17 for the heavy ice road (see "Mission setup"); the Tiger moved out of zone 4. Stakes updated
2026-11-22 for decisions 16 and 24.

### Zones 3 and 4 rework (v2, 2026-11-16)

v1's zones 3 and 4 gave the player almost nothing to lose, so flooding them was nearly free. v2 gives each one a stake
the player wants to keep:

- **Zone 3: the Forge dam and the Radixite upwelling.**
  - A Forge dam (embankment) holds the north shore. The frozen spillway is its only gap, so the N column comes down
    the spillway into zone 3. Pump 3 sits inside the dam, so it can only be reached from the ice or the spillway.
  - A vein branch runs north-west from the rig to a **Radixite upwelling**: crystals pushing up through the ice. It is
    the richest Radixite on the map. The base seep on the player's shore is the only other nearby Radixite.
  - **Flooding zone 3** stops the N column reaching the rig, but sinks the upwelling, and the N column then turns onto
    the west shore road and drives into the player's base.
- **Zone 4: the wreck bay is the player's metal.**
  - ~~The **wreck graveyard is the main metal** on the map. The scrap yard by the player's base is small and runs out.~~
    Replaced 2026-11-22 (decision 16): metal comes from shore nodes; the wrecks are cover only.
  - ~~A frozen Tiger II in the bay~~ (replaced 2026-11-17 by the frozen King Tiger by the landing shore, freed by
    scavengers; see "Mission setup").
  - After the rig falls, the **Forge ice road** (rig -> AI HQ) through zone 4 is the player's fastest attack route on
    the AI base.
  - **Flooding zone 4** ~~cuts the AI's counter-attack on the rig along the ice road, but sinks the metal and the attack
    road,~~ and cuts the heavy ice road at the AI's end. Since 2026-11-22 the Forge ice road never floods (decision 24)
    and there is no wreck metal (decision 16).
- v1's long fallback haul lanes through zones 3 and 4 were removed. The core goes home through zone 1 or zone 2.

### South column fix (v3, 2026-11-16)

In v2 the S column drove along the south shore into the player's base in about 65 s without touching the ice.

- **Pump 1's outflow gorge.** Pump 1's warm outflow cuts a steaming gorge from the lake to the south edge, and keeps an
  **open-water lead** about 100 m out into zone 1. The south road's bridge over the gorge is **blown**, and a Shield
  picket holds the far side.
- The S column now has to go round the lead tip and cross about 200 m of zone 1 ice to reach the player's shore:
  469 m, about 94 s at the tool's speed (was 325 m, 65 s).
- **Flooding zone 1** now also closes the S column's only way to the player's base; it then has to go the long way
  round by land. This adds weight to the zone 1 decision, which already costs the direct core haul and the ice salvage.
- **Player engineers can repair the blown bridge** (decided 2026-11-17). Repaired, it is a land route for the player
  but also for the S column (see "Decisions and recommendations").

- Roads: west and north (player -> canal bridge -> Shield HQ) and south (player -> S edge). The east shore has only a
  dead-end track, so the road is not a ring.
- Distances (route): player start -> rig 309 m, rig -> Shield HQ 294 m, each enemy entry about 300 m from the rig,
  Shield HQ -> player start 616 m.

Known weak points still open after v2:

- Every entry is about 300 m from the rig: readable but uniform.
