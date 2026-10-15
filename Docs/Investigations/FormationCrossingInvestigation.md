# Formation Crossing Investigation (Same-Priority Units)

## Observed flaw

When multiple units share the same unit type + subtype (therefore same priority bucket), the assignment stage can remap units to slots in a way that does not preserve their original lateral ordering. In practical terms this can swap the left and middle vehicles, creating unnecessary crossing paths.

## Why this happens in the current code

### 1) Row ordering is computed, then effectively overridden later

- Rectangle generation explicitly sorts each row by original lateral position (`SortRowUnitsByOriginalPosition`) so units keep side coherence before slot generation.
- However, after all final slot positions are generated, `BuildUnitAssignments` re-groups by type/subtype and sorts **again** with a different comparator (`UnitSortPredicate` / `SlotSortPredicate`).
- That second pass sorts by `AbsLateralOffset` first, which naturally favors center-near entities over left/right side continuity.

This second pass can therefore invalidate the earlier row-order intent and produce swaps among equal-priority units.

### 2) Unit and slot sort keys are not the same semantic objective

- Units are sorted by forward distance, then absolute lateral distance, then signed lateral.
- Slots are sorted by front priority, then absolute lateral, then signed lateral.

Even though both look similar, they are derived from different anchor semantics (unit originals vs generated slot geometry), and `AbsLateralOffset` as a primary tiebreak strongly biases center-first pairing instead of left-to-right stability.

### 3) Final per-actor assignment only occurs after this re-sort

The deterministic actor cache (`M_AssignedFormationSlots`) is filled only after the re-sorted pair matching is done. So once a mismatch is created in that phase, the actor-level replay path remains consistently wrong for that command.

---

## Three implementation strategies to minimize crossings

## Strategy 1 (Lowest risk): Stable side-preserving matching inside each priority bucket

For each `(UnitType, UnitSubType)` bucket:

1. Compute scalar lateral coordinate for every unit original location in formation space.
2. Compute scalar lateral coordinate for every candidate slot.
3. Sort **both arrays by signed lateral** (left -> right).
4. Pair by index.
5. Use forward distance only as a secondary tie-break when lateral is almost equal.

### Why this helps

- Preserves left/middle/right identity directly.
- Eliminates center-first bias from `AbsLateralOffset`.
- Minimal architectural impact (can be localized to `BuildUnitAssignments`).

### Trade-offs

- Not globally optimal in irregular layouts, but very reliable for your reported case (3 same vehicles moving mostly forward).

---

## Strategy 2 (Best quality): Cost-matrix assignment (Hungarian / min-cost bipartite)

For each `(UnitType, UnitSubType)` bucket with `N` units and `N` slots:

1. Build an `N x N` cost matrix:
   - `Cost = wLateral * abs(LateralUnit - LateralSlot) + wForward * abs(ForwardUnit - ForwardSlot) + wDistance * Distance2D(UnitPos, SlotPos)`
2. Solve minimum-cost one-to-one assignment.
3. Store actor->slot mapping in `M_AssignedFormationSlots`.

### Why this helps

- Directly optimizes “least movement distortion”, which correlates strongly with fewer crossings.
- Works for non-rectangular formations and uneven spacing.

### Trade-offs

- More code complexity and maintenance.
- Slightly higher CPU cost (still fine at RTS selection sizes if scoped per subtype bucket).

---

## Strategy 3 (Most robust for gameplay): Crossing-penalty post-pass on top of current deterministic assignment

1. Keep existing deterministic pairing as initial solution.
2. Build line segments from each unit original location to assigned slot.
3. Detect pairwise segment intersections inside each same-priority bucket.
4. Iteratively swap slot assignments between intersecting pairs if total crossing count decreases (or total cost decreases).
5. Stop when no improving swap exists.

### Why this helps

- Directly optimizes what the player visually perceives: crossing trajectories.
- Can be added incrementally without replacing your current system.

### Trade-offs

- Heuristic (not guaranteed globally optimal).
- Requires robust 2D segment intersection helper in formation plane.

---

## Recommendation order

1. **Implement Strategy 1 first** for a fast and safe fix to the reported left/middle swap bug.
2. If additional edge cases remain, evolve to **Strategy 3** as a surgical enhancement.
3. If you want mathematically best assignment quality across all formations, move to **Strategy 2**.

## Optional debug instrumentation to validate improvements

- Log per-unit tuple: `(ActorName, OriginalLateral, AssignedSlotLateral, BucketType/Subtype)`.
- Log crossing count per command before issuing movement.
- Add a debug draw line from original to assigned slot color-coded by bucket.

This will make regression testing very quick when testing vehicle platoons.

---

## Follow-up issue: 2x2 rectangle row inversion within same priority

### Symptom

With four same-priority units in rectangle formation, the two units that started in the back row can sometimes
take the front-row slots, while front-row units take back-row slots.

### Root cause in current implementation

After generating rectangle rows, `BuildUnitAssignments` performs a bucket-level re-sort for all units and slots
of the same type/subtype. The current matching logic sorts by lateral offset first, and only then uses a
forward/depth tie-break. This means row-membership is no longer explicitly preserved once units are flattened
into a single subtype bucket.

When depth values are close/noisy (or movement/override orientation changes the projection behavior), that
secondary tie-break can produce front/back inversions even though lateral side ordering is preserved.

### Two good solution options

#### Option A (recommended): Row-aware two-stage assignment

1. Keep current side-preserving sort as stage 1.
2. Before pairing, partition each subtype bucket into row bands using forward projection of slot positions
   (front row band, second row band, etc.).
3. Partition units into the same number of row bands using forward projection from original locations.
4. Match row band N units only to row band N slots.
5. Inside each row band, sort by signed lateral offset and pair by index.

**Why this works**
- Explicitly prevents cross-row swaps.
- Keeps the existing deterministic behavior and only adds a row constraint.
- Lowest risk incremental change to current architecture.

#### Option B: Min-cost assignment with explicit row-swap penalty

1. Build full cost matrix per subtype bucket.
2. Include terms for:
   - lateral mismatch,
   - forward mismatch,
   - Euclidean travel distance.
3. Add a large penalty term when a unit likely from back-row is mapped to front-row (and vice versa),
   based on forward-rank buckets.
4. Solve with Hungarian (or min-cost matching).

**Why this works**
- Handles irregular layouts better than hard row partitioning.
- Lets you tune “strictness” of row preservation by penalty weight.
- Can evolve into a globally better anti-crossing solver for all formations.
