# Wall hit damage by masking (investigation, 2026-11-09)

Goal: walls show holes where shells hit them, and pieces fly off, without a geometry collection. The model is the
add-on armour panels (`Docs/Units/Vehicles/AddOnArmorPanels.md`): the geometry stays in the mesh, a masked material
clips what is gone, one custom-primitive-data (CPD) write per hit, and a pooled CPU Niagara burst draws the pieces.
Nothing is built yet. This note lists what the walls do now and the ways to do it.

## What the walls do now

Two families, with different destruction:

| Family | Blueprints | Parent | While alive | On death |
|---|---|---|---|---|
| Concrete walls | `Rus_Wall_Concrete_Corner`, `_CornerNoFlag(_Soviet)`, `_Long(_Soviet)`, `_LongSandBags(_NoFlag/_Soviet)` (`Blueprints/Buildings/Walls/RusWalls`) | `BP_WallActor` -> `AWallActor` -> `ADestructableEnvActor` | nothing | `CollapseMesh` -> `FRTS_Collapse`: the `GeometryCollection` component gets `GC_SM_Wall_14` / `GC_SM_Wall_13`, 30 s lifetime, plus 2 debris explosions and 2 `StoneDestroyed` decals |
| Refinery walls (building expansions) | `Rus_Building_GateWired*`, `_Wall*`, `_WallFlag*`, `_WallLeftConnection*`, `_WallRightConnection*` (`BuildingExpansions/RusBuildings/Wall`) | `RTS_BP_BuildingExpansion` -> `ABuildingExpansion` | `Bp_OnHealthChanged` swaps to a hand-made `_At50` mesh at 50 % and plays explosions at random sockets | `VerticalDestruction` (`FRTS_VerticalCollapse`, the mesh sinks), no geometry collection |

So only the concrete family uses geometry collections, and only when it dies. The refinery walls would get holes in
place of (or on top of) their `_At50` mesh swap.

Meshes:

| Mesh | Used by | Tris | Size (cm) | Material |
|---|---|---|---|---|
| `SM_Wall_14` | `_Long`, `_Long_Soviet` | 12 (a box) | 2260 x 20 x 500 | `M_Concrete_5` (opaque, 1 UV channel) |
| `SM_Wall_13` | `_Corner*` | 24 (two boxes) | 1140 x 1160 x 500 | `M_Concrete_5` |
| `SM_Rus_ConcreteSandWall` | `_LongSandBags*` | 6,868 | 2324 x 112 x 500 | `MI_Rus_SandBag_01` + `MI_Rus_BunkerConcrete_05` |
| `SM_RustedRefinery_WalkableWall` (+`At50`) | refinery walls | 4,976 | 260 x 1220 x 740 | `M_Nuclear_Tower` + `M_Barbed_1` |
| `SM_RustedRefinery_WalkableGate` | gates | 6,614 | 571 x 1565 x 756 | same |
| `SM_RustedRefinery_WalkableWallConnection_Right` | right connections | 7,626 | 414 x 1256 x 734 | same |

Finding, not changed: `Rus_Wall_Concrete_LongSandBags` collapses `GC_SM_Wall_14` (the plain slab) for its sandbag
mesh, so at death the sandbag wall turns into slab pieces.

## What a hit gives us

- **Direct hits:** `AProjectile::OnHitActor` sets `M_DamageEvent.HitInfo.Location` and calls `TakeDamage` with the
  projectile as causer, so a point damage event with the hit position and the shot direction arrives.
  `ADestructableEnvActor::TakeDamage` and `AHPActorObjectsMaster::TakeDamage` (the building expansions, through
  `GotHit(DamageEvent)`) are both C++ and both see the event. Calibre and shell type are on the `AProjectile` causer.
- **Splash:** `FRTS_AOE::ApplyDamageToActor` passes a damage event without a position, but the causer is the
  projectile at the explosion. The wall can use the closest point of its mesh to the causer, as the add-on panels do.
- **Limits:** UE 5.5 has 36 CPD floats per primitive. A float holds 20 bits exactly, as on the panels. Whether the wall
  materials already read CPD is not checked yet.

## Options

All options share these parts:
- a `UWallHitDamageComponent` fed from `TakeDamage` (direct hit: hit position; splash: closest point to the causer;
  small arms below a calibre floor do nothing, like the 20 mm rule on panels);
- a pooled CPU Niagara burst at the hit, pushed along the shot direction (the `NS_AddOnArmorPanelDebris` HLSL, with
  several mesh user parameters like the test system `NS_TestSkirtDebris`, `User.DebrisMesh0..7`);
- the material only turns masked once the wall is hit: an intact wall keeps its opaque material and swaps to the
  masked instance on the first hit (`SetMaterial`, once). Masked surfaces lose early-Z in the depth prepass and
  shadow depths, and a wall covers far more pixels than a skirt, so this keeps intact walls at zero extra cost;
- death without Chaos: hide everything that is left, one big burst, and the existing decals and explosions (as
  `BlowOffIntactPanelsOnDeath` does on tanks). A rubble mesh (`CollapseMeshWithSwapping`, as the sandbag walls'
  `_Destroyed` meshes) can stay behind;
- the holes are visual only: collision, line of sight and the nav mesh stay as they are.

### A. Region grid (bitmask)

Split the wall into a grid of regions in mesh space (for example 1.5 m cells, about 45 on `SM_Wall_14`, 3 floats).
The shader finds the cell of each pixel from its local position (no mesh change, works on the 12-tri box) and
clips it when its bit is set; noise erodes the cell edges so they don't read as squares. A hit sets the bit of the
nearest intact cell (bigger shells: the cells within a radius).

- Good: closest to the panels; the state is a bitmask, so repair (clear bits in reverse) and save/load are easy.
- Bad: holes follow the grid. The cut is paper thin: on a 20 cm slab you look through a hole with no inner faces.

### B. Hit-point list (procedural holes)

Store up to K hits as (local X, local Z, radius) in CPD (3 floats each; K = 8 to 10 next to anything else). The
shader clips pixels inside a noise-warped radius of any hit, with a dark, cracked, rough rim band outside it. The
radius comes from the calibre. When K is full, merge the new hit into the nearest one (grow it).

- Good: round, organic holes exactly where the shell hit, sized per gun; works on any mesh with no re-authoring.
- Bad: same paper-thin cut. On the hollow refinery meshes the hole shows the empty inside of the shell (not checked
  whether they are hollow). Fixed number of holes. Repair has to drop hits, which is less natural than bits.

### C. Hybrid of A and B

Region bits decide what is gone (clean state for repair, save and death); each region also stores where it was hit,
which warps that region's hole shape. Organic holes with bitmask bookkeeping, at the price of the most complex
shader and the most CPD floats.

### D. Baked fracture chunks (the panel technique as it is)

Fracture the wall once into chunks (about 1 m Voronoi cells through the full thickness, in Blender as the skirt test
did with `make_skirt_pieces.py`, or from the existing `GC_SM_Wall_14` fracture). Merge the chunks back into one
static mesh, write each chunk's index into a UV channel and store the chunk centres in asset user data, like
`UAddOnArmorPanelLayout`. The masked material clips chunks whose bit is set. A hit hides the chunks within a
calibre radius of it.

- Good: broken edges. When a chunk goes, the neighbours' fracture faces show broken concrete at the rim (at the RTS
  camera, -70 degrees and 30 m, 20 cm of depth is a few pixels, so the gain is those faces and matching debris, not
  depth). The flying debris can be the chunks themselves, cut out the way `ExtractPanelDebris` cuts the panels.
- Carries over from the panels: the component pattern, the masked-material copy, CPD bits, repair and save sync, the
  debris Niagara. Does not: `BuildPanelHull`'s classifier (it finds panels by |Y| and Z bands and island length,
  vehicle rules); a wall needs "every island is one chunk", or a Blender export that already carries the chunk ids.
- Material change: the panel master reads one CPD float (20 bits). 100 chunks = 5 floats, so the wall master picks
  the float by `chunk index / 20` first.
- Bad: an authoring step per wall mesh. The 12-tri slab becomes a few thousand tris (still one draw). Holes have the
  chunk shapes. The fracture faces need their own material slot or atlas area (Blender gives them no UVs). The skirt
  test saw a lighting artefact on its merged mesh (likely the distance field); expect the same, it is not the mask.
- Authoring path for the prototype: Blender cell fracture -> FBX (proven by `make_skirt_pieces.py`). Not a C++
  conversion of the geometry collection to a static mesh: never done in this project, and mesh-description rebuilds
  have crashed the editor before.

## Recommendation

**D for the concrete walls**, prototype on `Rus_Wall_Concrete_Long` (`SM_Wall_14`): the simplest mesh, one material,
the family that really uses a geometry collection, and the one where the paper-thin cut of A and B is most visible.
B's rim band (scorch and cracks around each hit) would make the edges look shot, not cut, but it needs the hit
positions in CPD too (3 floats per hit on top of D's 5; about 8 hits fit in 36). Leave it out of the first prototype
and add it once the chunks alone have been judged.

For the refinery walls, decide after the prototype: B (no re-authoring of 5-7k tri meshes, but check first whether
they are hollow) or D on those meshes. Their `_At50` swap can stay until then.

## Measured shader cost (2026-11-09)

`MaterialEditingLibrary.get_statistics`, base pass, against `M_Concrete_5` (VS 289 / PS 284). The test materials
are in `/Game/RTS_Survival/Tests/WallHitMasking/ShaderCost/`.

| Material | VS | PS | delta PS | Samplers |
|---|---|---|---|---|
| Masked, constant mask | 289 | 284 | 0 | 5 |
| D (chunk index in UV, 5 CPD floats, bit clip) | 291 | 299 | +15 | 5 |
| A only (150 cm grid, noise-eroded edges) | 289 | 323 | +39 | 6 |
| B only (8 hits, noise-warped radius) | 289 | 355 | +71 | 6 |
| A + B mask, no rim | 289 | 374 | +90 | 6 |
| A + B with rim shading | 289 | 384 | +100 | 6 |
| A + B, local position computed in the VS | 317 | 369 | +85 | 6 |

- Depth-only and shadow-depth passes are not in the stats. A masked material runs only the mask part there, so the
  mask-only deltas are the estimate per depth/shadow pixel: D +15, A +39, B +71, A+B +90.
- The cost of masked mode itself (losing early-Z in depth and shadow passes) does not show in instruction counts.
- A Custom node with `[unroll]` costs the same as plain nodes. A `[loop]` reads 545 statically, which is not an
  execution estimate.
- On the real wall master, `M_RusBunker_Concrete` (PS 319), D's masked copy reads PS 337 with its tint (+18).

## Open points

- Chunks left floating when everything under them is gone. A simple support rule (hide unsupported chunks with the
  next hit) or accept it.
- Repair (`bIsHealing` on building expansions) and save/load: re-derive the mask from health, as the panels
  re-sync from module state.
- Two-material meshes (sandbags, refinery masters `M_Nuclear_Tower` / `M_Barbed_1` shared with other assets) need
  wall-only masked copies per slot.
