# Pripyat Marshes mission (working title)

Status: layout sketch v10 is the final layout, 2026-11-13. Later in the campaign than Berlin, further east.

## One-line pitch

Drive into the Pripyat marshes, break the four Soviet AA batteries that ring the base hill, take the small Soviet base in
the centre of the map and turn it into our own, repair its airstrip, and use the ME410 to break the armored columns that
converge on the base from three map edges. Then fly out and destroy a second Soviet base that no ground unit can reach.

## Layout sketch (MapDesigner)

- Spec: `MapDesigner/Pripyat/pripyat_01.layout.json` (v10, final). Older versions v1-v9 are kept beside it
  (`pripyat_01_v1.layout.json` ... `pripyat_01_v9.layout.json`, each with its `out_vN` folder).
- Render: `Source/Docs/Campaign/PripyatMission_layout_v10.png`. Report: `PripyatMission_layout_v10_report.md`
  (same folder). Earlier renders v1-v9 are kept there too.
- Object inventory for the builders (ids, positions, real sizes, heights, owners): `MapDesigner/Pripyat/pripyat_v10_objects.md`.
- Regenerate from `MapDesigner/`: `python md.py all Pripyat/pripyat_01.layout.json --out Pripyat/out`.
- The sketch uses the tool features built for this mission: causeway corridors, airstrip footprints, wreck markers and
  elevation (height lines). Missing and not built: flight distance and time, mission phases, per-class navmeshes, a
  general air-only zone, line of sight.

### Size and shape (v10)

- Map **700 x 670 m, 0.469 km2**: 1.05x Ger_FirstMission (627 x 712 m, 0.446 km2, measured from its blocking walls),
  0.90x the Berlin sketch (1000 x 520 m) and 1.80x Ger_MarderStugMission (about 0.26 km2).
  (An earlier version at 1157 x 1057 m was rejected as far too large.)
- **Central hill:** ridge top r 122 m at 10 m height (46,750 m2, 3.13x the compound), a 5 m bench around it and a
  terraced flank down to the bog (foot at r 211 m). Flank steps are impassable except on roads.
- **Compound** (the captured base): circle r 69 m on the ridge top. It overlaps the runway apron by about 50 m, so the
  airfield counts as part of the captured base.
- **Centre airstrip:** 173 x 20 m runway at a 45 degree heading, 12 m apron. All airstrip checks pass.
- **Island** (air only): 3 m high peat island in an algae lake, 81 x 15 m runway, 319 m from the centre airstrip.
- About 15% of the map is open water (algae lakes, oxbows, pools, flooded strips, rivers). Water never touches the
  ridge, ring, runways or aprons.

### The ring road and the four AA sites

- **Ring road:** 14 m wide (hard rule: at least 10 m), 962 m long, on the 5 m bench at 0% grade. At its closest point it
  is **50 m from the compound edge**. It is 42 m from the runway apron.
- **Four AA batteries** on 18 m rises beside the ring, about 90 degrees apart (bearings from the HQ: W 275, N 359,
  E 87, S 181). The player drives the west road to the ring and can clear all four around the hill **without entering
  the enemy compound**: every route from the start to an AA site stays on roads, with 0 m inside the compound and at
  least 48 m from its edge. After AA-W the order is free.
- AA rise edge to compound edge: W 106 m, N 107 m, E 69 m, S 69 m.
- Three **spiral ramps** (about 178 m each, 6-7% grade) wind round the upper step from the ring to the ridge top, so
  crossing the top is never shorter than staying on the ring. The lane-head pits and the west gate bunkers sit at the
  tops of the ramps.
- Start to the west gate via all four AA sites: about 1,380 m.

### Enemy columns and lanes

- **Columns enter from the map edge** along their own road, not out of nowhere: north rail embankment, east sand-ridge
  road, south log causeway. Each spawn is 2 m inside its edge, with the road running off the map and a labelled entry.
- Lane lengths and times (5 m/s tool default / 3.5 m/s assumed column speed):

| Lane | Edge to ring | Edge to HQ | Spotter to ring |
|---|---|---|---|
| North (rail, 14 m) | 310 m: 62 / 89 s | 521 m: 104 / 149 s | 60 / 86 s |
| East (sand ridge, 35 m, ford 30 m) | 323 m: 65 / 92 s | 556 m: 111 / 159 s | 63 / 90 s |
| South (logs, 14 m) | 311 m: 62 / 89 s | 467 m: 93 / 133 s | 61 / 86 s |

- Lane entries are 332-360 m from the centre airstrip by air, so every ME410 sortie to an entry is short.
- **Hairpins** (north and south lanes) are real vehicle turns: centreline radius about 15 m, inner road edge 8.2 m.
- **Sealing:** the north and south lanes have a 10 m choke and a 10 m bridge each, with wreck markers. The east lane
  (35 m road, 30 m ford) cannot be sealed. With every marker blocked, only the east lane stays open, so the waves never
  stop. A column that reaches the ring can drive round it to every ramp and to the west road.
- The west road is the player road (no columns).

### Optional sites on the map

- **Downed pilots:** three on ground lanes (beside the north and south hairpins, and on the east lane about 100 m from
  its entry) and two air-only in the bog.
- **Demeter pod** on a hummock, reached by a 4.5 m infantry-only boardwalk off the west road.
- **Fuel depot** on a rail siding off the north lane.
- **Far Radixite** on a peat dam spur off the south lane.
- Radixite and metal (the aircraft boneyard) are inside the base area, 27 m apart.

## Place in the campaign

- Later than Berlin, about 1,000 km further east (Polesie, southern Belarus).
- Unlocks the **ME410**.
- Still Act 1 territory (before the Svarog Line and the Ural Shield). The Phytophage turned forests into toxic,
  self-combusting peat bogs and choked rivers with purple algae, so the bog is the Phytophage's own work, not a nuke
  crater. This is why the Soviets never built proper roads there.

## Setting and geography

- A marsh with a few dry rises. Armor can only cross it on **causeways, rail embankments and gravel roads**.
- The Soviet base we take is in the **centre** of the map, so columns can arrive from several sides.
- The base stands on a high dry ridge with a **ruined pre-war airfield**.
- A second Soviet base sits on a **peat island in the bog** with its own airfield and no ground connection. It is left
  out of the ground navmesh (the project has per-vehicle-class navmeshes). Air only.
- Soviet AA batteries stand on dry rises around the base hill, 360 degrees (see the ring road above).

## Mission flow

1. **Drive in and clear the AA.** The player drives the west road to the ring and clears the four AA batteries around the
   hill, in any order after AA-W. The AA gates everything after it: the reinforcements and aircraft can only arrive once
   the air is safe.
2. **Seize the base.** Destroy the garrison and command post, then deploy our own HQ there (nomadic deploy).
3. **Repair the airstrip.** The ruined airfield must be repaired before aircraft can land. The player holds the base
   while engineers work. This is the first real pressure, and it opens the ME410. **The repair duration is a mission
   setting.**
4. **First sortie.** A small column arrives (east edge) and the player is told to use the ME410s on it. Teaches the unit
   before the waves get serious.
5. **Hold the centre.** Armored columns arrive from the three edges, each wave bigger than the last. A spotter post or
   radar shows direction and arrival time, so the player has to decide where to send the ME410s. This is the core of
   the mission. **The release time of the first column is a mission setting**, not baked into the map.
6. **Cut the causeways (tactic, not a forced step).** Destroying the lead vehicles of a column leaves wrecks that block
   a narrow embankment. The player can seal the north or south lane and keep one or two lanes open.
7. **Find the air-only base.** It starts hidden. An ME410 recon sortie reveals it. It has light AA, so the player
   picks an approach.
8. **Destroy the air-only base (finale).** No capture, because no ground unit can reach it. Destroy the generators and
   the command bunker. Ready when you are (see below).

## The air-only base has its own airfield (decided)

- The island base has an **airfield**, so the Soviets can send **fighters** to shoot down the player's ME410s.
- This makes the island a real counter-threat rather than a static target. Once the ME410s are flying, enemy fighters
  can show up over the centre base and over the columns.
- **Destroying the island airfield stops the fighters completely** (decided). It is a natural early strike target.
  The player is rewarded for finding the island early, and the fighter pressure ends for good.
- Open: when the fighters start (proposal: after the first ME410 sortie, small patrols that grow), and how many at once.
- Open: whether the player needs their own AA and fighter cover at the centre base to keep the ME410s alive on the
  ground and in the air.
- Dependency: Soviet fighters must exist as a unit. Not found in the project (only Pe-2, Pe-8 and Sturmovik, checked by
  asset names).

## The airstrip (mission-specific)

- Normally aircraft are only trained from the dedicated air building. In this mission the **captured ruined airstrip
  replaces it**: it is a separate production site for aircraft.
- **ME410s are replaceable** (decided): trained at the airstrip like from a regular air building, at normal cost.
  Losses are not permanent, so the player can spend aircraft on the strike and the defence.
- Until the airstrip is repaired it cannot train or land aircraft.
- Implementation question: does an existing air building class work as the airstrip's production component, or does
  the airstrip need its own actor? (Not checked yet.)

## Finale: ready when you are (decided)

There is no timer on the air-only base. The player strikes when they choose. Waiting is still not free, because the
waves keep escalating. No special finale column.

## Resources and buildable ground

The player must build up a real base here, so the map needs space and income. Marsh is not buildable, so the dry
ground decides everything.

- The central ridge top (46,750 m2, 3.13x the compound) holds the HQ, the airstrip, production, pits and the resource
  buildings.
- **Radixite and metal are both inside the base area, close to each other** (decided), because both are needed early.
  Neither needs a causeway trip at the start.
- Open: where Radixite comes from in the marsh without a nuke (a deposit at the ridge's bog edge, glowing purple;
  or an old crash site / firebreak scar as the reason). Not decided.
- Metal is the aircraft boneyard (scrap) on the ridge.
- A bigger second Radixite deposit far out (south dam spur) is a reason to push out.

## Optional objectives

- **Downed pilots (rescue for bonus aircraft).** Rescuing a pilot gives bonus aircraft on top of normal production.
  Where they are decides the risk:
  - three on dry rises near a lane (reachable by ground units, in the line of the columns),
  - two deep in the bog (air only, reachable only by a helicopter or by other means, to be decided).
  - Sealing a bridge also changes how risky a nearby rescue is.
  - Open: what a rescue gives (a free ME410 delivered to the airstrip, or a production discount), and how many.
- **Fuel depot.** Capture one so ME410 sorties do not run dry. (Only relevant if the airstrip uses fuel.)
- **Demeter pod crash site.** In the bog, reachable on foot by the boardwalk. Lore intel.

## Armored columns

- Each column is a long line on a causeway, so it is the ideal ME410 target.
- **The first few columns are already hard**: mainly T-34s with some heavy IS tanks. The player should feel they
  cannot hold the base without the ME410s. This is decided, so the ME410 sorties are not optional.
- Waves keep escalating in size and unit class after that.
- Columns come from three directions at the same time, so the player splits the air force.

## Open questions

- Fog-of-war margins at the edge entries (the tool has no vision radius): check against the real unit sight ranges.
  Nearest holdable spot to an entry: N pilot 149 m, S pilot 156 m, E pilot 100 m.
- AA-E and AA-S are 69 m from the compound edge, so the garrison may join those fights.
- The compound overlaps the runway apron by about 50 m. No base feature is placed in the apron.
- When the fighters start and how many (see above), and whether the player needs AA and fighter cover.
- How rescued pilots arrive and how many aircraft they add.
- Does the player need to keep the airstrip alive (can it be bombed), and is it a loss condition?
- Bunker and AA contents of the air-only base.
- Where Radixite comes from in the marsh (see above).
- Tool wishlist, none built: flight distance/time, mission phases, per-class navmeshes, general air-only zones, line of
  sight.
