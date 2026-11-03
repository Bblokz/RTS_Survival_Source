# 05 — Map vision: SM_Desert_01

Status: **draft** (2026-10-31). Owner decisions since: the five starts slide inward to about r 40 000 (islands unchanged apart from
small trims) and the map is cropped to roughly 136 000 cm a side, so the frontier zone below becomes a thin dune strip; the oasis stays exactly as
it is; points of interest (section 4 and 6) are built by a separate POI agent with runtime PCG, plus VAT animals and birds.
This doc gets revised once the rebuilt map is saved. The coordinator wrote it; nothing here is built yet. The terrain
worker takes it in from its round 5. Items marked *code* need work outside the terrain worker's ownership and wait for the
owner's go-ahead.

## 1. What the map is about

Survival is one player holding one bench while Soviet waves come in from the map edges and four Soviet bases stand on the
other benches. Today the map answers "where do I build" well (five benches, gates, ridge platforms) and "where do they come
from" not at all: between the benches is 1.5 km of even orange sand. The player sees attacks arrive from nowhere in particular,
and has no reason to look at the middle of the map except the oasis.

The vision in one line: **a dried inland basin with five remnant terraces, crossed by a few readable desert routes that
the waves follow and the player can fight over.**

- **Story the ground tells:** a salt basin that was once a lake. The five benches are remnant terraces of its old shore,
  the central mesa was an island, the salt pan and the oasis are what is left of the water. Washes drain from the dune
  country at the edges down to the basin floor. A Soviet offensive died here: wrecks lie along the routes, a power line
  still crosses towards a station off the map.
- **What the player should feel:** the base is high, safe ground; the basin floor is open and dangerous; the edges are
  hostile country where trouble starts. Leaving the bench should be a decision with a reason.

## 2. Three zones the player reads at a glance

| Zone | Where | Look | Play |
|---|---|---|---|
| **Home bench** | each start out to ~15 000 | Flat caprock pad, grey top, bright talus, gates cut into the rim | Build, defend the gates and ridge platforms (done in rounds 1-4) |
| **Basin floor** | the ground between benches, mesa, salt pan, oasis | Lower, paler, worn: wash beds, salt crust, wreck fields, tracks | Contested: routes cross it, forward defence, landmarks, later off-base rewards |
| **Frontier** | outer ring, ~55 000 to the edge | Dune sea, taller and darker dunes, haze, little detail | Where waves enter; nothing to build, nothing to hold |

The minimap should show these three as three tones, with the benches as the clearest shapes on it.

## 3. Routes: give the waves visible roads

Waves enter on 9 edge stretches and drive to the player. Make the paths they take part of the map:

- **Two to three desert routes per start**, each a broad, shallow wash or dune corridor from the frontier down to the basin
  floor and on to a bench gate. Readable from above as a worn, paler track with tyre marks; readable at RTS camera as a
  shallow channel. Not walls: a route must never be the only way (no blocking, every open ground stays drivable for HeavyTank).
- **One choke per route, in the 15 000-30 000 band from a start**: a narrowing between a butte and a dune, a ford across a
  wash, a gap between remnant rocks. That is where a player who wants to defend forward builds. A choke is 25-40 m wide (four
  to six heavies abreast), with ground to one side the attacker can use to flank it.
- **Routes meet the gates.** Each gate ramp's foot opens onto a route, so the gate is where the route was going anyway.
- **Fairness:** every start gets the same number of routes and chokes and comparable distances to its nearest edge stretches.
  Distinct in look, equal in function.
- Check: the existing nav search-node limit makes paths over ~90 000 partial; routes help nothing there unless the wave
  director uses waypoints (*code*, later).

## 4. Landmarks: one per sector between starts

Owner: the POI track (runtime PCG, `60_PointsOfInterest.md`), one landmark per sector between neighbouring starts.
The five buttes at r 31 000 sit between neighbouring starts (positions change with the rebuild). Give each a different identity so the player can say
"they are coming past the airfield":

| Butte (between) | Proposal | Assets to look at first |
|---|---|---|
| S0-S1 | **Crash site**: a downed transport or bomber in a scar of churned sand | aircraft crash wrecks used by the AirCrash shorts stage, crater decals |
| S1-S2 | **Dead convoy**: a column of burnt trucks and cars along a route, half buried | burnt old-car wrecks, tank wrecks, track trails |
| S2-S3 | **Pump station**: a ruined brick pump house and tank on the dry well line | building ruins, barrels, the radixite blight around crystals |
| S3-S4 | **Tank graveyard**: knocked-out armour from the failed offensive, turrets thrown clear | wreck turrets, crater decals |
| S4-S0 | **Relay station**: where the power line turns, a fenced transformer yard and mast | the existing pylons, fence and wall pieces |

Rules: each landmark is a cluster of 30-60 m with rest ground around it; it is cover and an obstacle, never a wall across a
route; nothing with collision inside a start's 15 000 or on a ramp; it reads from the minimap as a distinct mark.

The **central mesa** stays the neutral high ground; the **oasis** stays the one living place on the map. Both are
already good; keep them.

## 5. Look and light

- **Colour by height**, map-wide: grey caprock tops, bright talus, orange basin sand, cream salt, teal water. No single
  thing brighter than the salt pan on the minimap (the oasis is too bright there today).
- **Detail where the eye goes**: gates, route chokes, landmarks, the oasis. Open sand stays calm; calm is a rest area,
  not emptiness, once routes cross it.
- **Movement**: the ambient dust devils (one GPU system, baked sand masks) belong on the basin floor and the frontier,
  never on benches. Wind-blown sand on the dune crests if cheap.
- **Edges**: the player must never see void past the landscape. Frontier dunes rise and haze thickens towards the edge.
- **Budget**: the whole terrain and dressing stays inside what the earlier rounds measured; every landmark is checked in
  the same perf view.

## 6. Later, needs code (not for the terrain rounds)

- *code* **Off-base rewards in the basin**: a salvage wreck or a radixite field near each landmark, so going out pays.
  This touches start content and resource code.
- *code* **Wave routing**: waves choose a route and follow its waypoints, which also fixes partial paths on long drives.
- *code* **Landmark names** on the minimap or in wave announcements ("enemy column at the Dead Convoy").

## 7. For the terrain worker, rounds 5-7 (in order)

1. Routes and chokes (section 3): shape and paint them in the basin floor and frontier only. Nothing changes inside 15 000 of a
   start, so the field audit stays valid; rebuild nav and repeat the edge-to-gate HeavyTank paths.
2. Landmarks (section 4), starting with the two that reuse the most existing assets.
3. Frontier and edge treatment, minimap tones (section 5).
4. Each round, add one map-wide view with the routes drawn on it, and one RTS-camera view of each new choke and landmark.
