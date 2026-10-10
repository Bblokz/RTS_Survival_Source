# Infantry Cover System — Architecture and Phased Design

Author: design pass, 2026-10-06  
Status: **Phase 1 cover discovery and unit-local automatic cover use implemented and validated on TestCover (2026-10-07); cover montages and squad-level assignment remain future work**  
Test map: `/Game/RTS_Survival/Maps/TestCover/TestCover`

The original analysis and longer-term design are retained below; sections 2, 9, and 10 describe the plan as written before the automatic cover layer existed.

### Implemented automatic cover use (unit-local, parallel to the command queue)

- `URTSCoverFinderWorldSubsystem` staggers `ASquadUnit::UpdateAutomaticCover` over registered units (`M_MaximumTacticalUnitUpdatesPerFrame`), and does nothing while the world is paused.
- A unit takes cover only while its own command is `IdIdle` (squad queue idle) or `IdAttack` with its target already in weapon range. Team-weapon squads and squads inside cargo are excluded. Any commanded movement cancels cover first.
- Selection: nearest unreserved point within `M_AutomaticCoverSearchRadius` (1200 cm; enemy infantry, owned by player 2, uses `M_EnemyAutomaticCoverSearchRadius`, 2500 cm) that faces the target, lies inside 90% of weapon range, and has a target-specific firing lane. At most eight lane traces per search; a failed search waits 1.5 s before retrying.
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

### Squads-only move preview (player planning)

While only infantry squads are selected (no team weapons, nothing in cargo) and a right click would be a plain move, `ACPPController::Tick_UpdateSquadMovePreview` feeds the cursor to `USquadMovePreviewComponent`. The component is a non-scene `UActorComponent` on the controller; the formation code is untouched apart from one accessor for the rotation-arrow override.

- **Planning** lives in `FSquadMovePlanner` (`Player/SquadMovePreview/SquadMovePlanner.h`), pure value logic with no world access. Squads are placed in the shape picked in the formation picker (`EFormation`, read from `UFormationController::GetCurrentFormation`): a rectangle of squads centred on the cursor, a spear or thin spear with its tip on the cursor, or a semi circle whose wings hang back and face outward. The squads that already stand furthest forward take the front rows and keep their left-to-right order inside a row, so they do not cross. A squad in the open forms a wide block with even rows (seven men stand 4 + 3) instead of a line. Each squad ranks the cover around its anchor by distance, facing agreement, and whether the previous plan already used it; soldiers without cover line up in rows behind the cover that was taken. Soldiers are matched to slots left to right.
- **Cover use** is one `FindCoverPointsInRadius` query per replan against the published cache; points reserved by soldiers outside the selection are dropped. No traces and no scans are triggered by the preview.
- **Throttle**: a replan happens when the cursor moved 25 cm, the facing changed, or 0.5 s passed, and never more often than every 0.04 s. Measured on TestCover: 0.012 ms average for 35 soldiers in 6 squads.
- **Facing**: without the rotation arrow the squads face along their travel direction and take any nearby cover. While the player drags the rotation arrow the plan is anchored on the arrow's ground location, and only cover that protects against the chosen direction is used.
- **Drawing** (debug for now): a small sphere and facing arrow per soldier — green for regular standing, orange for standing cover, light blue for crouch cover. `bM_EnableSquadMovePreview` in the cover finder settings switches the whole feature off.
- **Issuing**: `MoveUnitsToLocation` first offers the order to `TryIssuePlannedMove`. The plan is handed to each `ASquadController` (`SetPlannedMoveDestinations`) and the normal move command is queued through the usual command path; when that command executes, `ExecutePlannedMove` sends each soldier to its own destination with its own path. Queued (shift) moves keep their plan until their turn.
- **Arrival**: a soldier planned onto cover walks to that point's entry location and then reserves and enters exactly that point through the existing automatic-cover state machine; if the point was taken meanwhile it falls back to the normal search. A soldier planned into the open turns to the planned facing and stays there until it has a target.
- **Arrival watchdog**: `UpdatePlannedMoveArrival` (run with the tactical cover update) notices a soldier that stays near its slot without arriving. Off screen that is the normal case: `RTSSquadUnitOptimizer` moves unseen soldiers in steps longer than the arrival radius, so they step over the slot back and forth; an unseen soldier is simply placed on its slot. A visible one is halted and re-sent once, then finishes its move where it stands, so the squad's command always completes.
- A soldier that cannot start walking (no path from where it stands) retries for 1.5 s and paths from the nearest spot its navigation filter accepts.

### Combat cover (choosing and changing cover by where the enemy is)

Without a target a soldier still takes the nearest free point. With a target, `TryReserveCombatCoverPoint` scores every free point in reach with `FRTSCombatCoverScoring` (`Navigation/CoverFinder/CoverCombatScoring.h`, pure value logic) and gives the firing-lane trace only to the best few:

- **Aim arc (hard limit)**: the target must be within `M_CombatCoverMaximumAimYawDegrees` (default 90, the range of the cover aim offsets) of looking straight at the cover. The same limit decides whether an occupied point is still valid.
- **Protection**: the share of threats within `M_CombatCoverProtectedHalfAngleDegrees` (default 60) of straight ahead, times `M_CombatCoverProtectionWeight`. Threats are the soldier's own target plus the targets of its squad mates, up to `M_CombatCoverMaximumThreats`.
- **Facing**: the cosine of the yaw to the soldier's own target, times `M_CombatCoverFacingWeight`.
- **Travel**: the walk to the point as a fraction of `M_CombatCoverRepositionRadius`, times `M_CombatCoverTravelWeight`, subtracted.

A soldier in cover with a target re-scores its point every `M_CombatCoverReevaluationSeconds` (spread per unit) once it has held it for `M_CombatCoverMinimumHoldSeconds`. It moves only when another point within the reposition radius beats the occupied one by `M_CombatCoverMinimumScoreGain`, has an open firing lane, and no squad mate is walking to cover at that moment, so a squad shifts one man at a time. `bM_EnableCombatCoverRepositioning` switches the moving off; the scored first choice stays.

**Tolerating target changes.** A point that cannot engage the current target (outside the aim arc, or no firing lane) is not given up at once. The soldier stays hidden with his weapon blocked, a standing soldier steps back first, and the point is tested again every 0.5 s; it is given up after `M_CoverTargetChangeToleranceSeconds` (2.5 s). An attack order is the exception and is never waited out: `GetCoverJudgementTarget` judges the cover against the ordered enemy whenever it is in weapon range, also when the weapon is on someone else, and a point that cannot engage it is left immediately. The search for new cover uses the same enemy. Leave reasons in the log tell the two apart ("stayed unusable against the target" and "cannot engage the attack order's target"), and `RTS_COVER_WAIT_ENDED` (Verbose) marks every wait that ended with the cover kept.

`-CoverFinderEnemyAdvance` and `-CoverFinderObserveCombat` on the TestCover scenario run an advancing enemy and log `RTS_COVER_OBSERVE` measurements of how well cover shields both sides; run them with `-RenderOffscreen` instead of `-NullRHI`, because without rendering the fog of war never starts and the enemy never shoots.

Tests: `RTS.CoverFinder.Combat.*` (5 tests) and the map test `-CoverFinderValidateCombatCover`, which puts one soldier at the densest free cover of the map, moves an enemy to twelve bearings around it, and logs `RTS_COVER_COMBAT_TEST RESULT PASS|FAIL`. `LogRTSCoverFinder Verbose` prints one `RTS_COVER_COMBAT_SEARCH` line per search and `LogRTSSquadUnitCover Verbose` one `RTS_COVER_REPOSITION` line per move, for tuning.

### Aimed probes: thin obstacles and grazing hits

The grid (200 cm, eight fixed directions) only finds a thin or round object when a grid position happens to line up with it. Two kinds of aimed probe, sampled after the grid of every environment scan, remove that luck:

- **Thin-obstacle ring** (`bM_ProbeThinObstacles`): an object no wider than `M_ThinObstacleMaximumWidth` (default 260 cm) gets `M_ThinObstacleRingSamples` probe positions around it, each looking at its centre. Width is taken from the collision of the overlapped body, per instance for instanced meshes, never from the visual bounds. When the whole collision is wider (a tree with branches), `MeasureTrunk` fires eight rays at crouch height toward the component's pivot and uses the box around the hits; the result is cached per component until it moves.
- **Re-aim** (`bM_ReaimSlantedHits`): when a grid probe hits an object at a slant (alignment below 0.8), one more probe is queued straight in front of the spot it hit. Queued probes are deduplicated per 45 cm cell and 30 degrees of direction.

Rings are not probed again on every scan. `FCoverThinObstacleCache` keeps, per obstacle (component plus instance index), its measured trunk and the unpublished candidates its ring produced. A ring is probed when the obstacle is new or has moved, and otherwise once every `M_ThinObstacleRefreshScans` scans (default 6), with the obstacles taking turns by cache id; in between, the cached candidates are sent straight to the worker (`EnqueueCandidateChunk`). Trunks are measured inside the sampling budget, not in the frame the scan starts. Entries of obstacles that no longer overlap the navigable world are dropped. The grid samples around every object and the re-aimed probes are still redone every scan.

**Room around a thin obstacle.** A pole has cover points on every side but shelters one soldier per `M_ThinObstacleWidthPerSoldier` of its width (default 70 cm, at least one). At publication `TagThinObstacleCoverPoints` gives every generated point within the obstacle's cover radius, whichever probe found it, that obstacle's `ThinObstacleId` and `ThinObstacleCapacity`. A point is unavailable to a soldier while that many others hold points with the same id (`GetIsCoverPointAvailableToUnit`, `TryReserveCoverPointById`), and the squad move planner counts planned soldiers per id the same way, after subtracting soldiers outside the selection. The TestCover scenario fails when any thin obstacle holds more soldiers than it has room for.

`RTS_COVER_AIMED_PROBES` logs the counts per scan (probed, reused, measured) and `RTS_COVER_TRUNK_MEASURED` (Verbose) each measured trunk.

### Standing space

A position only becomes cover when a soldier's body fits there (`GetCanInfantryOccupyLocation`). The body is a column of `M_StandingSpaceRadius` (25 cm) from `M_StandingSpaceFloorClearance` (60 cm) to `M_StandingSpaceHeight` (150 cm) above the ground, not the full navigation capsule. Infantry capsules are query-only and soldiers walk wherever the navmesh allows, so the full capsule rejected ground they stand on anyway: the dead trees on TestCover flare out up to 60 cm high at their foot, which removed every position around them.

### Explain at cursor (debug)

`RTS.CoverFinder.ExplainAtCursor` (or `-CoverFinderExplainAt="X=.. Y=.. Z=.."` on the command line) logs `RTS_COVER_EXPLAIN` lines and draws the probes for 15 s: published cover nearby, every obstacle with its collision size, visual size and trunk size and how it is scanned, then the result of each probe with the first check that rejected it. It lives in `CoverFinderExplain.cpp`. The command is only registered, and every function body only compiled, inside `if constexpr (GCoverFinder_Compile_DebugSymbols)`.

Debug code must sit inside the taken branch of the `if constexpr`. The early-return form (`if constexpr (not Flag) { return; }` followed by the body) leaves the body in the function and fails the debug-off build with C4702.

### Open-frame crouch cover (tank hedgehogs)

The regular crouch rule needs one continuous surface from knee to crouch height, which a frame of crossed beams never is. `SampleOpenFrameCover` is tried only by the ring probes of a thin obstacle, and only where the regular rule found no cover in that direction, so regular crouch points are unaffected. It traces through the whole obstacle at 30, 50, 70 and 90 cm; when at least two of the four hit a beam and nothing blocks the crouch firing height, it publishes a crouch point `M_OpenFrameCoverStandOff` (60 cm) short of the nearest beam, or at the ring position when a soldier does not fit closer. `bM_FindOpenFrameCover` switches it off.

### Firing through one's own cover

While a soldier holds a point, his weapon ignores every object in front of him there that does not belong to an enemy (`IgnoreCoverObstaclesWithWeapon`): the point's own cover object plus whatever else overlaps a box between him and the cover (`GatherCoverObstacleActors`), because a crystal cluster or a wreck with loose parts is several actors. The ignores are removed when he leaves the point. Before, only cover owned by his own player was ignored, so neutral objects such as radixite crystals were hit.

### Cover from sockets

Some actors carry their cover on mesh sockets instead of having it found by the scan: trenches, whose collision is a plane that lets tanks drive over them, and hand-placed cover such as sandbag walls. `M_SocketCoverActorClasses` in the cover settings (section "Cover From Sockets") lists the parent classes, by default `BP_TrenchMaster` and `BP_NaturalCover`. On the first tick of a map `RegisterSocketCoverOnce` finds every actor of a listed class or a class derived from one, takes its first mesh component that has sockets (an empty inherited mesh component and the health bar widget are passed over), and turns each socket into a cover point by its name:

| Setting | Default | Cover type |
| --- | --- | --- |
| `M_TrenchCoverSocketNamePart` | `cargo` | `TrenchStandUp` |
| `M_CrouchCoverSocketNamePart` | `crouch` | `Crouch` |
| `M_StandingLeftCoverSocketNamePart` | `standing` | `StandingLeft` |
| `M_StandingRightCoverSocketNamePart` | `standing_right` | `StandingRight` |
| `M_ProneCoverSocketNamePart` | `prone` | `Prone` |

Letter case does not matter. A name that contains more than one part gets the longest one (`FCoverFinderAlgorithms::TryGetSocketCoverType`), which is why `standing_right` is a right-peek point although it contains `standing`. Sockets with none of the parts are ignored. The soldier faces along the socket's forward (X) axis, and the point is moved onto the navmesh when that is close by.

The points go through the authored-provider path: they win a spacing conflict with an equivalent scanned point, two sockets of the same type facing the same way closer than `M_CoverPointSpacing` (90 cm) merge into one, and socket prone points are never thinned. When the actor is destroyed its points are taken out again (`RemoveSocketCoverOfDestroyedActors`). With `LogRTSCoverFinder` on Verbose, `RTS_COVER_SOCKET` lists every socket point and `RTS_COVER_SOCKET_NONE` every listed actor that gave none.

### Trench cover

Trench points come from sockets, see above. They draw red.

A trench point behaves like standing cover with no sideways step: protected is a crouch below the edge (`ESquadIdleAnimationPose::TrenchCover`), exposed is standing up in place (`TrenchPeek`), with zero enter and exposed offsets. The assets live in `CoverAnimations.Trench` on the infantry animation Blueprint, a `FSquadUnitStandingCoverAnimationSet`: `ProtectedIdlePose` for the crouch, `PeekAimAssets` for the standing aim, and optional enter, expose, return and exit montages. Left empty, the crouch falls back to `Crouch.ProtectedIdlePose` and the exposed pose to the regular standing aim (`ESquadCoverGraphPose::NotInCover`), so it works before any trench asset is assigned.

### Duck to reload

`CoverAnimations.CoverReload` on the infantry animation Blueprint is a full-body montage of a crouched soldier reloading. `PlayReloadAnim` plays it, scaled to the weapon's reload time, instead of the weapon's regular reload montage whenever the unit holds a cover pose; left empty, the regular montage is used. The weapon reports its reload to the soldier (`ASquadUnit::OnWeaponReloadStarted`), not straight to the animation instance: a soldier that is exposed from standing or trench cover first returns to its protected pose, and `ExposeFromStandingCover` keeps it there until the reload time has passed. While the return montage plays, the reload clip waits and then starts for the time that is left (`TryPlayPendingCoverReload`). Leaving cover stops the clip.

### Prone cover

`ERTSCoverType::Prone` is cover a soldier lies behind: a bump in the landscape or a low object. It draws purple in the cover debug view; the squad move preview shows it with the prone stance mesh (`ESquadPlannedPositionType::ProneCover`). Like crouch cover it fires from where it is (`RTSCoverTypes::GetFiresFromProtectedPose`).

**Finding it.** Every probe direction now starts with the knee-height probe (30 cm), because every kind of cover needs something there; open ground still costs one probe per direction. Where that probe hits something that is no crouch cover, one more probe at `M_ProneCoverMaximumHeight` (60 cm) decides (`FCoverFinderAlgorithms::GetIsProneCoverEvidence`): the face must be steeper than `M_ProneCoverMinimumSlopeDegrees`, and nothing may rise above the fire-over height for 120 cm behind it, which tells a bump from the foot of a hillside or a wall. The soldier lies `M_ProneCoverStandOff` from the face; that spot is moved onto the navmesh and checked for room with a low capsule along the body (`GetCanInfantryLieAt`). The landscape scan finds bumps, the environment scan low objects, with the same code. Objects between knee and crouch height that are thin also get the ring of probes, since the grid easily steps over small ones.

**Keeping it sparse.** Found prone points keep `M_ProneCoverPointSpacing` (500 cm) from each other and the regular point spacing from every other point (`AppendSpacedPronePoints`). A scan keeps all it finds; the thinning happens at publication, where points published before go first. Without that, which of two close candidates survives changed from scan to scan and soldiers kept losing their point.

**Companions.** `M_ProneCompanionChancePercent` (15) of the crouch and standing points get a prone point `M_ProneCompanionOffset` (170 cm) to the side, facing the same way within 15 degrees (`TryBuildProneCompanion`). The choice follows from the point's ID, so it is the same on every scan. Each is checked against the world once and cached (`M_ProneCompanionCache`), at most 32 new ones per publication.

**Using it.** Prone points have their own share of the firing-lane tests of a cover search (4, next to the 8 of the other types), so rolling ground full of prone points cannot push crouch and standing candidates out. The lane is tested from 70 cm. A prone point takes none of a thin obstacle's capacity.

**Animation.** `CoverAnimations.Prone` on the infantry animation Blueprint is a `FSquadUnitCrouchCoverAnimationSet`: `ProtectedIdlePose` (lying, head down), `AimAssets` (a non-additive prone aim pose as `BaseSequence`, its aim offset, optional fire montages), optional enter and exit montages and `EnterStartOffset`. `ProneCoverReload` is the lying reload; empty, `CoverReload` is used. While `Prone.ProtectedIdlePose` is empty a soldier at a prone point uses the whole Crouch set. With it assigned, the weapon's prone fire montages are used (the crouch ones where a weapon has none). The graph needs no new nodes: prone goes through the same `CoverIdle` and `CoverAim` branches.

`RTS_COVER_PERF` reports `prone`, `prone_lost` (prone points of the previous publication that are gone) and `prone_companions`. The count test also logs `reachability` per cover type.

### Stance meshes of the squad move preview

The squads-only move preview shows every planned soldier position as a stance mesh, turned to the facing the soldier will have there. There is no debug drawing left in it.

- `FSquadMovePreviewStances` (`Player/SquadMovePreview/SquadMovePreviewStances.h`) owns one instanced static mesh component per stance: no cover, crouch cover, high cover (both standing sides), prone cover and trench cover. The stance of a position follows from its cover type (`GetStanceForPosition`).
- The components sit on an actor of their own. They cannot sit on the player controller, which owns the preview component: a controller is a hidden actor, and nothing a hidden actor owns is drawn.
- Each component starts with `M_PreviewStancePreloadCount` (32) parked instances, so the preview allocates nothing while the cursor moves. A selection that needs more of one stance grows that stance's pool once. An instance costs a transform and a few bytes of bookkeeping; the 160 preloaded ones are a few kilobytes.
- Instances are written only when the plan changes (`ShowPlan`, one batched transform update per stance that is in use), never per frame. Hiding the preview only switches the components' visibility.
- The meshes have no collision, cast no shadow, take no decals and do not affect navigation.
- The meshes and the preload count are set in Project Settings > Cover Finder > Squad Move Preview. `M_PreviewStanceYawOffsetDegrees` (-90) turns them: the stance meshes were made from character poses and look along their Y axis.

`-SquadMovePreviewValidate` checks that every planned position has a mesh of its stance on it, looking along its facing, and that the meshes go away with the preview. Adding `-SquadMovePreviewCaptureStances` to a rendered run saves two pictures of the preview to `Saved/CoverFinderDebug/SquadPreviewStances_*.png`.

### Taking planned cover on arrival

A soldier that the squad move preview gave a cover position takes that cover the moment it gets there, without waiting for its squad. The move order's own walk is the cover approach; there is no second walk.

- `ExecutePlannedMove` reserves the planned point and assigns it at once (`TryAssignPlannedCoverForWalk`): cover state `MovingToCover`, with `bWalkBelongsToMoveCommand` set. A point that is still held, usually by a squad mate about to leave it under the same order, is tried again from the tactical update while the unit walks. A waypoint with more movement queued behind it gets no cover.
- While that flag is set the cover layer starts no walk and runs no deadlines of its own; the order's walk keeps its own watchdog (`UpdatePlannedMoveArrival`).
- When the order's walk ends, the unit enters cover first and reports the order done second (`EnterPlannedCoverAfterWalk`, then `OnCommandComplete`). The order matters for the last unit: its report ends the squad's command, and that must find it in cover, not on its way. A walk that stopped short hands over to the regular cover walk for the rest.
- A unit that finished ahead of its squad keeps its cover while the squad is still on that move order (`GetMayKeepAutomaticCover`), and the end of the order no longer throws it out: `CancelCoverWalkForTerminatedMovement` only ends cover the unit is still walking to.
- Player orders keep priority through the rule that was already there: whatever the squad does next cancels cover itself when it sets the unit moving, so a unit that is entering or holding cover joins the next move at once.

An approach move (below) may replace the last stretch of the order's walk. It then stands in for that walk: the order is reported done when the move is over (`bReplacedMoveCommandWalk`), also when the cover is lost in the middle of it, and never when a new order was what interrupted it.

### Sliding and rolling into cover

`CoverAnimations.ApproachMoves` on the infantry animation Blueprint lists full-body root-motion moves, such as a slide and a roll, that a soldier may finish its run to crouch or prone cover with. Each entry takes a root-motion sequence or a montage; a sequence is played on `ApproachMoveSlotName` ("FullBody"). Standing and trench cover never use them. `M_CoverApproachMoveChancePercent` (30) in the cover settings is how often a cover assignment asks for one; one of the entries is then picked at random.

The move only plays when it will end on the cover point:

- Its travel is measured from the clip's root track (`GetCoverApproachMoveTravelFrom`). A montage stops moving the capsule the moment it starts to blend out, so the travel of that last stretch is left out.
- While walking, the unit checks 60 times a second (`TickCoverApproachWatch`) and starts the move when its distance to the cover point is within 15% of that travel. It turns so the clip's travel, sideways drift included, points at the point, and scales the root motion to the exact distance.
- It must be running straight at the point, not from the enemy's side of the cover, on level ground, with the whole line on the navmesh and nothing solid on it (`GetCanApproachCoverInStraightLine`), and it must be on screen. The walk must be at least one and a half times the clip's travel long.
- While the move plays, the rest of its travel is rescaled every tick to the distance still to go (`CorrectCoverApproachMoveTravel`), and the movement component ticks every frame. With the coarse movement tick the unit optimizer gives unseen soldiers, the clip ran up to twice as fast and lost half its travel.

Where the move ends, the soldier takes its cover pose at once: the enter montage is skipped (`EnterCover(..., bSkipEnterMontage)`). A move that was cut short leaves the soldier to walk the rest. Weapon montages and stance transitions do not start during a move. `RTS_COVER_APPROACH_MOVE started` and `ended` (Verbose) log the distance, the scale and how far from the point the move ended. `-CoverFinderApproachAnims=PathA+PathB` on the TestCover and squad preview scenarios gives every soldier the named clips without touching the Blueprint. `-SquadMovePreviewWatch` makes a rendered squad preview run send its squad to low cover and look at it, so the moves can be seen on a player move order.

### Staying put in cover

Several things used to make a soldier give up or step out of cover it was about to take again. Each has its own guard:

- **Between two targets.** A soldier that stepped out of standing or trench cover and whose weapon has no target, or swapped to an enemy this point cannot reach, holds its firing position without firing for `M_StandingCoverTargetLossHoldSeconds` (2 s) before it steps back (`HoldExposedCoverWithoutUsableTarget`). Once back, it stays hidden for at least `M_StandingCoverMinimumHiddenSeconds` (1 s). An attack order skips both waits.
- **Squad closing range.** One soldier out of range of an attack order moves the whole squad closer. A soldier in cover whose own weapon has the ordered target in range is left where it is (`GetCanHoldCoverWhileSquadClosesRange`, checked in `ASquadController::GeneralMoveToForAbility`). A soldier that is out of range, or cannot engage the ordered target from its point, still moves: the order comes first.
- **Attack order not passed on yet.** A unit that is still idle while its squad already executes an attack order keeps its cover (`GetMayKeepAutomaticCover`).
- **Point missing from one scan.** A cover scan misses a point when another soldier walks through it. The occupant keeps its point and its reservation for `M_CoverPointLossToleranceSeconds` (12 s) before it leaves; cover whose object no longer exists is given up at once (`GetHasLostCoverPoint`).
- **Weapon handed over.** A dying squad mate's weapon goes to a survivor. `SetWeaponAimOffset` no longer cancels the cover pose for that.

`RTS_COVER_RETURN` (Verbose) logs why a soldier stepped back, `RTS_COVER_MOVE_CANCEL` which movement entry point took a soldier out of cover, and `RTS_COVER_KEEP_DENIED` why cover was no longer allowed.

### Cover animation asset checks

Two setup mistakes on the animation Blueprint are reported once per class through `ReportError`:

- An aim `BaseSequence` that is an additive aim-offset sample instead of a pose. Played as a pose it scales every bone to zero and the soldier vanishes while aiming. `GetUsableCoverAimBaseSequence` uses the pose the sample was made additive against instead; when there is none, crouch cover stays in its idle crouch.
- A `ProtectedIdlePose` whose root travels more than 10 cm. That is a transition clip; looped as an idle it makes the soldier step in and out of its cover for as long as it hides. Code cannot fix this one.

`RTS_COVER_AIM_ASSETS` (Verbose, once per animation class) lists every cover and regular aim offset with its base pose and samples.

### Cover tests on TestCover

- `-CoverFinderCloseUps` on the TestCover scenario logs `RTS_COVER_POSE` once a second for every soldier in cover (head and pelvis height above the feet, how far the pose carries the body off the capsule, mesh visibility, what drives the pose) and saves close-up pictures of player soldiers in cover to `Saved/CoverFinderDebug/CloseUps`. Needs `-RenderOffscreen`. A head height of a few centimetres means the pose collapsed.

- `-CoverFinderCountCover` logs the published points per object class and cover type and compares them with `Navigation/CoverFinder/Tests/Baselines/<Map>.txt`; `RTS_COVER_COUNT_TEST RESULT FAIL` means cover was lost. `-CoverFinderWriteCountBaseline` lowers the baseline to the run's counts where they are lower (write it several times: objects on the map are placed with some randomness), `-CoverFinderResetCountBaseline` starts it afresh.
- `-CoverFinderValidateTrenchCover` sends a squad into a trench and checks crouch, stand up against an enemy in front, no movement while standing up, firing through the trench, and crouching again.
- The idle phase of `-CoverFinderValidateTestCover` fails when a unit in cover would hit its own cover or a thin obstacle holds more soldiers than it has room for.

### First scan budget

Nothing can take cover, and no cover is drawn, until the first full scan of a map is published. That scan uses `M_FirstScanGameThreadBudgetMilliseconds` (default 4 ms per frame) instead of the 0.35 ms refresh budget, and the per-frame query cap is only a safety limit now. On TestCover the first scan dropped from about 1000 frames to about 35.

Tests: `RTS.SquadMovePlanner.*` (8 planner tests) and the map scenario `-SquadMovePreviewValidate` on TestCover, which logs `RTS_SQUAD_PREVIEW_TEST RESULT PASS|FAIL` after checking crouch, standing and open-ground plans, the arrow filter, several squads at once, replanning cost, and that an issued plan is walked to.

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
