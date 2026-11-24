# Mission timeline estimate (no play test)

Large maps cost too much to play-test for every tuning change. This tool estimates *when* things happen in a mission
from the data alone:
- when each objective plate appears and completes;
- each cinematic, voice line and air delivery;
- each convoy, from its spawn through its stops and branches to its exit;
- each counterattack wave.

It works for any map that uses the mission manager and `UMissionBase` stages. Started 2026-11-24 on the Ore Mountains
map, at the owner's request.

## Run it

From `Source/Tools/MissionTimeline/`:

```
python export_model.py Ger_2ndOreMountains          # ~30 s, headless, no GPU
python simulate.py Ger_2ndOreMountains               # < 1 s
```

Both write to `Saved/MissionTimeline/<Map>/`:
- `model.json`: the exported model;
- `export.log`: the commandlet log;
- `timeline.txt`: the readable timeline, followed by the pacing warnings;
- `timeline.json`: the same data, including convoys, marks, stage triggers and warnings;
- **`timeline.html`**: the report (open it in a browser). It holds:
  - summary cards;
  - a pacing warnings table;
  - the lane chart: objectives, cinematics, voice lines, air deliveries, convoys (with stops, branches, dismounts,
    ambush zones and minimap marks), attacks, and warnings. Hover any bar for its details;
  - the stage-flow graph: each stage one column right of the stage whose completion starts it;
  - the full event list.

Command-line overrides:
- `--set key=value` overrides a simulation assumption, for example `--set air_delivery_seconds=45` or
  `--set difficulty=hard`.
- `--pace key=value` overrides a pacing threshold, for example `--pace quiet_gap_s=120`.

### Pacing warnings (`report.py`, thresholds in `PACING`)

| Kind | Fires when | Default |
|---|---|---|
| burst | `burst_count` new objective plates within `burst_window_s` | 3 in 30 s |
| quiet | Longer than `quiet_gap_s` with nothing to see, hear or fight (no cinematic, voice line, delivery, convoy on the road, attack or new plate) | 90 s |
| voice queue | A line waits longer than `voice_queue_s` behind lines of **another** stage (a stage's own back-to-back lines are intended) | 8 s |
| screen wait | A cinematic waits longer than `screen_wait_s` for the screen | 5 s |
| reaction | A convoy reaches a minimap mark (within 80 m of its route, still shown) sooner than `reaction_window_s` after the mark appeared | 60 s |

First Ore result (2026-11-24, passive):
- **reaction:** convoy 1 reaches the ambush mark 40 s after the mark appears.
- **burst:** 3 objectives appear at 7:33 (Hold the ridge, Stop the ammo convoy, Destroy Base B).
- **voice queue:** the convoy 3 start line waits 15 s behind convoy 2's line and the counterattack warning.
- **quiet:** two quiet stretches of 203 s and 226 s, both inside the 240 s gaps before convoys 4 and 5.

**Which build:** `export_model.py --config DebugGame|Development` picks the editor build. Use one that no open editor
holds: a running editor locks its DLL, but not the other configuration's.

## How it works

### 1. Export (C++)

`UMissionTimelineExportCommandlet` (`Missions/Timeline/`) is an editor commandlet. It loads the map without starting
it, interprets nothing, and writes:
- **missions:** every entry of the mission manager's `Missions[]`, with its trigger. These are the map instance's
  property values, so instance overrides count.
- **blueprints:** every stage Blueprint, its Blueprint parents and the macro libraries it uses. For each graph it
  writes every node with its reflected settings (function, event, variable, struct, delegate references), its pins,
  pin defaults and links.
- **actors:** label, class chain, location, yaw and tags for every actor. Full properties only for the
  `-PropClasses`: convoy routes, wave spawners, the battery, the pressure director, towers, stations and similar.
- **assets:** the data assets and sounds those properties reference, followed 3 levels deep, with sound durations.
- **enums:** the enums seen on graph pins. Pin literals hold the index (`1`) while events hand over the name
  (`Escaped`), so the simulator needs the mapping between them.

### 2. Simulate (Python)

`simulate.py` interprets the stage Blueprints on a clock.

**Graph features it runs:**
- events, custom events and function calls (self functions run their own graph);
- branch, switch on int / enum, variable get and set;
- make struct and make array, pure math;
- the DoOnce, IsValid and ForEachLoop macros, and pure macros such as `WasConvoyStopped`;
- delegate binds;
- scheduled callbacks, single and repeated.

**Modelled from the exported data:**
- **Stage triggers:**
  - no trigger: the stage starts at 0:00;
  - mission class completed, also-watched classes included;
  - player units near: fires when a delivered or spawned player unit lands inside the radius;
  - `LoadToStartDelay`.
- **Location intro:** its duration (7 s when 0).
- **Camera tour:** the sum of its shots plus the return.
- **Screen time:** intros, tours and takeovers wait for each other.
- **Voice lines:** queued back to back, each lasting its sound length.
- **Cargo-plane deliveries:** an assumed flight time.
- **Convoy routes:** the first-convoy delay, forming seconds, and the pace (the slowest truck's calibrated speed × the
  truck speed fraction). Along the route:
  - the stop point and its hold;
  - the hunter dismount point;
  - the branch: its path length and the arrival event at its end;
  - ambush zones, with the radio-post speed boost.
- **`USovietCounterattackMission`:** preparation, waves × seconds between waves, from its settings asset at the chosen
  difficulty.

**Scenario:** what the player does.
- `passive` (the only scenario so far): the player stops nothing and destroys nothing. Every convoy escapes, kill and
  tracking objectives never finish, and "near" triggers fire only from delivered or spawned units.
- This gives the longest timeline and the bare pacing of the scripted parts.

### Assumptions (printed at the top of every timeline)

| Rule | Value | Why |
|---|---|---|
| `air_delivery_seconds` | 35 | Flight from the map edge plus lowering; the real spawn point depends on runtime aircraft settings |
| `takeover_read_seconds` | 12 | Field manuals stay up until the player closes them |
| `difficulty` | normal | Counterattack settings and convoy escort tier |

### Unmodelled

Every call or node without a rule is listed at the end of `timeline.txt` and counted as 0 s. When a new map shows
entries there, add a `fn_<FunctionName>` handler, or a `PURE` entry for math, in `simulate.py`.

## Limits

- **Travel times:** the convoy pace is the free-road pace. Slopes, the viaduct gap and traffic make real runs somewhat
  slower.
- **Plane flight time:** this is a guess, until the aircraft spawn logic is modelled.
- **What the player does:** only the passive scenario exists so far.

## Next steps (owner's plan, 2026-11-24)

1. *(done 2026-11-24)* Extractor and passive simulator on the Ore map.
2. *(done 2026-11-24)* Timeline chart (lanes), stage-flow graph and pacing warnings (`report.py`).
3. Prompt and custom scenarios: the player reaches spots by navmesh distance; events like "Base B falls at 25:00".
4. Run it on the Intro, Berlin and the other maps.
