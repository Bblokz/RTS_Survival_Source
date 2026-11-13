# Berlin mission (design draft)

Status: concept only, nothing built. Started 2026-11-12. Lore reference: `Source/DesignTexts/Lore.txt`.
Every item marked **(open)** is undecided.

## Layout sketch (MapDesigner)

The current layout sketch (**v6**, 2026-11-12) is the reference for the map's geometry. It is 1000 x 520 m, about 2x
`Ger_MarderStugMission`.

- **Spec (source of truth, tracked):** `MapDesigner/Berlin/berlin_01.layout.json` in the main repo. Older versions are
  `berlin_01_v3.layout.json` to `berlin_01_v5.layout.json` in the same folder.
- **Render (copy kept here):** [BerlinMission_layout_v6.png](BerlinMission_layout_v6.png) and the tool's analysis report
  [BerlinMission_layout_v6_report.md](BerlinMission_layout_v6_report.md).
- **Regenerate** (the tool's `out/` folders are gitignored): from `MapDesigner/`, run
  `python md.py all Berlin/berlin_01.layout.json --out Berlin/out`. See `MapDesigner/README.md` and
  `MapDesigner/AGENTS.md`.
- **Spec conventions** (also in the spec notes): gap width 4 m = infantry only, 6 m = small vehicles, 10 m or more = all
  vehicles; detection zones (`dz_*`) and the Zoo vision bubble (`zoo_vision`) are circles that can be moved on their own.
- **The sketch adds** to this doc: the Spree with a half-collapsed Moltke bridge, a quiet north flank route, and the
  Hauptbahnhof ruins as the north-west exit anchor. These are not described elsewhere in this doc yet.

## One-line pitch

A desperate, stealthy raid into the ruins of Berlin: the resistance has intercepted a Forge survey signal from a
crater everyone believed dead, and has to get in, take what it needs and get out before a Shield response force arrives.

## Place in the campaign

- Early mission. The player starts in East Germany with a small, scraped-together force, so **few Soviets are garrisoned**
  in the area. The danger comes from stealth and a clock (below), not from a big garrison.
- Tone: **desperation.** Everything here should feel like a bad idea that the resistance cannot afford to skip.
- Story function: the player's first look at Forge tech, and the first question the campaign plants about Radixite
  (why does the CSD care about a thirty year old crater?) without answering it.

## Why the resistance goes to Berlin

1. **Trigger (intel hook):** the resistance intercepts signals showing a Forge team surveying the Berlin crater. Why would
   the CSD spend scarce resources on a crater that has been dead for thirty years?
2. **Pull (desperation):** the Kommandant needs what the old Reich left in Berlin (a hardened bunker with a working
   long-range radio and archives; **(open)** exact contents) before the Soviets find it. The radio is the resistance's way
   to tell the scattered German cells they are alive ("they think we are dead").
3. **Reward:** captured Forge survey data that hints Berlin's Radixite behaves differently from what the player has
   seen so far.

### The hidden truth (not told in this mission)

Berlin was nuked in January 1945, before the Phytophage existed (August 1945). It is a nuke-only site: glass and
radiation, no bio-weapon fusion. To Forge scientists it is a **control sample** for understanding Radixite. The player
only sees the odd Soviet interest and a few survey readings. This seeds Act 3 (Radixite is alive) and Act 4 (why nukes
are banned) without spoiling either.

## What the player knows at the start

- Berlin was destroyed in 1945; the player has been told the Soviets did it.
- A Forge team is at the crater. The player does not know why.
- Nothing about the Phytophage origin of Radixite, the Svarog firebreaks or the Demeter pods.

## Setting and geography

Berlin is "completely destroyed" in the lore, so the map is mostly **ruin and glass**. The exception is the **flak towers**
(modelled by other agents). Their thick concrete is the one thing that plausibly survived thirty years and the blast, so
they become the map's landmarks. Berlin took a **single** nuclear strike (January 1945). Operation Svarog is a separate
later event and is not part of this mission's setting.

Real Berlin has three flak tower sites, which gives three natural anchor points. **(open)** The map can be a fictionalised
Berlin and does not have to follow the real layout.

**Decided 2026-11-12:** the map covers the strip **between the Friedrichshain tower (east) and the Zoo tower (west)**,
about 7 km in the real city, and **includes the Brandenburg Gate**. Humboldthain (north) is off the map.

| Anchor | Real location | Role here |
|---|---|---|
| Friedrichshain tower | East edge | Start-area landmark. A **more complete** flak tower than the Zoo tower: still standing and recognisable, weathered, with limited damage. |
| Brandenburg Gate (ruins) | West-centre | Landmark on the way west; the **ruins of the Gate** are on the map, beside the crater. |
| Crater and Forge survey station | Near the Brandenburg Gate | The intel hook and the main mission objective. |
| Zoo tower | West, in the Tiergarten | The objective: bunker, radio and archives. **Very heavily damaged** (walls split open, slabs tilted, interior exposed, rubble around it), but the **radio equipment inside is still somewhat intact**. |

(Humboldthain could still host the Demeter pod if the map is ever widened north; for now it is not on the map. The pod
is placed on the south lane instead.)

**Requirement from the designer:** the Zoo tower must be visible to the player from the start. The game uses an RTS
camera perspective, so this is done with a **vision bubble around the Zoo tower that removes the fog of war**. The tower
is revealed (and the goal is clear) from the first minute, but the player cannot see the area around it. It is not a
line-of-sight or skyline problem: the map does not need an oversized tower or an intro pan for it.

### Where the player starts

- On the **far east edge**, in a quiet, lightly ruined zone below the Friedrichshain tower, away from the crater and
  the Soviet patrol routes, since the mission is about being sneaky. The area should not be crowded: few enemy units,
  plenty of cover.
- The player pushes **east to west** toward the Zoo tower. Why not a central start: it would put the player next to the
  crater and the patrols from the first minute, work against stealth, and lose the build-up of danger with distance.
  (The crater is near the Gate in the west half of the map, so a central start would have put the player in the middle
  of the map and shortened the approach.)

### Routes: a wide band with three lanes

The map is wide enough that the east-to-west progression is not a single corridor. Three rough lanes, each with a
different trade-off:
- **North lane**, **central lane** (through Mitte) and **south lane** (through the Potsdamer Platz area).

| Lane | Risk | Reward |
|---|---|---|
| North | The Shield base in the Reichstag ruins | A route past the base, which can be raided |
| Central | The Forge team at the crater | Direct route to the main objective |
| South | Unstable ground and a garrisoned rail yard | Delay the response force; the Demeter pod |

It is fine if the north lane is **not** the fastest. Lane length is not a balance target; each lane is defined by its risk
and reward.

#### Lane blocks, unit classes and ruins

- **Small vehicles** can use the narrow gaps and infantry routes. **Medium and heavy vehicles** cannot: some routes are
  blocked to them entirely.
- The blocks between lanes are **dense fields of ruins**, with many pieces of rubble and collapsed walls in between, so
  vehicles cannot use them properly. This is the dressing that makes the lane separation work, not just an invisible wall.
- Infantry can cut across between lanes where vehicles cannot.

#### Detection zones (trigger areas)

Detection is made of **trigger areas** that are visible on the map and can be moved at any time. They directly decide
where the enemy first sees the player (and so where patrols and the Shield response start). Moving a zone is the way to
tune stealth, so zones are placed and adjusted during layout, not hard-coded.

#### South lane: unstable ground

Potsdamer Platz sits over a huge network of U-Bahn and S-Bahn tunnels and bunkers. After thirty years and a nuke, parts
of the surface would have collapsed into sinkholes and cave-ins.

- Small vehicles can squeeze through narrow gaps; medium and heavy vehicles cannot pass in some places (see lane blocks
  above). A column gets channelled and exposed, so the player has to scout and commit.
- The risk is terrain and time, not a base: the lane has no big garrison, but it is slow and awkward.
- **(open)** how many passable gaps, and whether collapsed sections can be cleared.

#### South lane: the rail yard

The rail line from the east (Ostbahnhof / Stadtbahn, through Alexanderplatz) brings part of the response force, so the
south lane passes its **unloading and staging point**. The old Anhalter and Potsdamer lines run south from Potsdamer
Platz and are scenery in the south.

- A small staging garrison guards it. This is the lane's enemy presence.
- **Reward:** sabotaging the rail yard before the alarm goes off **delays the response force**, which directly helps the
  main clock. This makes the south lane a planning choice: slower and more awkward, but it buys time later.
- The staging point has to be reachable before the alarm. A candidate is the rail yard just south-west of
  Alexanderplatz, which would also give that otherwise empty part of the map a purpose. **(open)** exact position.
- **(open)** how much the delay is worth (for example a fixed number of minutes, or the force arriving in smaller groups).

The **Demeter pod** can also sit on the south lane, since a discoverable bonus suits the least direct route.

### Crater position

**Decided 2026-11-12: the crater and Forge survey station are near the Brandenburg Gate.** The station sits close to the
Zoo tower, so the two objectives follow in quick succession and the second half of the mission is short and tense.
Alexanderplatz (mid-map) is now free for something else. **(open)** Any earlier note placing the crater mid-map or at
Alexanderplatz is out of date.

### Director of Shield base (Reichstag ruins)

**Decided 2026-11-12:** a small Director of Shield base in the Reichstag ruins, just north of the Gate and the crater.
Patrols leave from it once the player is detected.

- **Close enough to respond.** It is a few hundred metres from the survey station, so patrols reach the crater quickly
  once the player is detected.
- **Easy to avoid.** It sits on the north flank, not on the central route, so a careful player can skirt it.
- **It makes the lanes matter.** The north lane runs past the base, the central lane runs past the crater, and the south
  lane through the Potsdamer Platz area avoids both. Each lane has a clear risk and reward.
- **A strong ruin to build on.** The Reichstag is a recognisable, defensible shell. A small Shield post there, with the
  CSD marking it as theirs, would also be an easy landmark for the player.

**(open)** garrison size and vehicles (small, since this is an early mission), and patrol behaviour (quiet until the
player is detected).

## High-level mission structure (draft)

1. **Approach.** Cross the east ruins quietly. Learn the layout and meet the first patrols and signs of the Forge team.
2. **The Forge survey station** (the main mission). A small outpost at the crater: a few guards, hazmat troopers, sampling
   gear. The first look at cryo-sedation tech. Capture the survey data (and **(open)** possibly some equipment).
3. **The clock.** Once the station is hit or the alarm is raised, a Shield response force starts to arrive (roughly
   10 to 15 minutes). The player cannot hold Berlin and has to finish and leave.
   **The response force arrives from the east, behind the player, in two ways: by rail and as Soviet reinforcements by
   road. Both are implemented.** The CSD retreated east behind the Urals, so reinforcements logically come from there.
   This cuts off the way back, so a plain "retreat where you came from" is not an option and the escape has to go west
   or sideways. It is the main source of the desperation tone. The rail arrival is the part the south lane's rail yard
   sabotage delays.
4. **The Brandenburg Gate ruins and the Zoo tower.** Pass the Gate, reach the bunker, bring the radio online and send
   the broadcast (the radio equipment is somewhat intact, see the table above). **(open)** whether this is the climax or
   the escape beat comes after it.
5. **Escape.** Leave west, or through the map edges in the north-west or south-west (the lanes themselves run east to
   west, so "sideways" means these edge exits) before the response force closes in. **(open)** exit point.

Soviet presence is deliberately small: a thin garrison at the station, a few patrols, then the response force as the
real threat. The enemy's strength is time, not numbers.

## Early patrols and runners (idea, 2026-11-13)

Owner's idea: the map is big, so the opening can be dull. Put **patrols close to the player's start**, each a mixed
group of vehicles and infantry.

- While a patrol's **vehicles** live it behaves as a normal patrol.
- When its vehicles are **destroyed, the infantry break off and run for the Reichstag** (the Shield base).
- If a runner **reaches the Reichstag, the enemy is alerted** (the same alarm the detection zones raise: Shield patrols
  and the response clock).
- If every runner is **killed before that, the Soviets are not alerted.**

This gives the first minutes a small chase: win the fight, then stop the runners.

What the map already gives this (built, see [BerlinMap.md](BerlinMap.md)):

- The Reichstag ruin stands at about (8100, -15500), portico facing south; the open forecourt is at about (800, -15500).
- Candidate patrol areas near the start (13542, 44444): round the ruined electrical station and the approach from
  Alexanderplatz (about 11000, 30000, 150 m from the start), the north lane at about (18000, 30000), and the south lane
  at about (-3000, 34000).
- From those areas to the Reichstag forecourt is about 450-520 m in a straight line, a little more along the lanes: at
  the 300 cm/s the layout tool assumes for infantry that is roughly 2.5 to 3 minutes of running. All lanes route for
  infantry (checked on the navmesh); the wall cores make runners funnel through the infantry and small-vehicle gaps.

**(open)** how many patrols and what is in them; whether runners take the shortest lane or scatter; whether the alert is
immediate on arrival or needs a few seconds at the base (a last chance to shoot them at the portico); whether killing
runners should be shown to the player as an objective or left to be discovered; what a patrol does if the player kills
the infantry first. None of this is scripted: the map holds no units, triggers or waypoints.

## Layout summary

- **East edge:** player start, quiet ruins, the more complete Friedrichshain tower as landmark.
- **West-centre:** the Brandenburg Gate ruins, the crater and the Forge survey station beside them, and the Reichstag
  Shield base just north.
- **West:** the very heavily damaged Zoo tower objective, with its vision bubble clearing the fog of war.
- **Behind the player:** the response force, arriving from the east by rail and by road.

## Hazards and flavour

- Glass and craters from the 1945 blast. Plain ruin, no purple.
- Possible light Phytophage patches and Radixite growth that arrived later. **(open)** whether to keep them out of this
  mission to preserve Berlin as a "clean" nuke-only site (recommended, since it supports the control-sample reveal).
- Optional: heavy weapons fired near young Radixite veins make them pulse and grow (a small early taste of Act 3).
  **(open)**

## Discoverable bonus: the Demeter pod

Not directed at the player. A crashed 30 year old Allied payload pod somewhere off the main route (a candidate is one of
the north or south lanes, since Humboldthain is off the map). Rusty canisters labelled "Project Demeter: Wheat Defoliant - For Agricultural Targets Only", plus Allied
radio logs. This is the player's first clue that the Purple Death was a famine weapon. Players who do not explore will
simply miss it.

## Dependencies

- Flak tower meshes (in progress with other agents): Zoo tower (very heavily damaged variant) and Friedrichshain tower
  (more complete variant) required; Humboldthain is not needed.
- A ruined Brandenburg Gate model.
- A fog-of-war vision bubble around the Zoo tower.
- Ruined Berlin environment kit **(open)**.
- A response-force spawn and timer mechanism (check what the intro mission and survival mode already provide).
- Forge survey station assets: hazmat troopers exist; the survey equipment **(open)**.

## Open questions

1. Is the Kommandant personally tied to Berlin, or newer to command? (Currently: not decided.)
2. What does the player get out of the mission mechanically (unlock, commander or story only)?
3. Fixed-force mission, or does the player get to deploy a base?
4. Exact bunker contents, and how the radio broadcast plays out.
5. Escape route and end condition.
6. Difficulty scaling: response-force size and timer length per difficulty.
