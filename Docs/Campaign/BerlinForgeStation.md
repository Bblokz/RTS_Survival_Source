# Berlin: the Forge survey station (bonus objective)

Status: **built 2026-11-22** (section 10). Owner's picks the same day: alive layer A + B + C (Kulisse + Probennahme
+ Sedierung; no D, no E), 3 counted targets, garrison in the Berlin data asset, drop at the station, forecourt by the
ruin. Still open: section 9 points 5-7 and the three advisor lines (not recorded). The fixed rules come from
[BerlinMissionFlow.md](BerlinMissionFlow.md), sections 4.2 and 5.2:
- the station is a **BONUS** objective and has **no effect on the Zoo waves**;
- the reward is crater intel (an archive item and an advisor line) and an HQ reinforcement drop, with the composition
  set in the Berlin data asset;
- the station guards follow the fleeing rule.

Background: [BerlinMission.md](BerlinMission.md) and [BerlinMap.md](BerlinMap.md). The station mesh and its style:
`SM_Berlin_ForgeStation`, concept C "Torwerk", Director of Forge style (the style note `ForgeStyle.md` is in the Blender
folder `CustomMapItems/Environment/BerlinForgeStation`).

Concept plans: `Docs/Concepts/BerlinForgeStation/v01/` (`ForgeStation_v01_all.png`, one PNG per sheet). The plans are
2D and were drawn from the layout JSON by `Source/Tools/BerlinMap/poi/forge_alive_plans.py`. No editor or GPU was used.

## 1. Summary of the proposal

| Question | Proposal |
|---|---|
| Alive layer | **A + B + C**: the station runs (steam, lights, sound), hazmat crews sample the rim, and a cryo-sedation cycle plays at the rig. **D** (Forge tanker shuttle) is a stretch goal. **E** (searchlight sweep) is not recommended. |
| Garrison | Thin, Forge-coloured, on an `AEnemyStrongpoint` at the station. 2 squads on the easiest difficulty, up to 5 squads and 1 car on Brutal. |
| Bonus trigger | A **trigger ring** around the station, sized from the outermost guard post plus the guards' reach (about 80 m without option B, about 100 m with B's rim stakes). Any player unit entering it shows the plate and plays `Berlin_ForgeStationSighted`. The ring stands in for "first sight", which would need new fog-of-war C++. |
| Destroy condition | **3 counted targets**: the sampling rig, the cryo block and the command bunker. Each is its own enemy building, split out of the station mesh. Guards, crews, vehicles and the rest of the station do not count. |
| Reward | Plate completes → `Berlin_ForgeStationIntel` + archive item → `Berlin_ReinforcementsInbound` → German cargo-plane drop at the station (owner). |

## 2. The site

Numbers from `Saved/Berlin/terrain/Ger_Berlin_layout.json` and the station transform set by
`berlin_map_build.custom_poi_list()`. World frame: +X north, +Y east, centimetres.

- **Pivot** (363, 1382). **Yaw** -120°, so local +X faces the crater centre (-2167, -3000), 50.6 m away. The crater
  radius is about 38 m, so the crater-side buildings stand on the rim.
- **Part centres (world):**

  | Part | Position |
  |---|---|
  | sampling rig | (299, 91), on the rim |
  | cryo block | (1360, 1909) |
  | command bunker | (378, 2447) |
  | crew hut | (-227, 2900) |
  | cryo receivers | (-543, 773) and (-693, 514) |
  | sample bunker | (-1272, 1010) |
  | console and beacon | (8, 1807) |

  The cryo block and the command bunker are on the side the player arrives from. The rig, receivers and sample
  bunker are on the crater side.
- **Lanes:** the central lane comes in from the north-east, from (3250, 5556), and ends at the pivot. It then leaves
  along the rim towards the Gate (0, -8300), which is about 97 m away. The gate arch spans the station's own
  vehicle lane on the Gate side.
- **Patrol tracks:** `patrol_alex` (from Alexanderplatz) and `patrol_shield` (from the Reichstag) both end at the
  station. Some traffic is already part of the layout.
- **Runner geometry (important):**
  - The alarm trigger is the Reichstag forecourt, **by the ruin** (owner, 2026-11-22). Flow 4.2 gives (800, -15500),
    but that point is west of the Gate; the owner confirmed the forecourt belongs next to the ruin, which is centred
    on (8100, -15500) with its portico to the south. **Follow-up:** correct the coordinate in `BerlinMissionFlow.md`
    4.2 once the trigger is placed.
  - Runners go **north-west along `patrol_shield` to the ruin: about 186 m to the centre**, a little less to the
    portico. That is **about 45-60 s** at 3-4 m/s; the infantry run speed still has to be measured.
  - They flee away from a player who arrives from the east. With the flow's 20 s of combat before a squad can fall
    back, a quiet clear has a window of about 65-80 s.
  - **The blocking stretch is the north-west road** (`patrol_shield`) between the station and the Reichstag. It does
    not pass through the Gate. It does lead into the Reichstag's own detection zone, so block it close to the
    station. The trigger line has to say so (section 5).
- **E6 Radixite field** (-8000, -2900) is on the far, south-west rim, about 100 m away. It is not part of the station.

## 3. The "alive" layer

Rule for every element: either it is a **real unit** (it fights, dies, and follows the fleeing rule), or it is
**decoration owned by the station** (it cannot flee, it stops when its building dies, and it reacts to combat as
described in 3.3). Nothing in the alive layer can raise the alarm by itself.

### 3.1 Options (sheets 1-5)

Cost: **low** = Blueprint and assets only; **medium** = a small C++ addition; **medium-high** = a new system.

| | Option | What the player sees | Existing assets | New | Cost | Runner risk |
|---|---|---|---|---|---|---|
| A | **Kulisse** (the station runs) | Steam from the cooling stack, a pulsing beacon on the shard mast, 4 floodlights, the dish on the command bunker turning, the drill string turning and the hung core swaying, frost vapour on the receivers. Sounds: cooling hum, drill loop, radio static, Geiger clicks. A Forge megaphone plays Russian broadcasts. Guards stand on posts. | `NS_RadixiteSmoke1`, `NS_BerlinAtmo_CraterVapour`, `S_ConstructionDrill`, `Crane_Idle`, `TurnOnRadio_*`, `LogoGeiger`, `Rus_Bxp_Megaphone_Broadcast` (Directorate line set), `AEnemyStrongpoint` | `BP_Berlin_ForgeStation` (lights, Niagara, audio); dish and drill string split out of the mesh as two rotating parts | low | low: compact posts |
| B | **Probennahme** (rim sampling) | Hazmat crews walk a circuit of 5 sampling stakes on the rim. At each stake they kneel and work for 20-30 s, then carry the core back to the sample bunker. A Mosin escort follows them. Geiger clicks at the stakes. | `Rus_Squad_HazmatEngineers` (already `MI_SovietHazmat`), `Scavenge_infMontage`, `Rus_Squad_Mosin_Soviet` | **C++:** a work-circuit post option on the strongpoint (play a work montage, then move to the next post). Stake and cryo-canister props (Forge materials). | medium | medium: crews spread about 40 m along the rim |
| C | **Sedierung** (cryo-sedation cycle) | Every 75 s at the rig: the hung sample **pulses brighter** (2 s), four lances **vent frost** (6 s), and the core and the roof samples go **dull and frosted**, then thaw over 60 s. The crews step back during the vent. Sounds: compressor rise and hiss, pump thump. Frosted crystals on the rim near the rig. This is the first look at Forge cryo tech. It hints that Radixite reacts and that the Forge "sedates" it, without saying so (Act 3 seed). | white smoke bursts (`NS_SmokeBurst*`, `NS_ConstructionSmokeRound_*`), `NS_BerlinAtmo_CraterVapour`, `MI_Crystal1` as the parent material, decal kit | A timeline in the station Blueprint; **one new crystal MI** with pulse and frost parameters (never edit `MI_Crystal1`, the resource crystals use it); 2 sounds (hiss, compressor) | low-medium | none added |
| D | **Pendel** (traffic) | A Forge fuel tanker drives in from the Reichstag every 4-5 min. It parks under the arch for 60 s with a hose on the cryo main, then drives back. A parked BA-11 idles with exhaust. Vehicles on the two patrol tracks stop 30 s at the arch and turn back. | `Rus_ArmoredCar_SovietTanker_Forge`, `Rus_ArmoredCar_BA11_Soviet`, `NS_ExhaustTank`, the patrol tracks | A park-and-return shuttle: either a mission Blueprint timer with move orders, or a per-point wait time on `AEnemyConvoyRoute`. A convoy is **not** used as it is: it brings cargo, capture, the "Escaped" outcome and escorts that can become runners. | medium | **high**: the tanker drives to the Reichstag. If it is attacked it flees, and it counts as a runner. |
| E | **Sucher** (sensor sweep) | A searchlight on the rig deck sweeps 160° of the rim in 20 s. Its 6 m ground spot is a detection zone. | spot light, decal kit | A moving detection zone (detection zones themselves are new, flow 4.1); a rotating light rig | medium-high | none, but it is a fourth way to raise the alarm, and it reads poorly in Berlin's daylight |

### 3.2 Recommendation: A + B + C

- **C** does the station's story job: the first look at cryo-sedation, set up for the Frozen Lake mission. It is
  cheap.
- **B** is the visible life the brief asks for (hazmat crews at the rim). The crews are real units, so they also add
  runner risk. That gives the player a reason to scout before firing.
- **A** is the base layer under both.
- **D** is a good stealth puzzle: hit the tanker while it is parked, or between its visits. But it makes a quiet
  clear much harder and needs its own C++. Add it only if the station plays too easy.
- **E** adds an alarm source that the flow does not ask for. It also fights the daylight lighting.

### 3.3 How the layer reacts

- **Combat at the station** (any guard or crew in combat): the work circuit ends and the crews become normal squads
  (fleeing rule). The sedation cycle stops after its current step. The megaphone keeps broadcasting; it is a building
  and has no siren or alarm role here (its `PlaySiren` path stays unused).
- **A counted target dies:** its own decoration dies with it (lights, sound, rotating parts, roof samples). The
  destroyed mesh shows holes and fire, as other Soviet buildings do.
- **The alarm goes off elsewhere (optional):** the floodlights turn red and the crews fall back inside the strongpoint
  ring. This is a consequence of the alarm, not a source of one.

## 4. The garrison

Units in Forge colours: the `_Soviet` squads and the hazmats, which already wear `MI_SovietHazmat`. Posts are placed
on one `AEnemyStrongpoint` (`STRONG_ForgeStation`). Filled posts are used on every difficulty; outlined posts only on
harder ones. Set `bReleaseDugInDefenders` off and do not release defenders to direct control, so they stay at the
station.

| Post | Unit | From |
|---|---|---|
| Gate arch | `Rus_Squad_Mosin_Soviet` | every difficulty |
| Rim crew (work circuit, B) | `Rus_Squad_HazmatEngineers` | every difficulty |
| Console | `Rus_Squad_PTRS_Soviet` | Normal |
| Rig foot | `Rus_Squad_HazmatBazooka` (the station's AT) | Normal |
| Second rim crew | `Rus_Squad_HazmatEngineers` | Hard |
| Crew hut | `Rus_ArmoredCar_BA11_Soviet` (parked, engine idling) | Hard |
| Crew escort | `Rus_Squad_Mosin_Soviet` | Brutal / Ironman |

- The megaphone (`Rus_Bxp_Megaphone_Broadcast`) stands at the crew hut. It is a building and not a guard.
- **Who owns the garrison: the Berlin enemy-forces data asset** (owner, 2026-11-22; as flow 4.5 already says).
  - The data asset lists, per difficulty, which unit stands on which post. It uses named post ids (`Arch`,
    `RimCrewA`, `Console`, `RigFoot`, `RimCrewB`, `CrewHut`, `CrewEscort`) with the unit-picker type
    `FEnemyPlacementUnit`, never raw `FTrainingOption`.
  - The strongpoint `STRONG_ForgeStation` only gives the post positions, their names, and the work-circuit posts.
  - **New C++:** the strongpoint takes its units from a data-asset entry. Either a strongpoint option names the
    asset and entry, or a mission node fills the posts before the strongpoint starts. The table above is the
    starting content of that entry.
  - The reinforcement payload is in the same data asset.
- Runners and fall-back: the guards use the flow's rule unchanged. If one reaches the forecourt, the alarm goes off
  and the "Stop the runners" plate handles it. The station adds no rule of its own.

## 5. When the bonus plate appears

**Recommendation: a trigger ring around the station pivot.** Player units only; it fires once. On entry:
1. the BONUS plate "The Forge survey station" appears;
2. the minimap marks the 3 targets;
3. the advisor plays `Berlin_ForgeStationSighted`.

Reasons:
- **The plate must come before the fight.** The central lane ends at the station. Size the ring as:
  outermost real-unit post + measured guard reach (attack range + `AddedAggroRange` 25 m; about 55 m is an estimate)
  + 10 m margin.
  - Option A + C: posts are up to about 15-17 m out, so the ring is about **80 m**.
  - Option B as drawn: rim stakes are up to 37 m out, so the ring is about **100 m**. It then reaches the Gate, so a
    player who comes from the Gate gets the plate as they pass it.
  - Keeping the stakes within 25 m of the pivot brings the ring down to about 90 m.

  Measure the reach in a headless run before fixing the radius. The ring cannot guarantee the plate comes first
  against units that wander off their posts.
- **Works from every direction:** the central lane, the rim from the Gate, or the north lane round the Reichstag.
- **Existing tools only:** `CreateTriggerAreaSphere` and `BP_OnTriggerAreaCallback`. A real "first sight" check needs
  a fog-of-war callback that does not exist (no vision callback in `MissionBase.h`). A ring sized near vision range
  plays almost the same.
- **Why not the alternatives:**
  - The crater rim is inside the guards' reach, so the plate would arrive mid-fight.
  - A scouting action ties the plate to one unit type.
  - An advisor line after a trigger area is what this proposal already does.
- **The line has a job:**
  1. name the station and the three targets;
  2. warn that the guards will run north-west to the Reichstag;
  3. hint that the player should block that road close to the station first (further on it runs into the
     Reichstag's detection zone).

Edge cases:
- **Targets destroyed before the plate** (for example by artillery from outside the ring): the first target death
  shows the plate. If all three die before it, the plate shows and completes at once, and the reward still plays.
- **After the alarm:** the bonus stays open with the same reward. Runners no longer matter, because the alarm cannot
  go off twice.
- **No detection zone on the station.** The guards are its detection. A zone there would make the bonus loud every
  time. Whoever lays out the zones must keep the station, its ring and the first stretch of the north-west road
  outside the Gate and Reichstag zones. The Gate zone is about 97 m west.
- **Optional (open):** the Burrowers' map reveal from "The cellar people" could also show the plate early.

## 6. The destroy condition

**Proposal: 3 counted targets.**
- `BP_ForgeStation_Rig`: the sampling rig with its tower and plinth. About 6 Pz IV shots.
- `BP_ForgeStation_CryoBlock`: the cryo block with its cooling stack and sedation rack. About 8 shots.
- `BP_ForgeStation_CommandBunker`: the command bunker with its dish and sample cage. About 6 shots.

The shot counts follow the Soviet building shot-health rule (towers 6, research 8). How it works:
- The deterministic generator is split into a **shell** and the three parts. The shell keeps the gate wall, hut,
  receivers, sample bunker, console and lane.
- Each part gets a destroyed state.
- Each part is a Blueprint child of a Soviet building (BXP) class, as `Rus_BXP_RusBunker03_Soviet` is, so it is an
  enemy building that player units target on their own and that shows a health bar.
- The mission tracks the parts with `RegisterCallbackOnBXPDies` and `TrackActorsWithMiniMapProgress`. The plate shows
  "0/3".
- **Verify:** a placed BXP with no owning base works as an enemy target with health. There is no proven precedent: a
  spawned megaphone BXP reported "No valid hp component". The Soviet building shot bench (`-SovietBuildingShotBench`)
  is the place to test it. If it fails, use an `ADestructableEnvActor` child and
  `RegisterCallbackOnDestructibleCollapse`, at the cost of player units not targeting it on their own.

Do **not** count the guards, crews, vehicles, the shell or the megaphone. The player can win the bonus by sabotage
while the guards still stand, at the cost of a loud fight.

**Alternative (cheaper):** one target, the whole station as a single enemy building with one destroyed mesh. The
plate has no count and the target is one big 30 x 30 m health pool. **(open)** which one the owner wants.

Optional flavour: when the cryo block dies, the cryo receivers vent a large frost burst (decoration only, no damage).

## 7. Reward flow and lines

1. **Last target dies** → the plate completes. Stop the station's decoration.
2. **About 1 s later:** `Berlin_ForgeStationIntel` (advisor). At the same time, `AddArchiveItem` adds a new entry
   (proposal: `ERTSArchiveItem::BerlinCraterSurvey`, Lore category). The enum entry and the archive row are new.
3. **About 6-10 s after the line:** `Berlin_ReinforcementsInbound` (advisor). The HQ flies in the reinforcements:
   - the composition per difficulty comes from the Berlin data asset, as tank and squad subtype lists (the shape
     `UGA_AircraftDrop::SetDropPayload` takes);
   - the delivery uses the German drop abilities (`GA_GerParatroopers` for squads; a `GA_Ger*Drop` cargo plane for
     vehicles);
   - **New:** a mission node that fires a player drop with a payload at a location, built the way `AEnemyAirDrop`
     does it for the enemy (`InitGlobalAbility` + `ExecuteAbilityAtLocation`);
   - `ShowReinforcementsOnMiniMap` marks the landing zone.
4. **Drop location: at the station** (owner, 2026-11-22). The player sees the reward where they earned it.
   - Landing zone: the open ground on the central lane about 25 m east of the pivot, around **(1600, 3550)**. It is
     on the player's side of the station and away from the Reichstag. Check it is flat and on the navmesh before
     placing it.
   - **The drop does not raise the alarm.** The alarm sources stay the three in flow 4.1. The plane flies in and
     out over the east, not over the Reichstag.

### Advisor lines

Names are proposals. The owner writes the text; the voice is the `GermanAnnouncer` portrait.

| Line | When | Intent | Status |
|---|---|---|---|
| `Berlin_ForgeStationSighted` | Ring entered (section 5) | Name the station; three targets (the drill rig, the cooling plant, the command bunker); the guards will run north-west to the Reichstag, so block that road first. About 10-15 s. | **new** (add to flow section 6) |
| `Berlin_ForgeStationIntel` | Last target dies | The readings make no sense: the Forge is freezing, "putting to sleep", the crystals of a crater dead for thirty years. Why? A hint only; nothing about the Phytophage. | in flow section 6 |
| `Berlin_ReinforcementsInbound` | Drop starts | HQ sends what it can spare, and where it lands. | in flow section 6 |

### Crew radio lines (built 2026-11-24)

The station's hazmat crew talks on intercepted radio: `SovietHazmat` portrait, ElevenLabs "Stanislav" in Russian
through the "Broken transmitter" chain (`VoiceLines/VoiceChanger/hazmat_lines.py`), English subtitles. Owner's pick:
three lines, each plays once. They sit in the stage's `CrewLines` (`FBerlinForgeStationCrewLines`, C++ class default
`SovietHazmat`).

| Line | When | Text (English subtitle) |
|---|---|---|
| `Berlin_ForgeCrew_Sighted` | The plate appears (before the advisor's sighted line; skipped when the garrison already fights) | "Command post, this is the rim crew. Stake four, core taken. The crystals are asleep, sedation holds. Next cycle in seventy-five seconds. The counter still crackles by the old shell holes. Logging it." |
| `Berlin_ForgeCrew_Engaged` | The strongpoint's `OnGarrisonEngaged` (first fight or first loss) | "Contact at the rim! Germans, here, at the crater! Drop the cores, everyone into the bunker! Guards, keep them off the rig!" |
| `Berlin_ForgeCrew_TargetLost` | The station's `OnTargetDestroyed`, first counted target | "They're hitting the station! Pressure is dropping! If the cold goes, the samples wake up. Command post, do you hear me? Command post!" |

Lore stays a hint: "asleep", "sedation", "the samples wake up"; nothing on why.

The megaphone reuses the existing Directorate line set. **(open)** a small Forge set (Russian, about 6 lines: sample
counts, "sedation cycle in thirty seconds", cryo pressure) using the existing voice treatment B "Across the yard".

## 8. What has to be built (after the pick)

| Piece | Existing building block | New |
|---|---|---|
| Station split + damage | Deterministic Blender generator, the four Forge materials | Shell + 3 target meshes, destroyed states, 3 target Blueprints (Soviet BXP children) |
| Alive dressing (A) | Niagara and sound assets in 3.1, megaphone BXP | `BP_Berlin_ForgeStation` (lights, VFX, audio), 2 rotating parts |
| Sedation cycle (C) | Smoke bursts, crystal parent material | Station timeline, 1 crystal MI, 2 sounds, rim frost decals |
| Rim sampling (B) | Strongpoint posts, hazmat engineers, scavenge montage | **C++** work-circuit post option; stakes and canister props |
| Garrison | `AEnemyStrongpoint` | Placement only |
| Trigger and plate | `CreateTriggerAreaSphere`, BONUS plate, `TrackActorsWithMiniMapProgress` | Mission Blueprint nodes |
| Reward | `AddArchiveItem`, German drop abilities, `ShowReinforcementsOnMiniMap` | Archive enum entry + row; **C++** player drop mission node; payload in the data asset |
| Lines | `PlayPortrait` | `Berlin_ForgeStationSighted` (and the two lines already listed in the flow) |
| Shuttle (D, optional) | Forge tanker, patrol tracks | Park-and-return logic |

### Not verified yet

- `Scavenge_infMontage` plays correctly on the Rus hazmat skeleton (option B's work animation).
- A vehicle can stand on an `AEnemyStrongpoint` post (the parked BA-11).
- The tanker in option D counts as a runner. Flow 4.2 leaves open whether damaged vehicles flee.
- The infantry run speed (the 45-60 s runner time assumes 3-4 m/s).
- The guards' real engagement reach (it sets the trigger ring radius).

## 9. Open questions for the owner

1. ~~Which alive option(s)?~~ **Decided 2026-11-22: A + B + C**, without D or E.
2. ~~Destroy condition?~~ **Decided: 3 counted targets** (rig, cryo block, command bunker).
3. ~~Garrison owner?~~ **Decided: the Berlin enemy-forces data asset**; the strongpoint gives the positions (section 4).
4. ~~Reinforcement drop?~~ **Decided: at the station** (section 7).
5. Do the station crews react to an alarm raised elsewhere (red lights, fall back)?
6. Should a Forge megaphone line set be made?
7. Should the Burrower reveal show the plate early?
8. ~~Forecourt position?~~ **Decided: by the Reichstag ruin**, not at (800, -15500); runners go north-west (section 2).

Follow-ups for `BerlinMissionFlow.md` (not edited here):
- section 4.2: the forecourt coordinate (800, -15500) is wrong; it is by the ruin (8100, -15500), south side;
- section 5.2: the drop lands at the station; the garrison stays in the data asset (4.5 is already right);
- section 6: add the `Berlin_ForgeStationSighted` row.

## 10. What was built (2026-11-22)

Owner's picks: A + B + C, 3 counted targets, garrison in the data asset, drop at the station, forecourt by the ruin.

### C++ (all uncommitted, Development editor DLL rebuilt)

| Piece | Where | What it does |
|---|---|---|
| Strongpoint additions | `Enemy/Strongpoint/EnemyStrongpoint.h/.cpp` | `PostId` per post; `bUnitsSetByMission` + `SetPostUnitOverrides` (a mission picks the units per post, the posts only give positions); a per-post **work circuit** (walk points, work montage, work seconds); `OnGarrisonEngaged` (first fight or first loss, which also stops the circuits); editor shapes for the circuit points. |
| Work circuit runner | `Enemy/Strongpoint/WorkCircuit/EnemyWorkCircuitRunner.h/.cpp` | Walks a squad from point to point and loops the work montage on every soldier; stops on combat, death, release or clear. |
| Player air drop | `Missions/PlayerAirDrop/PlayerAirDropSubsystem.h/.cpp` | `DeliverPlayerUnitsByAir(LandingPoint, FPlayerAirDropPayload)`: copies of the German drop abilities (cargo plane `GA_GerPzIIIFlak41Drop` for vehicles, `GA_GerParatroopers` for squads) fly the payload in for the player; no cost, no cooldown. |
| Berlin forces data | `Missions/Berlin/BerlinMissionForcesData.h` | `UBerlinMissionForcesData`, one asset per difficulty; holds the station garrison (per `PostId`) and the HQ drop. Later sections of the flow (patrols, fleeing, factory) go into the same class. |
| Station actor | `Missions/Berlin/ForgeStation/BerlinForgeStation.h/.cpp` | `ABerlinForgeStation`: shell mesh; builds the lights, Niagara, sounds and turning parts at BeginPlay; runs the sedation cycle (writes `SedationPulse` / `SedationFrost` to `MPC_ForgeSedation`); swaps each dead target for its wreck and stops the dressing linked to it; no new cycle once the garrison is engaged; `StopStation` when the bonus is done. |
| Trigger | `Missions/Berlin/ForgeStation/ForgeStationSightedTrigger.h/.cpp` | `UForgeStationSightedTrigger`, a child of the generic `UPlayerUnitsNearMissionTrigger` (centre = the actor tagged `ForgeStation`, fallback the station pivot; radius 100 m), which also fires when any target already died. |
| Mission (C++ parent) | `Missions/Berlin/ForgeStation/Mission_BerlinForgeStation.h/.cpp` | `UMission_BerlinForgeStation` (BONUS plate). Natively: sets up and spawns the garrison from the difficulty's data asset **before** the trigger; once the plate is up plays the sighted line and tracks the three targets (title count + minimap); fires `BP_OnAllStationTargetsDestroyed` once (and stops the station) when they are gone. BlueprintCallable steps: `GiveIntel` (archive entry `CustomContent`, Lore, key `Berlin_ForgeCraterSurvey` + intel line) and `LaunchReinforcements` (reinforcement line + drop at (1600, 3550)). No timers. |
| Mission stage (Blueprint) | `/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/Mission_Berlin_ForgeStation` | Owns the reward flow, per `BerlinMissionScripting.md` (rules section 1, stage section 5): `BP_OnAllStationTargetsDestroyed` → `ScheduleSingleCallback(IntelDelaySeconds = 1)` → custom event `OnIntelDue` → `GiveIntel` → `ScheduleSingleCallback(ReinforcementsDelaySeconds = 8)` → custom event `OnReinforcementsDue` → `LaunchReinforcements` → `OnMissionComplete`. The callback pins are wired from the custom events' own delegate outputs (the same binding a Create Event node makes). Built with UECP `build_blueprint_graph` in a DebugGame editor. |
| Test | `Missions/Berlin/ForgeStation/Tests/ForgeStationTestSubsystem.h/.cpp` | `-ForgeStationValidate` on Ger_Berlin (runner `Source/Tools/BerlinMap/poi/run_forge_station_test.ps1`, `-Shots` for three lit captures). **30/30 PASS** on Normal, 2026-11-22: garrison from the data asset (7 defenders), targets built with their station meshes, plate hidden until a player unit comes within the ring, both crews reach a stake and play the work montage, a full sedation cycle, wrecks on death, bonus complete, archive entry, drop aircraft flying. |

### Assets (`/Game/RTS_Survival/Blueprints/Environment/Berlin/ForgeStation`)

- `Split/`: `SM_ForgeStation_Shell`, `_Rig`, `_CryoBlock`, `_CommandBunker` (+ `_Destroyed` each), `_DrillString`,
  `_Dish`, `_SampleStake`, `_CryoCanister`. Made in Blender by `v3_split/generate_forge_station_split.py`
  (Blender folder `CustomMapItems/Environment/BerlinForgeStation/v3_split`, layout in `split_layout.json`).
- `BP_ForgeStation_Rig`, `_CryoBlock`, `_CommandBunker`: copies of `Rus_Building_CoolingTowers_Soviet` (passive Forge
  building: health, selection, holes) with the part's mesh; enemy-owned (player 2). A placed building starts unbuilt;
  the station finishes it at BeginPlay (`StartFieldDefenseConstruction` + `FinishFieldDefenseConstruction`, as
  parachuted buildings do), then puts the station mesh back and clears the cooling tower's override materials, and
  switches off the cooling tower's two smoke emitters (the Blueprint components could not be deleted from Python).
  Like every enemy building they are hidden by the fog of war until a player unit sees them. They take the cooling tower's
  building data (health = 6 Pz IV shots each, its name in the UI) **(open)**: own building types if the owner wants
  the planned 6 / 8 / 6 shots or their own names.
- `MPC_ForgeSedation`, `M_ForgeSedatedCrystal` + `MI_ForgeSedatedCrystal` (on every Radixite slot of the station).
- `Sounds/`: `S_ForgeStation_CryoVent`, `_CompressorRise`, `_PumpLoop` (procedural,
  `Source/Tools/BerlinMap/poi/forge_sounds_gen.py`).
- `/Game/RTS_Survival/Blueprints/Missions/GerCampaign/Berlin/DA_BerlinForces_NewToRTS/_Normal/_Hard/_Brutal`.

### On Ger_Berlin (saved; the map is local only, skip-worktree)

Placed by `Source/Tools/BerlinMap/poi/forge_place.py` in a throwaway editor (`forge_editor_server.py` +
`forge_cmd.py`, command folder `Saved/BerlinForge/cmd`). Pre-edit copy:
`Saved/BerlinForge/backup/Ger_Berlin_before_forge_2026-11-22.umap`.
- `FORGE_Station` replaces `POI_ForgeSurveyStation` (same transform). **Note:** `berlin_map_build.step_custom_poi()`
  would place the old one-mesh station again; do not re-run it without removing that entry.
- `FORGE_Target_Rig / _CryoBlock / _CommandBunker`, `FORGE_Strongpoint` (id `ForgeStation`, 7 named posts, two rim
  circuits), `FORGE_Megaphone`, `FORGE_Stake_1..4` + `FORGE_Canister_*` on the rim; the mission (since the
  reshape the Blueprint stage `Mission_Berlin_ForgeStation`) is in
  `BP_MissionManager`'s Missions.

### Reshape to a Blueprint stage (2026-11-22, owner-approved via the scripting session)

- Removed from C++: the intel and reinforcement `FTimerHandle`s and the tick-driven completion.
- **Tracking:** `TrackActorsWithMiniMapProgress` normally completes its mission when the count is full. The scripting
  session added `bCompleteMissionWhenTrackingDone` + `OnTrackingReachedMaxCountNative` / `BP_OnTrackingReachedMaxCount`
  to `UMissionBase`; the Forge class sets the flag to false and fires `BP_OnAllStationTargetsDestroyed` from the
  native twin, so only the Blueprint's `OnMissionComplete` ends the stage.
- Ger_Berlin: the manager's `Missions` held only this stage, so it sits at index 0, not the planned index 9; the
  scripting session lays out the array. Pre-edit copy `Saved/BerlinForge/backup/Ger_Berlin_before_forge_bp_stage.umap`.
- Open question 5 (crews react to the alarm): the hook is there (`OnOtherMissionEndedNative`, alarm =
  `Mission_Berlin_StayUndetected` failing), not built until the owner decides.
- Compiled in the DebugGame editor target; the Development DLL was rebuilt at 18:39 on the user's go-ahead (it also
  carries the scripting session's G1-G4 and tracking flag). Test run with `run_forge_station_test.ps1 -DebugGame`: 29/29 PASS after the reshape.

### Not done / open

- The three advisor lines are empty slots on the mission (owner writes and records them).
- Section 9 points 5-7; the alarm reaction of the crews (point 5) is not built.
- Station guards use the map's enemy director for their look; `_Soviet` (Forge) variants only appear if the
  director override maps them.
- Lit captures from the test run: `Docs/Concepts/BerlinForgeStation/build/ForgeStation_front.png`,
  `_crater_side.png`, `_wrecks.png`. The stack plume is the green-yellow `NS_RadixiteSmoke1`; swap it for a white
  steam system if the owner prefers.
