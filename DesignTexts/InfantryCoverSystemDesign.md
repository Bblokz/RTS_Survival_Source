# Infantry Cover System — Architecture and Phased Design

Author: design pass, 2026-10-06  
Status: **Phase 1 cover discovery and unit-local automatic cover use implemented and validated on TestCover (2026-10-07); cover montages and squad-level assignment remain future work**  
Test map: `/Game/RTS_Survival/Maps/TestCover/TestCover`

The original analysis and longer-term design are retained below; sections 2, 9, and 10 describe the plan as written before the automatic cover layer existed.

### Implemented automatic cover use (unit-local, parallel to the command queue)

- `URTSCoverFinderWorldSubsystem` staggers `ASquadUnit::UpdateAutomaticCover` over registered units (`M_MaximumTacticalUnitUpdatesPerFrame`), and does nothing while the world is paused.
- A unit takes cover only while its own command is `IdIdle` (squad queue idle) or `IdAttack` with its target already in weapon range. Team-weapon squads and squads inside cargo are excluded. Any commanded movement cancels cover first.
- Selection: nearest unreserved point within `M_AutomaticCoverSearchRadius` that faces the target, lies inside 90% of weapon range, and has a target-specific firing lane. At most eight lane traces per search; a failed search waits 1.5 s before retrying.
- Movement: one tagged AI move whose request ID is intercepted before the command-completion switch, so it can never complete or replace a queued command. It uses the same reach test as commanded moves. A unit still running past its deadline is circling the point: it is stopped and sent once more, then the point is reported unreachable to the subsystem and withheld from every unit for 30 s.
- Firing: blocked while walking to cover, while entering, and while protected behind standing cover. Crouch cover fires once the lane is validated; standing cover exposes first. The lane is a single trace on the enemy team's trace channel from the posture's firing position, revalidated only when the target changes or moves more than 1 m. Any enemy as first hit keeps the lane open.
- Provider ignore: the scanner tags each point with a numeric handle for the actor whose collision produced it. On entering cover the unit resolves it once and registers the actor on its weapon with reason `Cover`, same-owner only. Cargo ignores use reason `External`, so the two never remove each other.
- Missing animations: every missing montage or pose is logged once per animation class and the gameplay state continues immediately. Without an expose montage the capsule is moved in code to the subsystem's default peek step and back.
- The scanner ignores soldiers and what they carry, both as cover geometry and as an obstruction of a slot. Before this, an occupied point unpublished itself on the next rescan.

### Cover animations and root motion

Each animation set on the infantry animation Blueprint (`CoverAnimations`) carries designer offsets of type `FSquadUnitCoverLocalOffset`, expressed for a soldier facing the cover: `TowardCover` (negative is further out in front of the cover) and `Right`.

| Property | Set | Meaning | Default |
| --- | --- | --- | --- |
| `EnterStartOffset` | Standing left, standing right | Where the unit stops before the enter montage plays. Set it to minus the travel of the enter clip. | `TowardCover = -125` |
| `EnterStartOffset` | Crouch | Same, for the crouch enter clip. | `0, 0` |
| `ExposedOffset` | Standing left | Where the expose montage leaves the capsule; also the origin of the firing-lane test. | `-46, -110` |
| `ExposedOffset` | Standing right | Same for the right side. | `-44, 62` |

The defaults are the root travel measured on the current clips. When a clip is replaced, run the map with cover debug symbols: the first use of each pose logs `RTS_COVER_ROOT_MOTION` (travel extracted as capsule root motion) and `RTS_COVER_ROOT_TRACK` (travel of the root track per sequence, including clips whose root motion is switched off). With `LogRTSSquadUnitCover Verbose`, every transition also logs `RTS_COVER_ALIGNMENT` with the residual between where the clip left the capsule and where the offset says it should be; add that residual to the offset to remove it.

How a unit uses them:

1. It walks to `cover point + EnterStartOffset`, turns to face the cover, and slides the last centimetres onto that exact location (path following stops up to an agent radius short).
2. The enter montage plays. If the montage has extracted root motion, the root motion carries the capsule onto the cover point. If it does not (the current high-cover enter clips are the exit clips played in reverse with root motion switched off), the mesh shows the travel as a root offset from frame one, so the capsule is placed on the cover point at montage start and the montage starts without blend-in; the soldier visually stays where he stopped and walks in.
3. Expose and return montages move the capsule by root motion. When each transition ends the capsule is slid onto the expected location (`cover point + ExposedOffset`, or the cover point), which absorbs clip drift such as the left return clip ending 29 cm short of undoing its expose clip.
4. The animation instance reports every cover action change through `OnCoverAnimActionChanged`, so capsule alignment and the weapon release happen on the frame a transition completes. `Cover_AimReady` and `Cover_BackInCover` notifies are optional: they move the switch earlier inside a clip, and a clip without them switches when it ends.
5. A transition whose montage does not advance (meshes that are not rendered stop ticking montages) is completed by the unit after the montage length plus 0.75 s, and the capsule is placed where the clip would have left it.

The scanner anchors standing points to the wall edge: it bisects the coarse opening search down to a few centimetres and places the point `M_StandingPeekEdgeInset` (default 25 cm) inside the edge. The inset must stay smaller than the sideways step of the shortest expose clip (62 cm on the right side today), or the exposed muzzle stays behind the wall.

Not wired yet: the exit montages. Leaving cover cancels the cover pose immediately so a move order is never delayed by an exit clip.

### TestCover scenario

`-CoverFinderValidateTestCover` on the command line, or `RTS.CoverFinder.ValidateTestCover <idle seconds> [capture]` in the console, runs `FCoverTestScenario` (cover-debug builds only). It starts the game past the start-game gate, waits for the first full scan, asserts idle cover (unique reservations, occupants on their points, command queue untouched), walks one team to the exposed side of occupied standing cover, orders attacks on contact, and logs `RTS_COVER_TEST RESULT PASS|FAIL` with a five-second combat timeline. `-CoverFinderPlayerApproaches` swaps which team walks; `-CoverFinderCombatSeconds=` changes the combat window.

### Implemented phase-1 refinements

- The dedicated worker owns grid planning, classification, deterministic ordering, stable IDs, and deduplication. Navigation projection, collision queries, publication, and debug drawing remain budgeted game-thread work.
- Landscape is scanned across the Character navigation domain once at world startup and cached. Periodic refreshes gather compact bounds for non-landscape environment primitives, so static terrain is not repeatedly reprocessed.
- `URTSCoverProviderComponent` lets a designer add explicit local-space crouch, standing-left, or standing-right points to an assumed-static actor. It transforms and registers copied value data at `BeginPlay`, shares the detector's point-spacing/deduplication rule, and unregisters at `EndPlay`.
- Authored points win an equivalent-position tie with an automatically discovered point. No provider actor or component pointer is retained by the cover cache or worker.
- Standing cover is published only for a validated left or right firing opening. The designer-facing minimum opening is `M_StandingPeekGapWidth`; internal continuity, navigation, capsule, transition, and firing-lane checks remain hidden.
- Accepted crouch points draw light blue and both standing-side types draw orange while `GCoverFinder_Compile_DebugSymbols` and the runtime draw setting are enabled.

---

## 1. Goals

The finished system should let ordinary squad infantry discover and use cover produced by world geometry without requiring the geometry to implement a cover interface or derive from a particular actor class. A landscape crater, a sufficiently steep hill, a static obstacle, a building, and a vehicle should all be evaluated by the same geometric rules.

The first implementation phase is narrower:

1. Periodically scan the playable world for infantry cover.
2. Perform the expensive classification, merging, and cache construction on a dedicated worker thread.
3. Keep all Unreal world, collision, navigation, actor, component, and debug-draw access on the game thread.
4. Publish immutable, queryable cover-point snapshots on the game thread.
5. Draw crouch cover in light blue and standing cover in orange when cover-finder debugging is compiled in and enabled.
6. Validate the finder in `TestCover` before connecting it to squad decisions, movement, weapons, or animation.

The longer-term design must leave room for:

- squad-level selection and reservation of cover;
- individual squad-unit destinations;
- standing-cover side peeking before firing;
- crouch-cover firing behavior;
- player-cursor-driven squad orientation when the selection contains squads only;
- moving cover providers such as vehicles;
- landscape deformation and destructible geometry invalidation.

---

## 2. Non-goals for phase 1

Phase 1 does **not**:

- make a squad decide when to seek cover;
- add a command or ability;
- alter `ASquadController`, `ASquadUnit`, `AInfantryWeaponMaster`, or `USquadUnitAnimInstance` behavior;
- reserve or occupy cover points;
- play cover montages;
- change the cargo system;
- change the player formation/cursor controls;
- score cover against a particular enemy or incoming fire direction;
- attach infantry to cover-providing actors.

Keeping phase 1 read-only from gameplay is important: the detector can be measured and corrected in isolation before it is allowed to redirect squads.

---

## 3. Existing codebase findings

### 3.1 Squad controller and squad-unit ownership

`ASquadController` owns the command queue and fans commands out to its `M_TSquadUnits`.

- A move begins in `ASquadController::GeneralMoveToForAbility`.
- The controller creates one base navigation path, clones it, and applies fixed per-unit offsets from `M_SqPath_Offsets`.
- Each `ASquadUnit` owns the actual `AAISquadUnit` movement request and reports completion to the squad controller.
- `ASquadController::OnSquadUnitCommandComplete` counts completions and advances the command queue.
- The controller actor is moved to the average live squad-unit position every tick.
- A weapon that finds a specific target out of range asks the squad controller to move the whole squad closer.

This means a later cover integration should keep high-level intent and assignment in `ASquadController`, but continue sending final per-soldier locations through `ASquadUnit`. The global cover finder should not command units itself.

### 3.2 Player formations already solve the outer formation problem

`UFormationController` currently assigns one destination and rotation per selected command actor. A squad controller is treated as one formation item; the soldiers inside that squad are subsequently spread by `ASquadController`.

The formation controller already has:

- cursor/click-derived facing;
- a player rotation override;
- stable actor-to-slot assignment intended to reduce crossing;
- a gate that filters selections to actors with the move ability.

The future Company of Heroes-style feature should extend this two-level flow instead of bypassing it:

1. `UFormationController` chooses the squad controller's outer destination and facing.
2. `ASquadController` uses that facing, nearby cover, and its live members to choose individual soldier slots.

The cover cache itself should remain independent of selection and cursor state.

### 3.3 Cargo must remain a separate state machine

`UCargoSquad` is an explicit occupancy system. It owns a current cargo component, seat socket assignments, attachment, movement disabling, and temporary ability removal.

Cover differs in every important respect:

- a cover point is a world-space tactical opportunity, not an owned cargo slot;
- it is not attached to or identified by a provider actor;
- it must disappear when the geometry no longer validates;
- entering cover must not call cargo APIs or mutate `ESquadCargoState`;
- units inside cargo must be ineligible for cover assignment.

The only shared concept should be future command-preemption policy: a move, retreat, cargo operation, death, or other incompatible action must release a unit's cover reservation and animation state.

### 3.4 Current animation flow

`USquadUnitAnimInstance` currently separates:

- `ESquadMovementAnimState`: `Idle`, `Walking`, `Running`;
- `ESquadAimPosition`: `Standing`, `Crouch`, `Prone`;
- weapon-specific aim-offset assets in `FAimOffsetTypes`;
- weapon fire/reload/switch montages in `FWeaponMontages`;
- full-body stance and miscellaneous montages in `FAimPositionMontages`;
- separate team-weapon crew montage runtime state.

Velocity is sampled by a timer on `ASquadUnit`, then passed to `UpdateAnimState`. When an ordinary infantry unit becomes idle while aiming, the current implementation may randomly choose standing or crouching. Starting to walk returns a crouched unit to standing. These automatic transitions use the same `FullBody` montage slot used by team-weapon crew animation, death, grenade, welding, and other full-body actions.

The attached AnimGraph shows the movement enum selecting idle/walk/run after the weapon/upper-body layers, with a final `FullBody` slot. The cover assets already present under `CoverAnimations` contain high/low, left/right, idle, aim, enter, and return variants. The montage screenshots also show a `Cover_BackInCover` notify.

Therefore cover should **not** be added to `ESquadMovementAnimState`. Movement and idle presentation are separate concerns. The later animation phase should add an idle-pose enum and a cover runtime state machine.

### 3.5 Current weapon flow is not yet cover-aware

`AInfantryWeaponMaster` selects targets and calls `WeaponState->Fire` once range and aim-angle checks pass. When idle, it may rotate the entire squad unit to face a target and approves firing immediately. The fire callback then asks the anim instance to play a standing/crouched/prone weapon montage.

That behavior is incompatible with standing cover. A standing-covered unit must not rotate freely or fire immediately: it must select a valid left/right peek, play the expose montage, enter a peek aim pose, and only then allow the shot. This must later be enforced before `WeaponState->Fire`; merely playing a cover montage after the weapon has fired would be too late.

### 3.6 Existing async precedent and its limit

The project has dedicated `FRunnable` workers for target and resource searches. Those workers operate on copied data snapshots and marshal callbacks to the game thread. That ownership direction is appropriate for cover classification.

The project also explicitly guards Recast projection against off-game-thread access in `EnemyNavigationAIComponent`. The cover finder must follow that stronger rule:

- no `UWorld` collision query on the worker;
- no Recast/nav-system query on the worker;
- no `UObject`, actor, component, `FHitResult`, or weak UObject pointer stored or dereferenced by the worker;
- no debug draw on the worker.

The phrase "async cover finder" consequently means an asynchronous pipeline, not that Unreal world queries themselves are moved unsafely off the game thread.

---

## 4. Cover definitions

### 4.1 Infantry dimensions

The detector uses a designer setting of **180 cm infantry height**.

Standing cover starts at **80% of infantry height**, so the initial standing threshold is:

`180 cm × 0.80 = 144 cm`

The ratio is stored, and the 144 cm value is derived. It should not be duplicated as a second magic number.

The minimum crouch-cover height remains a tuning decision. The proposed provisional value is 90 cm, but it must be confirmed before implementation.

### 4.2 Cover types

Use an enum, not a boolean:

| Type | Geometric meaning | Future combat meaning |
| --- | --- | --- |
| Crouch | Continuous obstruction reaches the configured crouch threshold but is clear at the 144 cm standing threshold. | Unit uses a low-cover idle/aim set. Exact over-cover versus side-peek firing is still to be confirmed. |
| Standing | Continuous obstruction reaches at least 144 cm. | Unit is protected while idle and cannot fire until a valid left or right peek transition exposes it. |

An enum is justified here because later cover types or postures are plausible, and the two existing states already have materially different behavior.

### 4.3 Directional definition

Every cover point has a horizontal `CoverNormal` that points from the blocking surface toward the protected soldier position. It is independent of any current enemy.

For a future query, a threat is on the protected-against side when the direction from soldier to threat points substantially opposite `CoverNormal`. Storing a normal rather than an owning actor lets one cache support threats from any direction and keeps the detector provider-class agnostic.

A location may legitimately produce more than one cover record when different normals protect it in different directions, such as the inside of a crater or a corner. Deduplication must compare both position and normal.

### 4.4 Candidate validity

A published point must satisfy all of the following:

1. It projects to the Character infantry nav data.
2. An infantry-sized capsule can occupy the protected position.
3. The blocking geometry is close enough to provide immediate cover rather than being a distant hill on the horizon.
4. Obstruction is vertically continuous from a low anchor sample through the required cover-height sample. This rejects roofs, branches, and floating geometry.
5. Adjacent lateral samples show enough width to protect an infantry unit. A thin pole should not become a full cover slot.
6. The candidate is not inside blocking geometry.
7. Its numeric data is finite.

The finder uses geometry and collision responses only. It never asks whether a hit actor is a wall, crater, building, or vehicle.

---

## 5. Proposed phase-1 architecture

### 5.1 `URTSCoverFinderWorldSubsystem`

A tickable world subsystem owns the system because cover is per-world, shared by every squad, and must have a lifecycle that matches PIE/map travel.

Responsibilities:

- resolve the Character infantry nav data through `URTSNavAgentRegistry`;
- enumerate loaded navigable tiles/regions that define the playable scan domain;
- schedule periodic tile refreshes;
- execute budgeted collision, nav projection, and capsule validation on the game thread;
- send plain-data observations to the worker;
- consume completed worker results;
- perform a final game-thread freshness validation;
- atomically replace the public snapshot for a completed tile revision;
- draw debug points and normals on the game thread;
- stop and join the worker during subsystem deinitialization.

The subsystem must ignore editor preview/CDO worlds and run only for supported game/PIE world types.

### 5.2 `FCoverFinderWorker`

A dedicated `FRunnable` is a good fit with the user's requirement and the codebase's target/resource finder precedent. Unlike the existing polling workers, this worker should wait on an event rather than sleep in a tight periodic loop.

Responsibilities are pure-data only:

- build deterministic grid/radial probe plans from tile bounds and a copied settings snapshot;
- classify returned probe observations as no cover, crouch cover, or standing cover;
- average/repair normals;
- reject discontinuous surfaces and inadequate width;
- merge neighboring candidates into cover segments;
- apply point spacing along segments;
- derive left/right segment directions and preliminary peek-side availability;
- deduplicate by tile, quantized position, type, and normal cone;
- return a result tagged with world generation, tile ID, and tile revision.

The worker owns its mutable arrays. Requests and results cross thread boundaries by move through thread-safe queues. It never shares a mutable `TArray`/`TMap` with the subsystem.

Shutdown must signal `Stop`, trigger the event, wait for completion, and only then destroy the thread. Forced thread killing is not part of the design.

### 5.3 Plain-data messages

The message layer should contain no reflected object references.

| Message/data | Essential content |
| --- | --- |
| Tile scan request | World generation, tile ID/revision, bounds, deterministic grid origin, copied numeric settings |
| Probe plan | Candidate foot position, radial direction index/vector, requested height samples |
| Probe observation | Blocking flags, impact positions/normals/distances copied from game-thread hits, ground/nav result, capsule-clear result |
| Tile result | Generation/revision, cover points/segments, classification statistics, elapsed worker time |
| Published cover point | Stable value ID, location, normal/tangent, type, measured height band, left/right peek availability, tile/revision |

Do not pass `FHitResult` to the worker because it carries physical-material, actor, and component references. Copy only the finite numeric fields required for classification.

### 5.4 Published cache

The game-thread cache is partitioned by spatial tile. Each completed tile replaces its previous result as one operation. Readers receive a const snapshot or copied query results; they never retain pointers/references into a `TArray` or `TMap` that a later publication can reallocate.

Each point gets a value-type stable ID built from tile ID, tile revision-independent quantized coordinates, normal bucket, and cover type. Future reservation state belongs in a separate game-thread table keyed by that ID. It must not be written into the immutable geometry snapshot.

Generation and revision tags prevent results from an old PIE world, unloaded tile, or superseded scan from being published.

---

## 6. Scan pipeline

### 6.1 Define the scan domain

Scan the loaded tiles of the Character infantry nav data rather than the landscape's raw bounds or every actor's bounds.

Benefits:

- points are generated only where infantry may plausibly stand;
- caves, buildings, landscape, and ordinary meshes are treated uniformly;
- the work naturally partitions for World Partition/streaming;
- the system reuses the project's explicit `ERTSNavAgents::Character` setup instead of assuming the default navmesh is always infantry navigation.

On the small `TestCover` map this should cover the entire playable test area. In streamed maps, only loaded/nav-ready tiles are published; tile load/rebuild marks a tile dirty.

### 6.2 Generate candidates on the worker

For each dirty tile, generate an XY grid using a world-aligned origin and configured spacing. World alignment is important: rescanning the same tile must produce the same candidate coordinates and stable IDs.

At each grid location, generate evenly spaced horizontal probe directions. Eight directions are a reasonable initial default; the count is developer-configurable and can be increased for curved crater rims.

### 6.3 Ground and navigation sampling on the game thread

Process only a configured maximum number of world queries per frame.

For each candidate:

1. Project to the Character navmesh using a batch where possible.
2. Establish the local foot height.
3. Reject candidates without navigable ground.
4. Queue radial collision probes at the configured vertical samples.

This is a multi-frame state machine. A scan interval schedules work; it does not permit one interval to issue every trace for the entire map in one frame.

### 6.4 Class-agnostic geometry probes

Use a dedicated `CoverProbe` trace channel. Its collision profile should:

- block landscape, world-static geometry, world-dynamic obstacles, buildings, and vehicle bodies;
- ignore pawn capsules, character meshes, triggers, selection volumes, decals, effects, and UI helpers.

This is still class agnostic: eligibility is a primitive collision response, not a cast or interface. It also makes it possible to opt a decorative/no-collision mesh out without adding a cover-specific actor class.

For each direction, sample at minimum:

1. a low anchor height;
2. the crouch-cover threshold;
3. the derived standing threshold of 144 cm.

Classification:

- no anchor/crouch obstruction: no cover;
- anchor and crouch obstructed, standing clear: crouch cover;
- anchor, crouch, and standing obstructed: standing cover;
- high obstruction without matching lower obstruction: reject as floating/overhead geometry.

Hits used for one classification must lie within configured distance/alignment tolerances and have compatible horizontal normals. This prevents unrelated foreground and background objects from being combined into a false vertical wall.

The maximum probe reach is deliberately short. It is what lets a crater rim or sufficiently steep hill count while a gentle, distant terrain rise does not.

### 6.5 Normalize the protected position

The initial grid location is only a seed. For a valid hit:

1. derive a horizontal cover normal from the hit normal and probe direction;
2. move from the surface into the protected side by infantry capsule radius plus a configurable gap;
3. project that position to Character nav data;
4. test an infantry-sized capsule there;
5. re-probe from the normalized position to ensure the same obstruction still supplies the classified height.

This produces points aligned to real surfaces instead of leaving them at arbitrary grid offsets.

### 6.6 Width, segments, and standing peeks

Neighboring valid points with compatible type, normal, height band, and surface distance are merged into a segment. Final points are distributed along the segment using configured soldier spacing.

Each point stores a tangent and preliminary left/right peek flags. A side flag requires:

- a capsule-clear lateral expose position;
- a clear transition corridor between protected and exposed positions;
- no immediate cover obstruction in the proposed firing corridor.

For standing cover, a point with no valid side can remain useful as a protection-only point, but it must never later be considered immediately fire-capable. This distinction matters for long walls: middle slots may protect many soldiers while only segment ends or openings provide side peeks.

The exact movement distance and whether authored root motion or code/warping owns the lateral displacement is deferred to the animation phase.

### 6.7 Worker classification and game-thread publication

Once a budgeted batch is complete:

1. move numeric observations to the worker queue;
2. wake the worker;
3. classify, merge, and deduplicate off-thread;
4. return a revision-tagged tile result;
5. on the game thread, reject stale generations/revisions;
6. perform final nav/capsule validation for the compact result set;
7. replace that tile's published points;
8. draw debug output if enabled.

Only one scan generation per tile may be in flight. Repeated dirty notifications coalesce into the next revision rather than creating an unbounded queue.

---

## 7. Dynamic geometry and invalidation

Phase 1 should support moving vehicles and destroyed/moved obstacles through refresh, not provider registration.

- Every published point belongs to a tile revision.
- Tiles are rescanned on a rotating interval.
- A new result replaces the entire old tile result, removing points whose geometry disappeared.
- A configurable faster refresh band can later be applied around active infantry/combat without changing the data model.
- Nav generation/streaming changes immediately dirty affected tiles.

This deliberately avoids storing a provider actor pointer. A point beside a vehicle remains a world-space result; if the vehicle moves, the next tile result removes or relocates it. A future unit occupying that point should also perform a cheap immediate revalidation before entering cover and before each peek.

Landscape deformation and destruction systems can later call a simple `MarkBoundsDirty` API. That is an optimization for freshness, not a requirement for a landscape/object to be recognized as cover.

---

## 8. Developer settings and debug controls

### 8.1 Settings object

Add a `URTSCoverFinderDeveloperSettings : UDeveloperSettings` under the RTS project settings. It should be `Config=Game, DefaultConfig`, with numeric validation/clamps and grouped categories.

Proposed settings:

| Category | Setting | Initial intent |
| --- | --- | --- |
| Dimensions | Infantry height | 180 cm |
| Dimensions | Standing-cover minimum ratio | 0.80, deriving 144 cm |
| Dimensions | Crouch-cover minimum height | Provisional 90 cm; confirmation required |
| Dimensions | Low anchor height | Reject floating obstacles |
| Sampling | Grid spacing | Accuracy/work tradeoff |
| Sampling | Radial direction count | Initial 8, configurable |
| Sampling | Maximum obstacle distance | Reject distant terrain rises |
| Sampling | Tile refresh interval | Periodic sweep cadence |
| Budget | Maximum world queries per frame | Hard game-thread budget |
| Validation | Capsule radius/half-height and cover gap | Occupancy clearance; may be initialized from Character agent dimensions |
| Validation | Nav projection extent | Final point projection tolerance |
| Validation | Surface normal/alignment tolerances | Avoid combining unrelated hits |
| Segments | Minimum useful width | Reject poles/slivers |
| Segments | Cover-point spacing | One-soldier slot spacing |
| Peeking | Lateral expose distance/corridor size | Preliminary left/right side checks |
| Debug | Runtime draw enable, radius, duration, rejected-probe option | Avoid drawing the entire world by default |

Settings are copied into each worker request. The worker never reads the settings UObject.

### 8.2 Mandatory compile-time debug flag

Add this compile-time switch to `DeveloperSettings::Debugging` in the existing `DeveloperSettings.h` pattern:

`GCoverFinder_Compile_DebugSymbols`

All cover debug drawing and verbose cover-finder logs must be inside `if constexpr` blocks using that flag. The settings object's runtime draw toggle is a second gate; it cannot compile the code back in when the constexpr flag is false.

Required accepted-point colors:

| Cover type | Color |
| --- | --- |
| Crouch | Light blue, `RGB(173, 216, 230)` |
| Standing | Orange, `RGB(255, 165, 0)` |

Draw a point/capsule marker at the protected position plus a short arrow for `CoverNormal`. Optional rejected-probe visualization should use separate subdued colors and be disabled by default.

### 8.3 Instrumentation

Record per generation and per tile:

- candidate count;
- world-query count;
- crouch/standing/rejected counts;
- rejection reasons;
- game-thread sampling time;
- worker classification time;
- queue depth and stale-result count;
- total published points.

Add named Unreal Insights scopes around sampling, worker classification, and publication. The system is not accepted merely because it is on a worker thread; the game-thread trace budget must also be measured.

---

## 9. Future query and reservation layer

This layer begins only after phase 1 is trusted.

A future query should accept:

- origin and maximum search radius;
- threat position/direction;
- allowed cover types;
- path-distance budget;
- whether a firing-capable point is required;
- requesting squad/unit IDs for reservation filtering.

It should return value-type candidate records scored by:

- protection direction versus threat;
- cover type preference;
- nav/path cost, not only straight-line distance;
- standing-cover peek availability;
- spacing from squadmates;
- reservation/occupancy state;
- hysteresis so units do not churn between almost-equal points;
- freshness/revision.

Reservation belongs to the world subsystem but is mutated only on the game thread. Use stable point IDs and weak unit ownership at that boundary. Never retain pointers to elements inside the published arrays.

---

## 10. Future squad integration

### Phase 2 — queries, assignments, and reservations

- Add a squad-owned cover plan containing one value-type assignment per live unit.
- Query once for the squad, then solve unique point assignment for all members together; do not let each soldier greedily choose the same nearest point.
- Preserve current lateral ordering where practical to reduce crossing, following the formation controller's existing assignment philosophy.
- Exclude units inside cargo and team-weapon operators unless explicitly supported later.
- Release reservations on death, command replacement, cargo entry, retreat, invalidated geometry, and controller destruction.

### Phase 3 — movement and cover occupancy

- `ASquadController` remains the high-level coordinator.
- Each `ASquadUnit` moves to its assigned cover point through its own AI controller.
- Arrival validates point revision, nav reachability, capsule clearance, and blocking geometry again.
- After arrival, align the actor to `-CoverNormal`/the authored cover-facing convention and enter a unit-local cover runtime state.
- A normal move command exits cover first and then uses the existing movement path.
- Cover movement completion must not corrupt `M_UnitsCompletedCommand` or complete the wrong `EAbilityID`; autonomous tactical repositioning needs an explicit internal movement context rather than borrowing an unrelated command ID.

### Phase 4 — animation and weapon gating

Keep these dimensions separate:

1. `ESquadMovementAnimState`: idle/walking/running, unchanged.
2. A new idle-pose enum: regular, crouch cover, standing cover, and any later cover-aim base poses.
3. A cover action/runtime enum: none, entering, protected, exposing/peeking, exposed/aiming, returning, exiting.
4. A cover side enum: none, left, right (and possibly over-cover if confirmed for crouch cover).

The animation Blueprint's idle branch reads the new idle-pose enum. Entering cover is a full-body montage; after its completion/notify, movement state remains `Idle` while the idle-pose enum selects the correct cover idle/AO.

Use dedicated designer-facing structs rather than adding loose montage members:

- a crouch-cover animation set;
- a standing-cover animation set;
- left/right side entries within each set where the assets differ;
- a small runtime struct for active type, side, action, and montage ownership.

Standing-cover firing sequence:

1. Weapon has a valid target but requests permission to expose instead of firing.
2. Unit chooses an available left/right side from the cover point and target geometry.
3. A full-body cover-to-peek montage plays.
4. An explicit notify marks the pose as safe to fire; only then may `WeaponState->Fire` proceed.
5. The peek AO/idle pose handles continued aiming and bursts.
6. After the burst/decision window, the return montage plays.
7. The existing `Cover_BackInCover` notify marks the unit protected again and removes fire permission.

Montage duration alone should not decide exposure/fire timing. Use animation notifies so the gameplay window matches the authored motion.

The FullBody slot needs explicit arbitration. Death has highest priority; movement/retreat/cargo and incompatible abilities cancel cover; cover must suppress the current random idle crouch transition; team-weapon crew state and ordinary cover state must not be active together.

### Phase 5 — squads-only cursor orientation

When the player selection contains only squads, the player controller/formation controller can continuously or on command derive a ground-space facing from the cursor. That facing is passed to each squad's internal formation/cover planner.

The internal planner can then:

- prefer cover normals facing away from the cursor-designated threat/facing direction;
- arrange units laterally along valid cover segments;
- choose left/right standing peeks coherently;
- fall back to ordinary internal formation positions where cover is insufficient.

Mixed selections keep the existing general formation behavior unless a later design explicitly defines otherwise.

---

## 11. `TestCover` validation plan

The on-disk map is `Content/RTS_Survival/Maps/TestCover/TestCover.umap`, corresponding to package path `/Game/RTS_Survival/Maps/TestCover/TestCover` and shown in the Content Browser under `/All/Game/RTS_Survival/Maps/TestCover`.

The supplied overview contains sculpted hills/craters, long and short low obstacles, tall wall sections, isolated small obstacles, and open flat ground. Phase 1 should be tested against all of them.

### 11.1 Visual acceptance

- Tall wall faces at or above 144 cm produce orange points on their protected sides.
- Low obstacles in the configured crouch height band produce light-blue points.
- Crater interiors/rims and sufficiently steep terrain rises produce the correct directional points without any landscape-specific code path.
- Open flat terrain produces no points.
- Points do not appear inside walls, on top of non-navigable obstacles, outside navmesh, or where the infantry capsule cannot fit.
- Thin decorative objects below minimum useful width are rejected.
- Normals point from the obstruction toward the protected standing position.
- Long standing-cover segments clearly show which points have left/right peek capability.

### 11.2 Dynamic acceptance

- A vehicle with blocking `CoverProbe` collision creates cover without a vehicle cast.
- Moving/destroying that vehicle removes the stale points after the configured refresh.
- Moving/destructing a generic world-dynamic obstacle has the same result.
- PIE stop, map travel, and rapid PIE restart produce no worker callback into a destroyed world and no thread leak.

### 11.3 Performance acceptance

- World queries never exceed the configured per-frame budget.
- Large scans progress over frames rather than hitching at interval boundaries.
- At most one generation per tile is in flight.
- Worker queues remain bounded and dirty-tile requests coalesce.
- Debug-off cost does not include debug geometry construction.

### 11.4 Automated coverage

Add pure worker tests for:

- 89/90/143/144/180 cm boundary classifications after the final crouch threshold is approved;
- floating obstruction rejection;
- incompatible-hit alignment rejection;
- normal-aware deduplication;
- segment merging and point spacing;
- stable IDs under identical inputs;
- stale generation/revision rejection;
- deterministic output ordering.

Add a map integration test later that loads `TestCover`, waits for a completed generation, and verifies representative regions contain standing, crouch, and no-cover results. Exact coordinates should be captured only after the map layout is final.

---

## 12. Recommended implementation order

1. Confirm the decisions in section 13.
2. Add cover types, pure-data messages, and settings object.
3. Add the mandatory compile-time debug flag.
4. Add pure worker classification tests before world integration.
5. Add world subsystem lifecycle and safe worker startup/shutdown.
6. Add Character-nav tile enumeration and budgeted game-thread sampling.
7. Add worker classification/segment construction and revisioned publication.
8. Add debug draw and profiling counters.
9. Tune and validate only in `TestCover` until phase-1 acceptance is met.
10. Do not begin squad, weapon, or animation integration until the detector output is approved.

---

## 13. Intent questions to resolve before implementation

1. **Crouch-cover threshold:** Is 90 cm a suitable initial minimum, or should it be derived from a specific crouched mannequin height/percentage?
2. **Crouch firing behavior:** Does low cover fire over the top while remaining crouched, peek left/right, stand up briefly, or support more than one of those choices?
3. **Standing cover without a side:** Should protection-only middle positions on a long high wall remain valid, or should every standing-cover point be required to have at least one usable peek side?
4. **Peek displacement ownership:** Are the high-cover animations authored with root motion that should move the capsule, or should code/motion warping move the actor while the montage supplies presentation?
5. **Scan scope:** Should the production system continuously cover every loaded Character-nav tile, or prioritize regions around squads/combat and refresh the rest more slowly?
6. **Dynamic-cover freshness:** How quickly must cover beside a moving vehicle disappear—approximately every frame, sub-second, or at the normal multi-second scan cadence?
7. **Eligible infantry:** Are team-weapon crews and special infantry allowed to seek ordinary cover, or should phase 2 initially limit this to regular `ASquadController` squads?
8. **Autonomy trigger:** In later phases, should cover seeking happen automatically on combat, only near a commanded destination, through a player command, or through a stance/behavior setting?
9. **Cursor behavior:** Should squad-only cursor positioning update a preview continuously before the click, set only the formation facing on release, or directly select a threat-facing direction after the move order?
10. **Debug runtime gate:** In addition to the required constexpr compile switch, should accepted cover draw automatically whenever compiled, or require the proposed runtime settings toggle/console control?

Until these are answered, phase 1 can be implemented only with provisional assumptions for crouch height, scan cadence, standing protection-only points, and dynamic refresh.
