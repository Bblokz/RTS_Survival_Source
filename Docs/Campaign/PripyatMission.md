# Pripyat Marshes mission (working title)

Status: early design, 2026-11-12. No layout sketch yet. Later in the campaign than Berlin, further east.

## One-line pitch

Drive into the Pripyat marshes, break the Soviet AA, take a small Soviet base in the centre of the map and turn it into
our own, repair its airstrip, and use the ME410 to break the armored columns that converge on the base from every
causeway. Then fly out and destroy a second Soviet base that no ground unit can reach.

## Place in the campaign

- Later than Berlin, about 1,000 km further east (Polesie, southern Belarus).
- Unlocks the **ME410**.
- Still Act 1 territory (before the Svarog Line and the Ural Shield). The Phytophage turned forests into toxic,
  self-combusting peat bogs and choked rivers with purple algae, so the bog is the Phytophage's own work, not a nuke
  crater. This is why the Soviets never built proper roads there.

## Setting and geography

- A marsh with a few dry rises. Armor can only cross it on **causeways, rail embankments and gravel roads**.
- The Soviet base we take is in the **centre** of the map, so columns can arrive from several sides (two or three
  causeways, possibly a rail embankment).
- The base stands on a dry ridge with a **ruined pre-war airfield**.
- A second Soviet base sits on a **peat island or sand ridge in the bog**, with no ground connection. It is left out of
  the ground navmesh (the project has per-vehicle-class navmeshes). Air only.
- Soviet AA batteries stand on dry rises along the approach causeways.

## Mission flow

1. **Drive in and clear the AA.** The player drives up to the base destroying the outer defences and the AA batteries.
   The AA gates everything after it: the reinforcements and aircraft can only arrive once the air is safe.
2. **Seize the base.** Destroy the garrison and command post, then deploy our own HQ there (nomadic deploy).
3. **Repair the airstrip.** The ruined airfield must be repaired before aircraft can land. The player holds the base
   while engineers work. This is the first real pressure, and it opens the ME410.
4. **First sortie.** A small column arrives and the player is told to use the ME410s on it. Teaches the unit before the
   waves get serious.
5. **Hold the centre.** Armored columns arrive along the causeways from multiple directions, each wave bigger than the
   last. A spotter post or radar shows direction and arrival time, so the player has to decide where to send the ME410s.
   This is the core of the mission.
6. **Cut the causeways (tactic, not a forced step).** Destroying the lead vehicles of a column leaves wrecks that block
   a narrow embankment. The player can seal a direction and keep only one or two lanes open.
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
- Dependency: Soviet fighters must exist as a unit. Not checked.

## The airstrip (mission-specific)

- Normally aircraft are only trained from the dedicated air building. In this mission the **captured ruined airstrip
  replaces it**: it is a separate production site for aircraft.
- **ME410s are replaceable**: trained at the airstrip like from a regular air building, at normal cost. Losses are not
  permanent, so the player can spend aircraft on the strike and the defence.
- Until the airstrip is repaired it cannot train or land aircraft.
- Implementation question: does an existing air building class work as the airstrip's production component, or does
  the airstrip need its own actor? (Not checked yet.)

## Finale: ready when you are (decided)

There is no timer on the air-only base. The player strikes when they choose. Waiting is still not free, because the
waves keep escalating. No special finale column.

## Resources and buildable ground

The player must build up a real base here, so the map needs space and income. Marsh is not buildable, so the dry
ground decides everything.

- The central ridge must be big enough for the HQ, the airstrip, production, pits and the resource buildings.
- **Radixite and metal are both inside the base area, close to each other** (decided), because both are needed early.
  Neither needs a causeway trip at the start.
- Open: where Radixite comes from in the marsh without a nuke (a deposit at the ridge's bog edge, glowing purple;
  or an old crash site / firebreak scar as the reason). Not decided.
- Open: what the metal is (scrap yard, old rail yard, wrecks).
- Optional later: a bigger second Radixite deposit far out (near the air-only island or at the end of a causeway) as a
  reason to push out. Not decided.

## Optional objectives

- **Downed pilots (rescue for bonus aircraft).** Allied-side or German pilots who went down in the marsh. Rescuing a
  pilot gives bonus aircraft on top of normal production. Where they are decides the risk:
  - some on dry rises near a causeway (reachable by ground units, in the line of the columns),
  - some deep in the bog or near the air-only base (reachable only by a helicopter or by other means, to be decided).
  - Open: what a rescue gives (a free ME410 delivered to the airstrip, or a production discount), and how many.
- **Fuel depot.** Capture one so ME410 sorties do not run dry. (Only relevant if the airstrip uses fuel.)
- **Demeter pod crash site.** In the bog, reachable by air or on foot along one causeway. Lore intel.

## Armored columns

- Each column is a long line on a causeway, so it is the ideal ME410 target.
- **The first few columns are already hard**: mainly T-34s with some heavy IS tanks. The player should feel they
  cannot hold the base without the ME410s. This is decided, so the ME410 sorties are not optional.
- Waves keep escalating in size and unit class after that.
- Columns can approach from different directions at the same time, so the player splits the air force.

## Open questions

- Resource placement (Radixite, metal) and the size of the buildable ground: see the section below. Not decided.
- How rescued pilots arrive and how many aircraft they add.
- Does the player need to keep the airstrip alive (can it be bombed), and is it a loss condition?
- How many causeways, and can all be sealed? (Sealing every one would end the waves, so one should always stay open.)
- Bunker and AA contents of the air-only base.
- Does the map get a layout sketch with the MapDesigner tool (the Berlin workflow)? Not started.
