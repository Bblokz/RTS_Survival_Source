# Ore Mountains mission (design draft, working title)

Status: concept only, nothing built. Started 2026-11-13. This is the **second mission** of the campaign.
Lore reference: `Source/DesignTexts/Lore.txt`. Related: [BerlinMission.md](BerlinMission.md) (the mission that follows).
Every item marked **(open)** is undecided.

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

### Soviet bases (secondary targets)

**Two small bases**, in old Wismut mine and processing sites, as depots or storage that feed the convoys.

- **Base A, at the east end of the valley:** this is where the convoys originate. **Light defences**: MG nests, a couple
  of infantry squads and two light tanks. It is close to the start, so it can be hit early.
- **Base B, on the foothills across the valley from the player's ridge:** it overlooks the road and is **better defended**,
  with AT guns and some infantry on the high ground. It forces a **flanking approach** and uses the elevation tiers.
- **(open)** whether each base thins escorts or removes a convoy source, and the exact defences.

## Mission structure

**(open)** not yet designed beyond the beats above. Likely: hold the ridge, ambush the first lightly escorted convoys,
discover and hit the bases, then escalate to the heavy last convoy and the special cargo.

## Dependencies

- Ore Mountains terrain: ridge, foothills and open valley, with several elevation tiers. **(open)**
- A large Radixite field and metal deposits placed in the map. **(open)**
- Wismut-style mine and processing site assets for the Soviet bases and the player's tunnel base. **(open)**
- Soviet truck convoy units and the convoy movement/spawn logic along a road. **(open)** check what exists.
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
