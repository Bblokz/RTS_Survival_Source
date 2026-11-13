# Wall gates (`AWallGate`, 2026-11-12)

Gates in Soviet walls that open by themselves for one player's units and close again when none are near. The
player is usually German and the Soviets are the enemy AI, so by default a gate opens for player 2 and stays shut for
everyone else.

## Runtime (C++)

Code: `Source/RTS_Survival/Environment/DestructableEnvActor/WallGate/`.

| Piece | What it is |
|---|---|
| `AWallGate` (`WallGate.h/.cpp`) | Derives from `AWallActor`, the class the Rus walls use (`Rus_Wall_Concrete_Long` is a `WallActor` Blueprint, not a bare `ADestructableEnvActor`). It inherits the RTS component, `SetupWallCollision`, health, holes and death. |
| `UWallGateNavModifier` (`WallGateNavigation.h/.cpp`) | Nav modifier over the doorway. Its box comes from the leaf mesh, not from the owner's collision. |
| `UNavArea_WallGatePlayerSide` / `UNavArea_WallGateEnemySide` | The doorway area of a closed gate whose allowed player is 1, or any other player. |
| `URTSWallGateNavQueryFilter` | Base class for the four `RTS_NavFilter_*` Blueprints. It excludes the other side's closed-gate area for each querier. |

### Components (created in C++)

| Name | Class | Notes |
|---|---|---|
| `GateRoot` | `USceneComponent` | The actor root. The Blueprint's own components (frame `StaticMesh`, `RTSComponent`, hole components) hang under it. |
| `GateLeafMesh` | `UStaticMeshComponent` | The moving leaf. Mesh and VAT materials are set in the Blueprint. `CanEverAffectNavigation` is off. The default custom primitive data has index 3 = 1.0. |
| `GateNavModifier` | `UWallGateNavModifier` | Doorway nav area. |
| `FrameNavModifier` | `UWallGateFrameNavModifier` | `NavArea_Null` boxes over the frame's simple collision elements. |
| `RTSComponent` | `UWallGateRTSComponent` | `URTSComponent` with wall defaults: owning player 2, not added to the game state, formation radius 0. `AWallActor` only looks one up, so the gate creates its own. |

The frame is the Blueprint's `StaticMesh` component, as on the Rus walls. It is matched by name (`FrameMeshComponentName`).
The gate needs no Blueprint graph nodes and no extra components:
- At BeginPlay it calls `SetupWallCollision(StaticMesh, bFrameAffectsNavMesh)` itself. A Blueprint that also calls it
  sets the same values again.
- Don't add a second RTS component; `AWallActor` uses the first one it finds.

### Properties (per instance)

| Property | Default | Meaning |
|---|---|---|
| `AllowedPlayer` | 2 | Owning player whose units open the gate. The project has no shared constant: 1 is the human and 2 the enemy AI (`URTSComponent::GetOwningPlayer`). It is `WallGateDefaults::EnemyAIPlayer` here. |
| `OpenRadius` | 1400 | Distance in cm, measured in 2D from the doorway centre, on both sides. |
| `DetectionIntervalSeconds` | 0.3 | Scan timer interval. The first scan comes at a random offset. |
| `CloseDelaySeconds` | 2.0 | Time without an allowed unit in range before closing starts. |
| `DoorwayClearancePadding` | 200 | The closing check grows the leaf's rest bounds by this many cm. |
| `OpenDurationSeconds` / `CloseDurationSeconds` | 2.5 / 3.0 | Durations of a full move. |
| `PassableProgressThreshold` | 0.15 | An opening gate at or below this progress turns passable. |
| `FrameMeshComponentName` | `StaticMesh` | The leaf copies this component's collision. |
| `NavMode` | `PlayerAwareArea` | See Navigation. `BlockEveryone` is the zero-setup fallback. |
| `NavDoorwayPadding` | 60 | The doorway nav box is thickened by this many cm across the wall (the leaf's thinner horizontal axis) and downward. It is not padded along the doorway, so it stays out of the pillars' boxes. |
| `bFrameAffectsNavMesh` | true | Passed to `SetupWallCollision` for the frame. True matches the Rus walls, whose `NavArea_Null` modifier needs the wall mesh to affect navigation. |
| `FrameNavBoxPadding` | 40 | Each frame nav box grows by this many cm on every side. |
| `FrameNavMaxBoxBottom` | 300 | Frame collision elements whose bottom lies above this height (frame-mesh space) are skipped, such as gate C's lintel at Z 1000..1125. |
| `OpenSound`, `CloseSound`, `GateSoundAttenuation`, `GateSoundConcurrency` | none | Played at the doorway centre when each move starts. |
| `PreviewOpenAmount` (editor only) | 0 | 0 shows the gate closed, 1 open. It is written in `OnConstruction` to the transient custom primitive data and has no effect in game. |

Read-only from Blueprints:
- `GetGateState()`: `EWallGateState` (Closed, Opening, Open, Closing, Wrecked).
- `GetGateOpenAmount()`: 0..1.
- `GetIsGatePassable()`.

Blueprint events, for VFX: `BP_OnGateStartedOpening`, `BP_OnGateStartedClosing`, `BP_OnGateFullyOpen`,
`BP_OnGateFullyClosed`.

### Custom primitive data contract (`GateLeafMesh`)

- **Index 3 is the VAT progress:** 1.0 = closed (rest pose; the shader early-outs at >= 0.9999), 0.0 = fully open.
  `WallGatePrimitiveData::ProgressIndex`.
- **Closed by default everywhere:**
  - The constructor sets the default CPD index 3 to 1.0.
  - `OnConstruction` puts it back if a Blueprint changed it.
  - BeginPlay writes 1.0.
- **Only index 3 is written,** one float at a time (`SetCustomPrimitiveDataFloat`). The array is never reset.
  `UWallHoleDamageComponent::WriteMask` only writes 8..34, and growing the array keeps index 3.
- **Writes happen only while the leaf moves.** The actor ticks at 30 Hz during a move and is switched off when idle.
  A closed gate gets exactly 1.0 at the end.

### Behaviour

- **Detection:**
  - A timer reads the unit registry (`UGameUnitManager::GetPlayerTanks` / `GetSquadUnitsOfPlayer`) and checks each
    unit's 2D distance to the doorway centre. There are no physics queries.
  - Units must be alive and owned exactly by `AllowedPlayer`.
  - Aircraft are ignored. Tanks include the nomadic vehicles.
- **Open:** an allowed unit in range opens a closed or closing gate. A reversal continues from the current progress.
- **Close:** after `CloseDelaySeconds` without an allowed unit in range, an open or opening gate starts closing. It
  never starts closing while any unit's centre is inside the doorway box. That box is the leaf's rest bounds plus
  `DoorwayClearancePadding`, and units of both registry sides count. The check runs only when the gate is about to
  close.
- **Collision:**
  - After BeginPlay the leaf copies the frame's object type, responses and enabled state.
  - The Rus wall profile only blocks visibility, building placement and one weapon trace. Units are kept out by
    navigation, not by collision.
  - The leaf collision is switched off (`NoCollision`) once the opening gate reaches `PassableProgressThreshold`. It
    comes back as soon as closing starts.
  - While the leaf moves its collision stays at the rest pose, which is why it must not exist once the gate is open.
- **Holes:** the Blueprint carries two `UWallHoleDamageComponent`s, one with `WallMeshComponentName = StaticMesh` and
  one with `= GateLeafMesh`. `ADestructableEnvActor` routes each hit:
  - A point hit goes to the component whose mesh collision lies nearest to `HitInfo.Location`, or to the one named in
    `HitInfo.Component`. Meshes without query collision, such as an open leaf, are skipped.
  - Splash goes to every component whose mesh has query collision; each one holes only where the blast reaches its
    mesh.
  - Actors with a single hole component behave exactly as before.
- **Death:**
  1. In `OnUnitDies`, before the Blueprint event, the gate stops its timer and tick.
  2. It hides the leaf and switches off the leaf's collision.
  3. It sets the doorway nav area to default.
  4. The hole components then collapse as usual through `OnDestructableEnvActorDied`. The leaf's pieces are thrown
     from its rest pose.

### Navigation

The navmesh uses `RuntimeGeneration=DynamicModifiersOnly`, so only nav modifiers change at runtime, which is how the
walls already block (a `NavArea_Null` `NavModifier` plus the `RTS_BP_NavBloker_RusWall` obstacle child actors). Wall
meshes do not block pawns by collision. There are four agent navmeshes (Character, Light, Medium, Heavy tank), and the
filters per agent come from `URTSNavQueryFilterSettings`, shared by both players. The project has no team or alliance
check for units: `ERTSFaction` (`FactionSystem`) is only the faction a player picks. Hostility is owning player 1
against everyone else, and the unit registry is split the same way.

The frame blocks navigation through `FrameNavModifier`, which works for gates spawned at runtime too.
- It covers one `NavArea_Null` box per simple collision element of the frame mesh's body setup. Non-box elements use
  their bounding box.
- The boxes are in frame-mesh space, padded by `FrameNavBoxPadding`.
- Elements starting above `FrameNavMaxBoxBottom` are skipped.

The gate also owns a `UWallGateNavModifier` over the doorway, oriented with the leaf: the leaf's static-mesh rest
bounds, thickened across the wall by `NavDoorwayPadding`. Its area follows the gate:

| Gate | `PlayerAwareArea` (default) | `BlockEveryone` |
|---|---|---|
| Closed or closing | `UNavArea_WallGateEnemySide` (allowed player is not 1) or `..._PlayerSide` (allowed player is 1) | `NavArea_Null` |
| Passable (opening past the threshold, open) | `NavArea_Default` | `NavArea_Default` |
| Dead | `NavArea_Default` | `NavArea_Default` |

**PlayerAwareArea** lets the allowed units plan through a closed gate, which then opens when they come near; other
units do not plan through it.
- **How:** `URTSWallGateNavQueryFilter` has `bInstantiateForQuerier` set. `InitializeFilter` takes the querier's pawn
  (the AI controller's pawn) and its `URTSComponent` owning player.
  - It excludes the other side's gate area: player 1 cannot use `EnemySide` gates, every other player cannot use
    `PlayerSide` gates.
  - Queries without a known unit, such as the cover finder (which passes the world), exclude both.
- **Setup required:** reparent `RTS_NavFilter_Characters`, `_LightTanks`, `_MediumTanks` and `_HeavyTanks` (in
  `Blueprints/Nav/Filters`) onto `URTSWallGateNavQueryFilter`. Their area overrides are kept, because the class calls
  `Super::InitializeFilter` first.
- **Fallback:** at BeginPlay the gate checks that `DefaultFilterClass` and every `NavFiltersByAgent` entry derive from
  that class. If they do not, it reports an error and falls back to `BlockEveryone`.

### Limitations

- **BlockEveryone:** allowed units do not plan through a closed gate. They only route through once a unit within
  `OpenRadius` has opened it and the doorway tiles have rebuilt. Paths that crossed the old doorway are replanned by
  the engine's path invalidation, but units that were already routed around are not.
- **Filter cost:** with `PlayerAwareArea` the four RTS filters are no longer cached per class but built per query,
  which costs a little CPU per path request.
- **Queries that use other filters:** convoy routes, POI and survival planners pass a null filter class, so they see
  gate areas as plain ground (cost 1) and may plan through a closed gate.
- **Open gates are open to everyone.** A German unit can plan through and drive through an open gate. The gate does not
  close on it while it stands in the doorway, and once it closes the doorway is blocked again for German units.
- **Units are not physically stopped.** Wall collision ignores the unit channels, and with `PlayerAwareArea` a closed
  doorway is walkable for allowed units.
  - An allowed vehicle that covers `OpenRadius` in less time than the leaf takes to clear (about 2.1 s:
    `OpenDurationSeconds` x (1 - `PassableProgressThreshold`)) drives through a leaf that is still closing or opening.
  - Raise `OpenRadius` or shorten `OpenDurationSeconds` for fast units.
- **Allowed units idling within `OpenRadius` keep the gate open.**
- **Doorway checks use unit centres** (actor location) against the padded box. Long vehicles rely on the 200 cm
  padding.
- **The rest box ignores bounds extensions.** It is the leaf static mesh's bounding box with the mesh's
  positive/negative bounds extensions removed. A VAT leaf may widen its bounds that way without inflating the doorway,
  the nav box or the detection centre. The component's `BoundsScale` is not used.
- **Two hole components mean two collapse sounds** on death, unless one has no `CollapseSound`.
- **Rest-pose collision and holes:** the leaf's collision, its hole positions and its hole debris all use the rest
  pose, and the hole centres are stored in the leaf's mesh space.
  - The holes only follow the moving leaf if the hole mask in its material is evaluated on the pre-VAT local
    position.
  - The leaf's death debris is thrown from the rest pose, even when the gate died open.
- **Both meshes need simple collision** (boxes or hulls). Hit routing measures the distance to collision. Complex-only
  collision falls back to render bounds, and the frame's bounds contain the doorway, so hits would favour the frame.
- **The frame's collision must leave the doorway free at ground level.** Use per-pillar shapes; a lintel is fine above
  `FrameNavMaxBoxBottom`. `FrameNavModifier` and the hole routing both use these shapes.
  - Don't add a stock `NavModifierComponent`: it covers the whole actor's navigation-relevant collision, frame
    included.
- **Tested (2026-11-12)** in a throwaway DebugGame editor (`gate_headless_test.py`, see below), not yet in the
  Development editor (its DLL still lacks `AWallGate`).

## Assets (gates A and C)

Picked from five concepts (`Source/Tools/RusWallGates/Concepts/`, sheets in `Concepts/v01/`): **A** twin swing leaves
with star roundels between capped pillars, **C** a concrete slab lifted between 11 m towers with counterweights.

Content: `/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/`

| Asset | What |
|---|---|
| `BP_RusWallGateA` / `BP_RusWallGateC` | `AWallGate` Blueprints, no graph. Components added by data: `StaticMesh` (frame), `WallHoleDamage_Frame`, `WallHoleDamage_Leaf` (no collapse sound). A: health 1400, open/close 2.5/3.0 s; C: 2200, 4.0/4.5 s. Sounds `Sus4_Iron_Doors_*` with `Att_Attachments`/`SC_Attachments`. Aim points over pillars/towers and leaf. |
| `Meshes/SM_RusWallGate{A,C}_Frame` | Static part, slots `Concrete` (the wall's `MI_Rus_BunkerConcrete_05`) and `GateSteel`. Box collision per pillar / tower (+ C lintel). |
| `Meshes/SM_RusWallGate{A,C}_Leaf` | Moving part as one VAT mesh. UV0 atlas/concrete, UV1 lightmap, **UV2 = (piece u, pivot.x), UV3 = (pivot.y, pivot.z)**. One box collision = the closed leaf. A has `positive_bounds_extension` Y 350 (open leaves swing into +Y). A: 2 pieces (leaves); C: 5 (slab, 2 counterweights, 2 sheaves). |
| `Textures/T_RusWallGate_Atlas` | Painted-steel atlas (olive plate, star roundel, dark steel, rust, lamp glass) used as `ConcreteDiffuse`. |
| `Textures/T_RusWallGate{A,C}_VAT_Offset/_Rot` | Baked rigid VAT, width = pieces, 61 frames; RGBA16F, no mips, clamp. |
| `Materials/MF_WallGate_VAT` | The nomadic VAT shader (`NomadicDeploy/nomadic_deploy_vat.hlsl`) reading TexCoord **2/3**; params `WallGateVatOffset/Rot/Frames`, progress `WallGateProgress` = CPD 3. |
| `Materials/MI_RusWallGate_Steel`, `MI_RusWallGate{A,C}_Leaf_*` | Steel = child of `MI_Rus_BunkerConcrete_05` with the atlas; leaf MIs turn the `WallGateVAT` switch on. |
| `Holes/*_WallHoles`, `Holes/SM_RusWallGateSteel_Debris_*` | Hole instances under `MI_Rus_BunkerConcrete_05_WallHoles` (no grid), VAT ones for the leaves; steel debris for A. |

Shared master change: `M_RusBunker_Concrete` and `M_RusBunker_Concrete_WallHoles` got a static switch `WallGateVAT`
(default off, so walls and bunkers compile as before) feeding `MF_WallGate_VAT` into WPO, and the holes master's
WorldPosition node now excludes material offsets, so holes are tested on the leaf's rest pose and travel with it.
**Why UV2/3 and not the nomadic UV4/5:** these masters are also compiled for skeletal meshes, whose GPU-skin vertex
factory takes at most 4 UV sets; a TexCoord 4 read fails there and the whole instance falls back to the default
material (no WPO: the leaves never moved in the first test).

Pipeline (`Source/Tools/RusWallGates/`, Blender 4.0 + editor python):
1. `make_gate_atlas.py <png>` (system python).
2. `blender -b --factory-startup -P build_gates.py -- Saved/RusWallGates/build [--preview]`: meshes + VAT (or previews).
3. In an editor: `ue_import_gates.py` (BUILD global), `ue_gate_mf.py`, `ue_patch_gate_masters.py`,
   `ue_gate_collision.py`, `ue_gate_materials.py`, then `ue_build_gate_bps.build_all()`.
   `gate_fix_headless.py` runs the mesh reimport + material chain in a throwaway editor.
4. Test: `gate_headless_test.py` in a throwaway DebugGame editor on `RTS_ToCopySmall` (fog post process switched off for
   pictures): holes on both closed leaves, Soviet T-34 opens A then C, A closes after it leaves, a German tank in C's
   doorway keeps C open, C closes once it leaves, a German tank in front of A does not open it. Pictures + log in
   `Saved/RusWallGates/test/`.

Open: Development editor rebuild + reparenting the four `RTS_NavFilter_*` Blueprints onto
`URTSWallGateNavQueryFilter` (until then each gate logs an error and uses `BlockEveryone`); gates not yet placed in
any map; an A banner/flag variant and a camo-skin (`T_SovietConcrete_Camoe`) child are easy follow-ups.
