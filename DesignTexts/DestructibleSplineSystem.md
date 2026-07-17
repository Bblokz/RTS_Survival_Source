# Destructible Spline System — Design Document

> Status: IMPLEMENTED (Phase 1+2 core, 2026-07-16) — see Source/RTS_Survival/Environment/Splines/DestructibleSpline/
> Created: 2026-07-16, research + codebase analysis
> Target engine: Unreal Engine 5.5 (source build), module `RTS_Survival`
> Related code: `Environment/Splines/RoadSplineActor`, `Environment/DestructableEnvActor/*`, `Collapse/*`, `Utils/CollisionSetup/*`

---

## Design Decision (§15 — Finalized)

### Per-System Destruction Configuration (v1)

**All pieces on one `ADestructibleSplineActor` share the same destruction type and settings.**

- Drop a spline with 100 segments → all 100 pieces sink vertically using the same `VerticalCollapseSettings`, FX, etc.
- Alternatively, all 100 fracture into the same geometry collection (if `DestructionType = GeometryCollectionCollapse`).
- **If mixed destruction is needed later** (wooden posts sink, concrete pylons fracture): author pieces on **separate spline actors**.
- Per-piece overrides can be added as a **phase-4 extension** if gameplay demands it.

This simplifies v1 and keeps the blueprint interface clean. One actor = one destruction recipe, applied uniformly to all its pieces.

---

## 1. Summary

We want a **custom spline system** whose repeated mesh runs forward along world **+X** (typical spline mesh), where **each individual mesh piece on the spline** independently supports:

1. **Collision** (and optionally **overlap**) using the **exact same settings** as crushable `ADestructableEnvActor` meshes.
2. Two kinds of **destruction, per piece** (not per whole spline):
   - **Vertical collapse** — piece removed, temporary actor spawned at same transform runs vertical-collapse animation (with FX).
   - **Collapse-mesh (Chaos geometry collection)** — piece removed, temporary actor spawned at same transform swaps to Geometry Collection and simulates fracture (with FX).
3. **Crushable-by-vehicles** behaviour identical to destructible env actors (nav-agent gated overlap → destruction).

Must stay **performant for large splines with many pieces** (hundreds to low thousands of segments).

---

## 2. Architecture Decision

### Primary: Per-Segment `USplineMeshComponent` (§5–7)

**Why**: Each segment is already a `UPrimitiveComponent`, so per-piece collision, per-piece overlap events (crucial for crush!), and per-piece transform tracking are **native and free**. Reuses existing collision and collapse code **unchanged**.

| Capability | Result |
|---|---|
| C1: Collision for traces | ✔ Native. Call `FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision` per segment. |
| C2: Overlap events for crush | ✔ Native & per-piece: `OnComponentBeginOverlap` fires on that component; identifies which piece to destroy. |
| C3: Transform + removal | ✔ `GetComponentTransform()` per segment; `DestroyComponent()` removes one piece. |
| Curve deformation | ✔ True spline-mesh bending. |

**Cost**: N primitive components = N scene proxies + N bodies + N cooked collisions. §8 mitigates this (async cooking, distance gating, pooling, Nanite, hybrid for largest scenes).

### Fallback: ISM-Derived (§9)

For **very large rigid non-curved piece counts**, use `URTSHidableInstancedStaticMeshComponent`-derived component: excellent render/trace scaling but **no native per-instance overlap events** (the single blocker for crush). Documented as a scale option, not primary path.

---

## 3. Core Classes

### `ADestructibleSplineActor : public AActor`

Owns `USplineComponent` (root) and builds one `USplineMeshComponent` per segment, mirrors `ARoadSplineActor::BuildRoad()`.

**Per-system (Blueprint) configuration** (shared by all pieces):
```cpp
// Geometry
UPROPERTY(EditAnywhere) UStaticMesh* PieceMesh = nullptr;
UPROPERTY(EditAnywhere) UMaterialInterface* PieceMaterialOverride = nullptr;

// Collision / crush (R2/R6)
UPROPERTY(EditAnywhere) bool bCrushableByVehicles = true;
UPROPERTY(EditAnywhere) ECrushDestructionType CrushDestructionType = ECrushDestructionType::AnyTank;

// Destruction (R4/R5) — all pieces use these
UPROPERTY(EditAnywhere) ESplineDestructionType DestructionType = ESplineDestructionType::VerticalCollapse;
UPROPERTY(EditAnywhere) FRTSVerticalCollapseSettings VerticalCollapseSettings;  // if VerticalCollapse
UPROPERTY(EditAnywhere) TSoftObjectPtr<UGeometryCollection> PieceGeometryCollection;  // if GeometryCollectionCollapse
UPROPERTY(EditAnywhere) FCollapseForce CollapseForce;      // geo impulse
UPROPERTY(EditAnywhere) FCollapseDuration CollapseDuration; // geo lifetime
UPROPERTY(EditAnywhere) FCollapseFX CollapseFX;             // shared FX contract (both types)

// Editor
UFUNCTION(CallInEditor) void RebuildSpline() { BuildSpline(); }
```

**Key methods**:
- `BuildSpline()` — one `USplineMeshComponent` per segment; apply collision; wire crush handlers; cache deform params.
- `ApplyPieceCollision(USplineMeshComponent*)` — call `FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision(Seg, bCrushableByVehicles)`.
- `BindCrushOverlap(USplineMeshComponent*)` — bind `OnComponentBeginOverlap` to per-piece handler.
- `DestroySegment(int32 SegmentIndex)` — guard + mark `bIsCollapsing`, disable collision, spawn `ASplinePieceCollapseProxy` at piece transform, hide segment.

### `ASplinePieceCollapseProxy : public AActor`

Tiny temporary actor. Its only job: be the owner that collapse utilities need.

**Public entry points**:
```cpp
void StartVerticalCollapse(
	UStaticMesh* Mesh, UMaterialInterface* MaterialOverride,
	const FVector& StartPos, const FVector& StartTangent,
	const FVector& EndPos,   const FVector& EndTangent,
	const FRTSVerticalCollapseSettings& Settings, const FCollapseFX& FX);

void StartGeometryCollapse(
	UStaticMesh* SourceMesh,
	TSoftObjectPtr<UGeometryCollection> GeoCollection,
	const FCollapseDuration& Duration, const FCollapseForce& Force, const FCollapseFX& FX);
```

**Key idea**: Proxy creates a deformed `USplineMeshComponent` (for vertical path) or `UGeometryCollectionComponent` + `UStaticMeshComponent` (for geo path), then calls the existing static collapse utilities (`FRTS_VerticalCollapse::StartVerticalCollapse`, `FRTS_Collapse::CollapseMesh`). No gameplay logic, no health, no aim-offset — just the owner.

---

## 4. Destruction Flows

### Vertical Collapse (R4)

```
1. Guard (bIsCollapsing), unbind overlap, disable segment collision
2. Read segment world transform + cached deform params
3. Hide/destroy USplineMeshComponent
4. Spawn ASplinePieceCollapseProxy at segment transform
5. Proxy creates identical deformed USplineMeshComponent (no pop)
6. Proxy calls FRTS_VerticalCollapse::StartVerticalCollapse(proxy, Settings, FX)
7. Proxy self-destructs on finish (bDestroyPostVerticalCollapse = true)
```

**Visual fidelity**: Proxy replicates exact `SetStartAndEnd` deform params (cached per segment) → pixel-identical bent shape, zero pop.

### Geometry-Collection Collapse (R5)

```
1–4. Same guard/hide/spawn as vertical
5. Proxy owns UGeometryCollectionComponent + UStaticMeshComponent (MeshToCollapse)
6. Proxy calls FRTS_Collapse::CollapseMesh(proxy, GeoComp, GeoCollection, MeshToCollapse, Duration, Force, FX)
7. FRTS_Collapse async-loads geo, hides source mesh, enables physics, applies radial impulse, plays FX
8. Proxy lifetime bounded by CollapsedGeometryLifeTime; destroy proxy after
```

**Limitation**: Geometry collection is authored from **base straight mesh**. On curved segments, fracture doesn't follow bend — acceptable (same as any mesh→geo swap in the project). For tight curves, use `VerticalCollapse` or shorter segments.

---

## 5. Crush Wiring (R6)

**Reuse env-actor nav-agent gate + enums**, destroy only the overlapped piece:

```cpp
void ADestructibleSplineActor::HandlePieceBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, ...)
{
	if (not IsValid(OtherActor)) return;
	
	const int32* SegmentIndex = M_SegmentIndexByComponent.Find(OverlappedComponent);
	if (not SegmentIndex) return;
	
	// Same gate: IRTSNavAgentInterface, ERTSNavAgents, ECrushDestructionType (AnyTank/MediumOrHeavy/HeavyTank)
	if (not PassesCrushGate(OtherActor, CrushDestructionType)) return;
	
	DestroySegment(*SegmentIndex);  // per-piece; uses this system's DestructionType + settings
}
```

**Future extract**: `FRTSCrushGate::Passes(OtherActor, ECrushDestructionType)` so both `ADestructableEnvActor` and this system call one implementation.

---

## 6. Performance & Scale (R9)

### Component Cost Mitigation

1. **Collision cooking**:
   - Author simple collision (box/convex) on `PieceMesh`; avoid `UseComplexAsSimple`.
   - Set `Seg->bUseAsyncCooking = true` before `SetStartAndEnd` (cook off game thread).
   - Defer collision on far pieces (build with `NoCollision`, enable within radius).
   - Non-crushable pieces use `QueryOnly` (no physics body).

2. **Rendering**:
   - UE 5.4+ **Nanite spline meshes** (`r.Nanite.AllowSplineMeshes=1`) decouple triangle cost from segment count (opt-in; validate on target HW).
   - Non-Nanite: set `CastShadow` off where unneeded; rely on distance culling.

3. **Pool the proxies**:
   - **Vertical proxies**: cheap (one spline mesh + timer) — large pool fine.
   - **Geo proxies**: expensive (geometry component + async load + Chaos sim) — maintain a smaller pool and cap concurrent sims (e.g., 4 active fractures; beyond cap, fall back to vertical/hide).

4. **Distance/relevance gating**:
   - Enable per-segment collision/overlap only within radius of player + vehicles.
   - Toggling `SetCollisionEnabled` / `SetGenerateOverlapEvents` is scalar, no rebuild.
   - **Single biggest runtime win for very large splines.**

5. **Hybrid for extreme counts**:
   - **Crushable/curved/critical** pieces → per-segment `USplineMeshComponent` (full C1/C2/C3).
   - **Rigid non-crushable trace-only** bulk → ISM (component-level collision, per-instance trace hits, no crush).
   - Keeps component count ∝ crushable subset; rest rides GPU instancing.

6. **Build-time amortization**:
   - Build at construction (not `BeginPlay`); bake into level.
   - If runtime build unavoidable: amortise across frames (M segments/frame).

### Memory & Teardown

- Store per-segment data: `TArray<FSplineDestructibleSegment>` + `TMap<UPrimitiveComponent*, int32>` (crushable only).
- On `EndPlay`, destroy all components, drain proxy pool (mirror `UDestructibleWire` teardown).
- Soft-reference geometry collection (`TSoftObjectPtr`) — async-loads on demand, idle splines don't keep fractured assets resident.

---

## 7. Testing Plan

1. **Build correctness** — N-point spline → N-1 pieces, forward +X, deformed, destructible collision profile ✓
2. **Weapon interaction** — traces hit one piece; visibility/click via `ECC_Visibility` ✓
3. **Crush gates** — Heavy/Medium/Any tank nav agents; verify only overlapped piece destroys ✓
4. **Vertical collapse** — proxy spawns at exact transform, sinks per settings, FX plays, proxy self-destructs, no pop ✓
5. **Geo collapse** — geo async-loads, fractures with impulse, FX plays, cleaned up after lifetime ✓
6. **Scale** — 1k-segment spline build time, steady-state overlap cost (gated/ungated), 50-piece mass-crush (pooled/unpooled) ✓
7. **Teardown** — `EndPlay` mid-collapse destroys proxies, clears timers, no leaks ✓
8. **E2E** — drive actual flow in PIE with `/verify` style ✓

---

## 8. Implementation Phases

- **Phase 1** — Core: `ADestructibleSplineActor` build + collision + `ASplinePieceCollapseProxy` + both destruction flows (no crush yet). Deliverable: shoot a piece → collapses with FX at correct transform.
- **Phase 2** — Crush + polish: per-piece overlap crush (with `FRTSCrushGate` extraction), `bIsCollapsing` guards, BP events, preview.
- **Phase 3** — Scale: proxy/geo pooling, distance gating, async cooking, Nanite validation, ISM hybrid.
- **Phase 4** — Optional: per-piece overrides; generalise `FRTS_VerticalCollapse` to `USceneComponent*` target (drop vertical proxy).

---

## 9. File Layout

```
Environment/Splines/DestructibleSpline/
  DestructibleSplineActor.h / .cpp
  SplinePieceCollapseProxy.h / .cpp
  ESplineDestructionType.h
  FSplineDestructibleSegment.h  (or nested in DestructibleSplineActor.h)
  DestructibleSplineISMComponent.h / .cpp  (phase 3, fallback)
```

Optional shared extraction: `Environment/DestructableEnvActor/CrushDestructionTypes/FRTSCrushGate.h` (used by both actor classes).

---

## 10. Reused APIs (Cheat Sheet)

| Need | Symbol | Notes |
|---|---|---|
| Per-piece collision (R2) | `FRTS_CollisionSetup::SetupDestructibleEnvActorMeshCollision(UMeshComponent*, bool bOverlapTanks)` | Exact same responses as env actor |
| Geo collision reset | `FRTS_CollisionSetup::SetupDestructibleEnvActorGeometryComponentCollision(UGeometryCollectionComponent*)` | Physics off until FRTS_Collapse enables |
| Vertical collapse (R4) | `FRTS_VerticalCollapse::StartVerticalCollapse(AActor*, const FRTSVerticalCollapseSettings&, const FCollapseFX&, TFunction<void()>)` | Sinks the actor |
| Geo collapse (R5) | `FRTS_Collapse::CollapseMesh(AActor*, UGeometryCollectionComponent*, TSoftObjectPtr<UGeometryCollection>, UMeshComponent*, FCollapseDuration, FCollapseForce, FCollapseFX)` | Async-loads geo, impulse, timed cleanup |
| Crush enums | `ECrushDestructionType`, `ECrushDeathType` | Reuse directly (no new types needed) |
| Nav-agent gate | `IRTSNavAgentInterface::GetRTSNavAgentType()` | Port the `GetIsOverlap*Tank` logic |
| Spline reference | `ARoadSplineActor::BuildRoad()` | One USplineMeshComponent per segment, SetStartAndEnd, ForwardAxis=X |

---

## 11. Edge Cases & Mitigations

| Risk | Mitigation |
|---|---|
| Double destruction (crush + weapon same frame) | `bIsCollapsing` guard; unbind overlap + disable collision before spawning proxy |
| Visual pop on vertical collapse | Proxy replicates exact SetStartAndEnd deform params (cached per segment) |
| Geo fracture doesn't follow curve | Documented limitation; use VerticalCollapse or shorter segments for tight curves |
| Overlap fires for non-vehicles | Reuse nav-agent gate exactly as env actor does |
| Build spikes on huge splines | Build at construction (not BeginPlay); async cooking; defer far collision; amortise if runtime |
| Mass-destruction churn | Proxy + geo pooling; cap concurrent Chaos sims |
| EndPlay leaks | Guarded teardown like UDestructibleWire; clear proxy timers like ADestructableEnvActor |
| Nav mesh not updated | Pieces set SetCanEverAffectNavigation(false); rely on BP nav modifier or invalidate nav tiles on destruction |
| Nanite culling cost | Opt-in r.Nanite.AllowSplineMeshes; validate on target HW; moderate curvature |

---

## 12. Research References

- ISM component-level collision/material/shadow: https://dev.epicgames.com/documentation/en-us/unreal-engine/instanced-static-mesh-component-in-unreal-engine
- Per-instance overlap identity (MultiBodyOverlap, OtherBodyIndex): https://forums.unrealengine.com/t/get-overlapping-index-of-hierarchical-instanced-static-mesh/461890
- ISM overlap event limitation: https://forums.unrealengine.com/t/add-onbeginoverlap-an-onendoverlap-event-to-instanced-static-mesh/2724810
- Trace hits instance index (HitResult.Item): https://forums.unrealengine.com/t/get-index-of-collided-instanced-static-mesh/466536
- Nanite spline mesh (UE 5.4+): https://forums.unrealengine.com/t/nanite-and-spline-mesh-component/269688
- Nanite roadmap: https://portal.productboard.com/epicgames/1-unreal-engine-public-roadmap/c/1252-nanite-spline-mesh
- Collision authoring best practices: http://www.freeformlabs.xyz/blog/ue5-antipractice-mesh-collision
- Async cooking caveats: https://forums.unrealengine.com/t/collision-data-isnt-removed-after-clearing-a-section-if-buseasynccooking-true/268178

---

**Document complete. Ready for implementation.**
---

## 13. Health & Damage (added 2026-07-16)

Per-piece health, per-system configuration (consistent with the destruction decision):

- **`PieceHealth`** (default 300, min 1): hit points of every individual piece. Weapon damage reduces a piece's health; the piece collapses (vertical or geo, per DestructionType) when it reaches 0.
- **`PieceDamageReduction`** (default 0): flat reduction subtracted from every hit (mirrors ADestructableEnvActor::DamageReduction).
- **Crush-by-vehicle bypasses health** — crushing always destroys the piece instantly (same as the env actors).

### Damage routing (TakeDamage override)

Follows the established AInstancedDestrucablesEnvActor convention:

| Damage source | Event type | Piece resolution |
|---|---|---|
| ICBM (full hit result) | FPointDamageEvent with HitInfo.Component | Direct component -> segment map lookup |
| Projectiles, bombs | FPointDamageEvent with only HitInfo.Location | Nearest alive piece to the hit location |
| Radial/AoE events | FRadialDamageEvent | All alive pieces within Params.OuterRadius of Origin |
| Hitscan weapons (UWeaponState trace/laser/flamethrower) | plain FDamageEvent, no spatial data | **Ignored** (returns 1.f; project convention - instanced destructibles ignore these too) |
| BP / mission scripts | n/a | DamageSegment(Index, Damage) / DamageSegmentByComponent(HitComponent, Damage) |

Never returns 0 from TakeDamage: weapons treat 0 as a kill signal (kill credit/voicelines), which env actors deliberately avoid.

### BP surface additions

- `DamageSegment(int32, float)`, `DamageSegmentByComponent(UPrimitiveComponent*, float)` - explicit damage entry points.
- `GetSegmentHealth(int32)` - remaining health query.
- `BP_OnPieceDamaged(SegmentIndex, RemainingHealth)` - fired when a piece takes damage and survives.

### Known limitation (inherited project-wide)

Hitscan weapons (UWeaponStateTrace/Laser) build a plain FDamageEvent without the trace FHitResult, so neither this spline nor AInstancedDestrucablesEnvActor can attribute their damage to a piece. If hitscan should chip spline pieces, add an FPointDamageEvent-carrying overload of UWeaponState::FluxDamageHitActor_DidActorDie (the FHitResult is in scope at all call sites) - a deliberate gameplay change that also makes hitscan damage instanced destructibles.

---

## 14. Troubleshooting: pieces not destructible / not crushable (added 2026-07-16)

**#1 cause: the piece mesh has no usable collision.** USplineMeshComponent builds its deformed physics
body from the source mesh's collision data. If the mesh has NO simple collision shapes and its Collision
Complexity is not UseComplexAsSimple, the engine SILENTLY produces no body at all
(USplineMeshComponent::GetBodySetup() returns null): pieces render fine but cannot block, be hit by
weapons, or overlap vehicles - i.e. nothing reacts, no errors.

Mitigations built into ADestructibleSplineActor:
- `ValidatePieceMeshCollision()` (on build): reports on screen if the PieceMesh asset has no simple
  collision and is not complex-as-simple, with fix instructions.
- `EnsureSegmentCollisionBuilt()` (per segment): if the deformed body is missing after setup, forces
  `USplineMeshComponent::RecreateCollision()` + `RecreatePhysicsState()`; reports if a body still cannot
  be built.
- `ReportUnattributableDamageOnce()`: if damage arrives that carries no spatial data (plain FDamageEvent,
  e.g. hitscan weapons), a one-shot on-screen error explains why it is ignored.

**Checklist when a spline will not destroy:**
1. Watch the screen/log for the DestructibleSplineActor errors above - they name the exact problem.
2. Piece mesh asset: add simple collision (box/convex preferred) in the Static Mesh editor, or set
   Collision Complexity = UseComplexAsSimple. Prefer simple collision (cheaper, deforms cleanly).
3. Cooked/packaged builds with complex-as-simple meshes need CPU-accessible mesh data (bAllowCPUAccess)
   for runtime trimesh cooking - another reason to prefer simple collision.
4. Crush requires `bCrushableByVehicles = true` (sets vehicle-overlap channels + overlap events).
5. Geo collapse requires `PieceGeometryCollection` assigned (a config error is reported otherwise and the
   piece is deliberately NOT removed).
6. Weapon type matters: projectiles/bombs (point damage with location) and radial damage work; hitscan
   trace/laser/flamethrower damage is unattributable and ignored (see section 13).

**Attribution detail:** nearest-piece resolution uses distance to each piece's start-end segment line
(not piece centers), so hits near the end of a long piece are attributed to that piece, not its neighbour.