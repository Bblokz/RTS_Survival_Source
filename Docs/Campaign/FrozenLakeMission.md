# Frozen Lake mission (design draft, working title "Cold Vein")

Status: concept only, nothing built. Started 2026-11-16. Opener of **Act 3**, mid-campaign.
Lore reference: `Source/DesignTexts/Lore.txt`. Related: [BerlinMission.md](BerlinMission.md) (seeds the cryo tech),
[PripyatMission.md](PripyatMission.md) (the mission before, last Act 1 map).
Every item marked **(open)** is undecided.

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
  nearby Svarog firebreak strike. **(open)** whether a Svarog crater is visible on or near the map.
- The Forge keeps the whole lake frozen with **cryo pumps** so it can sedate the vein and mine it through the ice.
- **Without the pumps the lake would not stay a lake.** The vein's heat would thaw it and then steam it away, like every
  other body of water in the region. The lake is alive only because the Forge keeps it cold. **(open)** how far to push
  this in the visuals (steaming open water vs. plain meltwater).
- **The cold is local.** Snow and frost only reach as far as the pumps do. Beyond that, the ground goes back to the dry,
  scorched land of the rest of the game. On the map this reads as a white disc in a brown world.

### The reveal shot

Through clear black ice near the vein, the player sees the root network **glowing and pulsing below**. This is Act 3's
reveal ("Radixite is a living root network") shown, not told. It should be close to the start or on the intro camera
tour, so the player sees it before the fighting starts.

### The thaw is the vein waking up

- Lore.txt (Act 3): crystals glow, pulse and grow toward heat when heavy weapons fire nearby.
- So the fighting itself wakes the vein. **The harder you fight on or near the lake, the faster the ice thins.**
- There is also a slow base thaw over the mission. **(open)** split between the base timer and combat heat.

### The lake is full of the past

Tanks from both sides are frozen into the ice: wrecks from the "massive, mostly unrecorded war" of 1946-47. This is
environmental storytelling for the Act 2 truth (the war that happened across Europe), and it is also salvage (see below).

## Core mechanics

### 1. Load, not just class

- Each patch of ice carries the **total weight of everything standing on it**, not just the heaviest unit.
- Light vehicles are safe. Mediums crack it. Heavies break through.
- **Bunching up breaks it too**, so spacing matters. A blob of mediums can do what one heavy does.
- **Standing still adds stress over time**, and firing from the ice adds more (recoil). Tank destroyers cannot camp on the
  lake; the ice is for moving across, not holding.
- Infantry is effectively weightless on the ice, but exposed (no cover except pressure ridges and frozen wrecks).
- Tuning seed **(open)**: real ice capacity follows roughly `allowed load = A x h^2` (thickness h in cm, A about 3.5 to
  7 kg/cm^2, Gold's rule). With A = 5, a 30 t medium needs about 75 cm and a 57 t Tiger about 105 cm. This gives a
  believable starting point for the thickness bands below. Arcade and Realistic balance may use different numbers.

### 2. Readable thickness

The player plans routes at a glance, and sees the thaw spread:

| Colour | Meaning | Who can cross |
|---|---|---|
| White | Thick | Everything, with spacing for heavies |
| Grey | Thin | Lights, spaced mediums |
| Black | Dangerous | Lights and infantry only |
| Orange glow from below | About to open | Nobody should stop here |
| Open water | Hole | Nobody (vehicles sink) |

**(open)** whether cracks under a unit (stress) show separately from thickness, e.g. a spreading crack decal under a
stopped medium tank.

### 3. Artillery shapes the ground

- Large HE shells punch **open-water holes**. Howitzers can cut a moat in front of an enemy column, or sink an ice road.
- The enemy can do the same to the player.
- Small calibres chip the ice and add stress but do not open holes. **(open)** calibre threshold. The existing landscape
  impact decals already use a > 15 mm cut-off for craters, which may be the natural place to hook this.
- Holes refreeze slowly while their pump zone is running. **(open)** refreeze time.

### 4. Cryo pumps are the main choice

- The lake is split into **pump zones**. Each zone is kept frozen by a Forge pump station.
- **Destroying a pump floods its zone over time.** The player can drown an enemy crossing or seal a flank.
- The cost: the player loses that short route too, and the vein wakes faster there (more heat for the whole lake, but also
  more Radixite exposed to harvest).
- This is the mission's main strategic decision: which zones to keep frozen for yourself, and which to break.
- **Pumps can only be destroyed, never captured** (decided 2026-11-17); a flood is permanent.
- Flood speed (decided): a 10 s warning plus a 40 s melt front (see "Decisions and recommendations").

### 5. Forge cryo-troopers and player lane preparation

- **Forge cryo-troopers** (cryo-suits, as in the lore) refreeze holes and can build **ice bridges** across open water so
  their heavies can cross. Killing the engineers first is the counter.
- **Player counter-tool:** engineers can **prepare a lane** by laying timber or flooding it to thicken the ice (real
  ice-road practice). A prepared lane lets heavies cross.
- Heavies are therefore not banned. **They cost planning**: a prepared lane is slow to build, visible, and a target.
- **(open)** which player unit does this (existing engineers, a new Gandy-Dancer-style unit, or a mission-only ability),
  and whether it carries over as a campaign unlock after this mission.

### 6. Going through

- A vehicle that breaks through **sinks**. The crew bails out onto the ice (survivors as a small infantry group).
- The hull stays visible under the ice as a dark shape.
- **Burning wrecks melt through and sink** too, which removes the cover they gave.
- **(open)** whether a sinking vehicle drags nearby ice with it (a bigger hole).

### 7. Salvage

- Sunken hulls and the frozen 1946-47 wrecks can be salvaged for **metal** (Scavenger theme).
- **Cutting a wreck out of the ice weakens the ice around it.** Greed thins your own routes.
- **(open)** whether metal on this map comes mostly from the wrecks (forces the player onto the ice) or from the shore.

## Mission setup (agreed with the owner, 2026-11-17)

These parts are kept. The full flow below is still a draft around them.

### Early game

1. **Arrival.** A camera tour crosses the lake. It ends on the reveal: the glowing Radixite root network under clear
   black ice, close to the player's shore. The player lands on the south-west shore, on a flat area with a build radius
   of about 66 m.
   - Objective that runs the whole mission: **keep the HQ alive**.
2. **Establish the shore.** First objective: reach the ice edge, which plays the reveal.
   - Starting resources: a Radixite seep on the player's shore and a small scrap yard for metal, which runs out.
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
  - It passes through zones 2, 3 and 4, so **flooding any of those cuts it**. Big shells can hole it.
  - Core haul: the rig's W face is about 40 m of thin ice from the road. A short prepared stub there plus the road's
    zone 2 leg is now the alternative to the zone 1 prepared lane (it replaced v2's separate zone 2 haul lane).
- **Optional: the frozen King Tiger.** A 1946-47 King Tiger is frozen into the shore ice at the head of the heavy
  road, about 95 m from the HQ (112 m by path), in sight from the start. **Scavengers free it with their scavenge
  ability; it takes 180 s.** Then it is the player's. This replaces the earlier Tiger II in the wreck bay.
  - **(open)** whether it is an `AScavengeableObject` with a vehicle reward or a new actor, and whether 180 s is per
    scavenger squad or fixed.
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
keeps thinning from a slow base thaw plus the heat of the fighting.

- **Many of these reinforcements come by Soviet cargo planes** that land on a **landing field behind the Shield HQ**
  (east edge, 90 x 18 m strip, about 45 m from the HQ yard). The planes land, unload Shield heavies and
  cryo-troopers, and fly off; the units then move out by the heavy road, the Forge ice road or the columns' routes.
- Existing system: `AEnemyAirDrop` with delivery **CargoPlane (lands)**. It already has landing points, per-difficulty
  units and a landing order.
- Proposed counterplay **(open)**: an airfield radio mast beside the strip, linked with the air drop's
  **OnlyWhileAlive** rule, so destroying it stops new landings (planes already in the air still deliver). Reaching it
  means a raid deep past the HQ.
- **(open)** how much of the AI's reinforcement comes by air versus the three ground columns, and whether air
  deliveries ramp up after the rig falls.

## Mission flow (draft, all open)

The agreed parts are in "Mission setup" above. The rest is a proposal to react to:

1. **Arrival on the shore.** Intro camera tour across the lake, ending on the clear black ice and the glowing network
   below (the reveal shot). The player sets up on the near shore.
2. **Learn the ice.** Early Forge patrols and light vehicles on the ice. A small heavy crossing goes wrong in front of the
   player (an enemy tank breaks through), which teaches the rule without a tutorial box. **(open)** whether this needs an
   info takeover like the other mechanics.
3. **The pumps.** The player finds the pump stations and the zones they hold. Breaking or keeping them is the main
   decision for the rest of the mission.
4. **The Forge answers.** Shield heavies arrive with cryo-troopers building ice bridges. Artillery duels over the ice.
5. **The objective (open).** Candidates:
   - **A. Take the core (proposal).** Capture the Forge drill rig over the vein and haul a cryo-sedated Radixite core
     back to the shore. The core carrier is **heavy**, so the player has to prepare a lane and keep the right pump zones
     running to bring it home. Ties every mechanic together in the finale, and the sedated core is the Act 3 evidence.
   - B. Destroy the sedation plant.
   - C. Hold a shore base while Forge ice-road convoys cross; sink them instead of shooting them.
6. **Final objective (decided 2026-11-17): destroy the AI's base.** After the core is home, the last objective is to
   destroy the Shield HQ. The sketch chains it after the core haul (`destroy_ai_base`, 537 m from the HQ to the AI
   base by the tool's route). Likely attack routes: the heavy ice road (needs zones 2, 3 and 4 frozen), the Forge ice
   road from the rig (zone 4), or the long west and north shore road over the canal bridge.
   - **(open)** what counts as "destroyed" for a base the AI builds over the mission: only the Shield HQ building, or
     every AI production building. `USecureBaseMission` ("destroy a set of base buildings", with a camera tour) is
     the closest existing class, but it works on buildings placed in the map, not ones the AI builds later.
7. **(open)** what the mission unlocks (lane preparation as a campaign tool, a cryo unit, intel only).

## Decisions and recommendations (2026-11-17)

- **Final objective: destroy the AI's base** (decided). See flow step 6.
- **Pumps can only be destroyed, never captured** (decided). A dead pump cannot be repaired, so every flood is
  permanent. That makes each pump a one-way decision.
- **Player engineers can repair the blown south bridge** (decided). The consequence has to be clear to the player:
  repairing it reopens the south road **both ways**. You get a land route south (to the burnt convoy and pump 1), but
  the S column gets its land route back into your base (about 65 s instead of about 94 s over the ice). Only player
  engineers can repair it; the AI does not, and its cryo-troopers only build ice bridges. **(open)** whether the
  repaired bridge can be blown again (by player demolition or by artillery).
- **Flood speed (decided 2026-11-17): a 10 s warning, then a melt front that takes about 40 s.**
  - **Warning:** the pump explodes, a voice line plays, the ice cracks, and an orange glow spreads under the zone.
  - **Melt front:** the ice turns thin, then black, then open water, moving out from the pump at about 5 m/s.
  - **Why:** that is about vehicle speed. A unit driving away from the pump gets out; a unit driving toward it, or a
    heavy standing still, sinks. You can time a pump kill to catch an enemy column in the middle of the zone, and your
    own units still get a fair chance to leave.
  - Instant flooding would sink units with no warning. A flood over several minutes would never catch anyone.
  - Holes in a flooded zone never refreeze (no pump).
- **The lake (decided 2026-11-17): Lake Vygozero, Karelia**, on the White Sea-Baltic canal (Belomorkanal). A real
  name players may know.
  - Vygozero is about 1,250 km², far too big for four pumps. Suggested framing (needs the owner's OK): the map is **a
    bay of Vygozero that the Forge cut off with its dam**. The pumps keep only the bay frozen. Beyond the dam the rest of
    the lake has steamed away to a cracked, dry lakebed, like every other lake in 1976. That fits the lore ("without
    the pumps the lake would not stay a lake") and the dam and spillway already on the sketch.
  - The Forge dam, the spillway canal and the pump stations can read as the canal's old dams and locks, reused by the
    Forge.
  - The Belomorkanal was a Soviet forced-labour canal. Handle this with care in text and voice lines; it is real
    history.

## Static mesh work

Prompts for delegated modelling, with what can start now and what waits: [FrozenLakeMeshPrompts.md](FrozenLakeMeshPrompts.md).
Work folder: `D:\Blender\BlenderProjects\BlenderTanks\AA_LevelSpecific\FrozenLake` (one subfolder per kit).

## Code TODOs

The sketch uses objective types that have no mission class yet:

- [ ] **Reach area** (`reach_area`): complete when a player unit enters a trigger area. Used by "Establish the shore"
  (the ice-edge trigger that plays the reveal). Probably a small `UMissionBase` child that listens to a `TriggerArea`.
- [ ] **Capture** (`capture`): take an object and hold it. Used by "Capture the Forge drill rig".
  - The frozen King Tiger is also marked `capture` in the sketch, but it is really a **scavenge** objective:
    complete when scavengers finish the 180 s scavenge and the tank changes owner. It may fit the existing scavenging
    (`AScavengeableObject`, `UScavengerComponent`) with a vehicle reward, plus a mission class that tracks it.
- [ ] **Escort** (`escort`): a unit (the heavy core carrier) must reach a destination alive. Used by "Haul the sedated
  core home". Needs: spawning the carrier at the rig (maybe `AsyncSpawnMissionVehicle`), a fail state if it dies or
  sinks, and a completion area at the HQ.
- [ ] **Destroy the AI's base** (`destroy_base`): MapDesigner maps it to `UKillActorsMission`. Check that this works
  for buildings the AI builds during the mission, not only for buildings placed in the map (see flow step 6).

## Enemy

- **Director of Forge** runs the site: cryo-troopers, hazmat crews, pump stations, the drill rig.
- **Director of Shield** provides the guard force and the heavy response.
- **Enemy AI start (2026-11-16):** the enemy AI build system runs this mission. The Shield HQ on the far shore is the
  AI's start location: a flat yard (about 56-62 m build radius in the sketches) with its own Radixite and metal, from
  which the AI builds out over the mission. The sketches mark it `ai_start` on the Shield `enemy_base`.
- **(open)** whether Forge cryo-troopers already exist as a unit or need to be built. Not checked.
- The enemy AI must respect the ice too. If AI heavies drive onto black ice and sink by themselves, it looks stupid and
  makes the mission too easy. AI needs either path costs from ice thickness or scripted lanes. **(open)**

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
- **Too punishing:** losing a heavy to the ice must feel like the player's mistake, never a surprise. Warnings (orange
  glow, cracks, a voice line) before anything sinks.

## Map (brainstorm, nothing decided)

### What the mechanics need from the map

- A **short route over the ice** and **longer routes around it on land**, so crossing the ice is a choice.
- **Three or four pump zones**, each with a pump station the player can reach, so breaking one is a real decision.
- The **frost edge**: snow only inside pump range, scorched brown ground at the map edges.
- **Clear black ice over the vein** near the player's start, for the reveal shot.
- A **wreck graveyard** (1946-47) in a shallow bay: cover, salvage, storytelling.
- **Pressure ridges** on the ice as the only cover lines for infantry.
- Size: similar to Pripyat (700 x 670 m) or the built Ore Mountains map (570 x 570 m). **(open)**

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
| 1 (S) | wide open ice | S column -> rig; E column -> player | direct core haul, ice salvage |
| 2 (W) | pressure-ridge field | N column -> player | heavy ice road's first leg (ridge gap), alt core haul via the road |
| 3 (N) | frozen spillway through the Forge dam | N column -> rig (its only way in) | the rich Radixite upwelling, heavy road's vein crossing |
| 4 (E) | wreck bay (1946-47 wrecks) | E column + the AI's Forge ice road -> rig | main metal, heavy road's last leg into the AI HQ |

Updated 2026-11-17 for the heavy ice road (see "Mission setup"); the Tiger moved out of zone 4.

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
  - The **wreck graveyard is the main metal** on the map. The scrap yard by the player's base is small and runs out.
  - ~~A frozen Tiger II in the bay~~ (replaced 2026-11-17 by the frozen King Tiger by the landing shore, freed by
    scavengers; see "Mission setup").
  - After the rig falls, the **Forge ice road** (rig -> AI HQ) through zone 4 is the player's fastest attack route on
    the AI base.
  - **Flooding zone 4** cuts the AI's counter-attack on the rig along the ice road, but sinks the metal and the attack road,
    and cuts the heavy ice road at the AI's end.
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
