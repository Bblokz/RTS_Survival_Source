# Berlin mission: mission logic and flow

Status: skeleton, 2026-11-22, written from the owner's answers to the first flow brainstorm. Nothing is scripted yet;
`Ger_Berlin` holds only the template MissionManager. Background (lore, layout, lanes, the 2026-11-18 decisions) is in
[BerlinMission.md](BerlinMission.md); the built map is described in [BerlinMap.md](BerlinMap.md). Every item marked
**(open)** still needs the owner's decision. Numbers are starting values to tune; response-force numbers are the owner's
and are set with nodes in the mission Blueprint.

Objective plates use the existing plate UI: **MAJOR** = primary objective, **BONUS** = secondary objective.

## 1. The mission in one paragraph

The player sets up a base in the quiet Friedrichshain ruins on the east edge and moves west through three lanes to the
Zoo Leitturm, where a repaired Forge relay holds the buffered survey data from the Berlin crater. As long as the enemy
has not raised the alarm, the player can pick off patrols, take bonus targets and prepare. Every enemy unit that gets
away to the Shield base in the Reichstag, and every player unit (harvesters included) seen in a detection zone, raises
the alarm. Holding the relay bunker to pull the data always raises it. From the alarm on, a response force comes from
the east by rail and by road, straight at the player's base. The finale is a race: hold the bunker until the data is
out before the base falls.

## 2. Decisions (owner, 2026-11-22)

1. **Race against the base.** The response force arrives from the east, where the player's base is. The finale is a
   race between holding the relay bunker in the west and defending the HQ in the east. Losing the HQ loses the mission.
2. **Harvesters count for detection.** Any player unit inside a detection zone, harvesters and builders included, can
   raise the alarm. Expanding to the lane fields (E2, E5, E6) is a stealth decision.
3. **Retreating enemies raise the alarm** (section 4.2). Enemy squads and patrol infantry that lose a fight fall back
   to the Shield base at the Reichstag. One that gets there raises the alarm: the player is spotted and stealth is over.
4. **The alarm is avoidable until the finale.** A player who kills enemies before they get away and stays out of the
   detection zones keeps the Soviets unaware. Holding the relay bunker always raises the alarm (fork C2).
5. **The Forge survey station is a bonus**, not part of the main path. It has no effect on the waves at the Zoo. It
   holds unique intel about the crater, and taking it makes HQ fly in reinforcements (composition in a data asset).
6. **The Reichstag Shield base trains units the whole mission.** Killing its factory lowers the number of Soviets on
   the whole map, not only the alarm response.
7. **The win is holding the relay bunker** until the data transfer completes (fork A1).
8. **The Demeter pod stays hidden**: no plate; finding it adds an archive item.

## 3. Pacing at a glance

Target length: **30-40 minutes** on Normal (proposal).

| Phase | Rough time | Player | Enemy | Ends when |
|---|---|---|---|---|
| 0. Arrival | 0:00-1:30 | Location intro, briefing, Leitturm shot | Nothing | Briefing ends |
| 1. Quiet build-up | 1:30-8:00 | Deploys the HQ, harvests, scavenges the power-station wrecks, meets the first patrols | Patrols near the start; Reichstag factory trains | Player leaves the start area |
| 2. Westward | 8:00-25:00 | Chooses lanes and bonus targets; avoids zones, stops runners | Patrols, factory reinforcements, station guards | Player reaches the Leitturm |
| 3. The relay | 25:00-35:00 | Holds the bunker; defends the HQ | Alarm (always): response force on the base, Shield waves on the Zoo | Data out (win) or HQ lost (loss) |

## 4. The rules underneath

### 4.1 Stealth state and the alarm

The mission has one stealth state: **Quiet** until the alarm, then **Alarm** for the rest of the mission (no way back).

What raises the alarm:

| Source | Rule |
|---|---|
| Detection zone | Any player unit (harvesters and builders included) inside a visible detection-zone trigger area. **(open)** instant or after a short dwell time. |
| Fleeing unit reaches the Reichstag | An enemy unit in the Fleeing state (4.2) enters the Reichstag forecourt trigger. |
| Relay bunker hold starts | Always (C2). |

What the alarm does:
- The advisor line for the alarm plays, and the mission timer for the rail arrival appears.
- The **response force** starts: a rail arrival at the rail-yard unloading point (later if the yard was sabotaged, 5.3)
  and a road convoy from the east (`AEnemyConvoyRoute`). Both head for the **player's base**. Size and timing per
  difficulty come from the owner's mission nodes.
- The Reichstag sends out Shield waves toward the player's forces in the west (crater, Gate, Zoo).
- Detection zones are removed (they no longer mean anything).

### 4.2 Fleeing enemies (runners and fall-backs)

Two cases put an enemy unit in the **Fleeing** state. Both send it to the Shield base at the Reichstag.

1. **Patrol runners.** A patrol is a mixed group of vehicles and infantry. When its last vehicle dies, its infantry
   break off and run for the Reichstag.
2. **Losing fights (fall-back).** Any enemy squad in combat with the player for some time that looks like it will lose
   falls back to the Reichstag. Starting rule (all values tunable in the Berlin enemy-forces data asset):
   - in combat with player units for at least **20 s**, **and**
   - the squad has lost at least **half** its members, **or** the player's strength nearby is clearly greater
     (**(open)** how to measure: a strength ratio over a radius, as the enemy AI already estimates for its formations).
   - Hazmats and team weapons follow the same rule. **(open)** whether damaged vehicles also fall back to the Reichstag.
     (The enemy's existing damaged-vehicle retreat sends vehicles under 20 % health back to their own base.)

While Fleeing a unit:
- runs to the Reichstag forecourt by the shortest lane. The forecourt trigger is **by the ruin**, on its south side
  near (8100, -15500) (owner, 2026-11-22). Runners from the crater therefore head north-west.
  **(open)** whether runners scatter over lanes;
- does not stop to fight (**(open)** whether it fires while running);
- is shown on the minimap as a fleeing group heading for the Reichstag (`TrackUnitGroupOnMiniMap`).

When a Fleeing unit reaches the forecourt trigger, the **alarm** is raised (4.1). **(open)** whether there is a short
grace time at the portico, as a last chance to shoot it.

If every Fleeing unit of that group dies first, nothing happens: the enemy stays unaware.

What exists already: `UEnemyRetreatController` (scripted group retreats to a location with a post-retreat strategy) and
the direct-control damaged-vehicle retreat. The losing-fight check, the Reichstag target and the alarm on arrival are
new.

### 4.3 The "Stop the runners" plate

> **Changed 2026-11-22** ([BerlinMissionScripting.md](BerlinMissionScripting.md) 4.5): a mission stage cannot complete
> and re-open. The plate therefore **stays open** with a live count, collapses to "No runners. Stay quiet." at zero and
> **fails on the alarm**. Where the list below says "completes" or "re-opens", read this instead.

- **No plate before the first break.** The player first meets the rule when a unit starts to flee.
- **When it appears:** the first time any enemy unit enters the Fleeing state (a patrol's last vehicle dies, or a squad
  falls back). The advisor line `Berlin_RunnerFleeing` plays; the minimap shows the fleeing group and the Reichstag
  marker flashes.
- **Plate:** BONUS "Stop the runners", description "Enemy soldiers are running to the Reichstag. Kill them before they
  raise the alarm." with a live count of fleeing units left.
- **Completes** when all currently fleeing units are dead (`Berlin_RunnerStopped`). The plate then closes.
- **Re-opens** each time a new unit starts to flee, with the advisor line only on the first few times (**(open)** how
  many) so it does not get repetitive.
- **Fails** when one reaches the Reichstag. That is the alarm (4.1), and the plate is removed for the rest of the
  mission (fleeing no longer matters after the alarm).

### 4.4 The relay bunker hold (win)

- The objective is the radio bunker at the north-east foot of the Zoo flak tower (the Forge relay in the Leitturm).
- **Start:** a player infantry squad enters the bunker trigger area. This always raises the alarm (C2).
- **Progress:** a transfer bar fills while player infantry is inside and no enemy infantry is inside.
  **(open)** whether contested progress pauses or drains, and whether vehicles nearby count.
- **Duration:** set per difficulty (proposal 2-3 minutes on Normal, long enough for the response force to reach the
  base).
- **Win:** the transfer completes. **(open)** whether an escape follows; as written, the data ends the mission.
- **Loss:** the HQ is destroyed (`DontLoseHQ`, running from the start).

### 4.5 Berlin enemy-forces data asset

One data asset per difficulty (owner's decision 2026-11-18) holds the tunable enemy side, so the mission Blueprint
does not have to change for tuning:
- patrols: count, composition, routes;
- fleeing: combat time, losses fraction, strength ratio, grace time at the portico;
- Reichstag factory: what it trains, how often, the population cap, where new units go (4.6);
- Forge station garrison;
- player reinforcement drop for the Forge station bonus (5.2).

Response-force size and timer stay in the mission Blueprint nodes (owner).

### 4.6 The Reichstag factory

The Shield base trains units **the whole mission**, Quiet or Alarm:
- before the alarm the new units join the patrols (they replace killed patrols, up to a cap);
- after the alarm they join the Shield waves.

Killing the factory stops this, which lowers the number of Soviets on the map for the rest of the mission. Raiding the
Reichstag almost certainly raises the alarm, since it lies in its own detection zone: it is a deliberate choice to go
loud early in exchange for a thinner enemy. **(open)** where fleeing units go once the base is destroyed (proposal:
still the Reichstag, as long as its command building stands; with the whole base gone, fleeing no longer raises the
alarm).

## 5. Step by step

### Phase 0: Arrival (0:00-1:30)

1. Location intro "BERLIN" (`UMissionLocationIntroSession`).
2. Advisor: `Berlin_Briefing_Crater` (32.7 s).
3. Camera shot on the Zoo tower under its vision bubble (cinematic take-over, sequence still to be made), with
   `Berlin_Cinematic_Leitturm` (12.5 s).
4. Advisor: `Berlin_Briefing_StayQuiet` (16.2 s).
5. **MAJOR 1: "Set up in the Friedrichshain ruins."** Deploy the HQ. Completes on deployment (**(open)** also a first
   refinery on the start crater).

### Phase 1: Quiet build-up (1:30-8:00)

- The player harvests the start crater and the tram-depot metal field; the power-station wrecks give vehicle parts.
  No plate for these.
- Mixed patrols near the start (data asset). Their first fights teach the fleeing rule and the "Stop the runners" plate
  (4.3). Built 2026-11-24 as the BONUS "Soviet patrols": three patrols marked on the minimap, the plate counts the
  units left ([BerlinMissionScripting.md](BerlinMissionScripting.md) section 8).
- **BONUS: "The cellar people"** (brewery enclave, south-west of the start). Reach the brewery hall and vaults with a
  unit: the Burrowers there show the **patrol routes and detection zones on the minimap** for the rest of the mission.
  Until then the zones are visible only once explored. The Burrower squad is based on the Scavenger squad, with the
  look picked 2026-11-22: A "Kumpel", coal-black miners with cap lamps. Design: [BerlinBurrowers.md](BerlinBurrowers.md). **(open)** whether the Burrowers also give a unit, and whether their reveal shows
  the zones before the player explores them.

### Phase 2: Westward (8:00-25:00)

- **MAJOR 2: "Reach the Leitturm."** The plate points at the Zoo tower vision bubble.
- Bonus targets, one per lane:

#### 5.1 North: "Raid the Reichstag" (BONUS)

Destroy the Shield factory under the dome ribs. Effect: no more trained units (4.6), so fewer Soviets on the map and
fewer Shield waves after the alarm. Almost always loud.

#### 5.2 Central: "The Forge survey station" (BONUS)

- Destroy the Forge survey station at the crater beside the Brandenburg Gate.
- **Reward:**
  - unique intel about the crater (an archive item and an advisor line; story only, a hint that Berlin's Radixite is
    different);
  - HQ flies in reinforcements for the player (composition per difficulty in the data asset), dropped **at the
    station**.
- **No effect** on the waves at the Zoo.
- The station guards follow the fleeing rule (4.2), so attacking it is quiet only if no guard gets away. The garrison
  comes from the Berlin enemy-forces data asset; an `AEnemyStrongpoint` gives the positions.
- **Destroy condition:** 3 counted targets (rig, cryo block, command bunker).
- **Plate:** appears when any player unit enters a trigger ring round the station (about 80-100 m). It plays
  `Berlin_ForgeStationSighted`.
- **Alive layer (picked A + B + C):** the station runs (steam, lights, sound), hazmat crews sample the rim, and a
  cryo-sedation cycle plays at the rig.
- Full design: [BerlinForgeStation.md](BerlinForgeStation.md).

#### 5.3 South: "Sabotage the rail yard" (BONUS)

- Destroy the switch tower and the unloading ramp. Scavengers can do it (sabotage, as the scavenger hint on the
  factory missions teaches).
- Only counts **before the alarm**. Effect: the rail part of the response force arrives later (**(open)** a fixed delay
  or split into smaller groups).
- After the alarm the plate is removed.
- The rail yard is bare now. Its redesign (six sketch versions, wooden track as on the Trains mission) is in its own
  session.

#### 5.4 Hidden: the Demeter pod

No plate. Reaching the crashed pod (south lane) adds an archive item (`AddArchiveItem`) and plays
`Berlin_DemeterPod`.

### Phase 3: The relay (25:00-35:00)

1. A player infantry squad enters the bunker: **alarm** (4.1). Advisor `Berlin_RelayTransferStart`.
2. **MAJOR 3: "Hold the relay bunker until the data is out."** Transfer bar (4.4).
3. At the same moment, the response force heads for the base: **MAJOR (shown with it): "Keep the HQ alive."**
   Rail timer and convoy on the minimap (`ShowReinforcementsOnMiniMap`).
4. Shield waves from the Reichstag (and, if the factory still stands, its new units) attack the bunker.
5. Transfer complete: `Berlin_DataSecured`, victory and debrief.

If the alarm went off earlier (zone or runner), phase 3 works the same way, but the response force is already on its
way, so the player has less time at the base.

## 6. Voice lines

**Exist and are imported** (advisor, `GermanAnnouncer` portrait; SoundWaves in
`Sounds/VoiceLines/GerFaction/Anouncer/AAAGerMissions/Berlin`; subtitles in `DT_CinematicTranscripts`). Nothing plays them
yet.
- `Berlin_Briefing_Crater`
- `Berlin_Cinematic_Leitturm`
- `Berlin_Briefing_StayQuiet`

**Needed** (names are proposals; the owner writes the text):

| Line | When |
|---|---|
| `Berlin_RunnerFleeing` | First fleeing unit (4.3) |
| `Berlin_RunnerStopped` | All fleeing units dead |
| `Berlin_AlarmRaised` | Alarm from a zone or a runner (before the relay) |
| `Berlin_RailArriving` | Rail part of the response force about to arrive |
| `Berlin_RoadConvoy` | Road convoy sighted |
| `Berlin_HQUnderAttack` | Response force reaches the base |
| `Berlin_CellarPeople` | Brewery enclave reached |
| `Berlin_RailYardSabotaged` | Rail yard bonus done |
| `Berlin_ReichstagRaided` | Factory destroyed |
| `Berlin_ForgeStationSighted` | Player enters the station's trigger ring (plate appears) |
| `Berlin_ForgeStationIntel` | Station destroyed: the crater intel |
| `Berlin_ReinforcementsInbound` | HQ reinforcement drop |
| `Berlin_DemeterPod` | Pod found |
| `Berlin_LeitturmReached` | Player reaches the Zoo tower |
| `Berlin_RelayTransferStart` | Hold starts (and the alarm) |
| `Berlin_DataSecured` | Win |

The briefing names only rail reinforcements; the road convoy has to be introduced by `Berlin_AlarmRaised` or
`Berlin_RoadConvoy`.

**Reusable:** the 15 enemy-air warning lines (if the response force uses `AEnemyAirDrop`), and the Russian megaphone
broadcast (`Rus_Bxp_Megaphone_Broadcast`) at the Reichstag base.

## 7. What has to be built

| Piece | Existing building block | New |
|---|---|---|
| Opening | Location intro session, cinematic take-over, `PlayPortrait` | Leitturm camera sequence |
| Stealth state + alarm | Trigger areas, mission nodes | Alarm state, visible detection zones |
| Fleeing | `UEnemyRetreatController`, minimap unit tracking | Losing-fight check, Reichstag target, alarm on arrival |
| Runner plate | BONUS plates | Count and re-open logic |
| Patrols | `AEnemyWaveSpawner` patrols | Berlin enemy-forces data asset |
| Reichstag factory | Enemy training (trainer component) | Continuous training into patrols / waves |
| Response force | Convoy route, wave spawner, mission timer | Rail arrival at the yard; delay from sabotage |
| Forge station | Kill / destructible callbacks, `AddArchiveItem` | Living station, bonus trigger, reinforcement drop |
| Rail yard | Scavenger sabotage | Redesigned yard, wooden track |
| Brewery | Trigger area | Burrower squad, zone reveal |
| Relay hold | Trigger area, progress bar | Contested transfer |
| Win / loss | `DontLoseHQ`, mission debrief | |

## 8. Open questions

1. Detection-zone trigger: instant or after a dwell time?
2. Fleeing: strength measure, whether fleeing units fire, whether damaged vehicles flee to the Reichstag, grace time at
   the portico, lane choice.
3. Where fleeing goes once the Reichstag base is destroyed.
4. Rail-yard delay: fixed minutes or smaller groups.
5. ~~Forge station drop and plate~~: decided 2026-11-22 (5.2). Still open: do the station crews react to an alarm
   raised elsewhere?
6. Relay hold: contested rule, duration per difficulty, escape after the data or not.
7. Brewery: does the Burrower contact give a unit too?
