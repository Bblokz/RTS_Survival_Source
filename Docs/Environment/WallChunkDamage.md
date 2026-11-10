# Wall chunk damage and procedural wall holes (prototypes, 2026-11-09)

Two prototypes to compare side by side:
- **D, baked chunks:** `Rus_Wall_Concrete_Long` (`UWallChunkDamageComponent`), below.
- **A + B, procedural holes:** `Rus_Wall_Concrete_Long_Holes` (`UWallHoleDamageComponent`), in its own section further
  down.

Both use `WallDamageShared.h/.cpp`: the hole and collapse settings, hit resolution, hole radius, the debris bursts
(`NS_AddOnArmorPanelDebris`), the ground trace and the collapse sound. A hit gives both walls the same hole radius and
health cap, so they compare fairly.

Option D of `WallHitMasking.md`, built on `Rus_Wall_Concrete_Long` (and its child `_Long_Soviet`, which inherits
it). The wall is cut into 100 chunks baked into one mesh. Shells knock out the chunks around the hit through a masked
material, and the chunks fly as pooled Niagara debris. When the wall dies, every chunk left flies off and the actor is
destroyed. No geometry collection and no Chaos.

## Pieces

| Piece | Where |
|---|---|
| `make_wall_chunks.py` | `Tools/WallChunks/`. Blender 5, headless. Builds the slab of `SM_Wall_14` (X -2260..0, Y -20..0, Z 0..500), cut into a jittered 20 x 5 Voronoi grid (seed 9113). Each chunk is a closed prism with its own vertices. UV0 = the slab's planar map. UV1 = a placeholder for the generated lightmap. UV2.x = the chunk index. Slot 0 `Concrete` holds the slab faces, slot 1 `ConcreteBroken` the fracture faces. Also writes 8 typical chunks as debris pieces, and `<name>_layout.json` (centres, areas, which piece and scale each chunk flies off as). Exports are in `Tools/WallChunks/Exports/`. |
| Meshes | `Blueprints/Buildings/Walls/RusWalls/Meshes/Chunks/`: `SM_Wall_14_Chunks` (1,808 tris, one LOD, box collision, lightmap UVs in channel 1), `SM_Wall_14_Chunks_Debris_0..7` (16-28 tris, no collision). Imported with the legacy FBX importer, `import_uniform_scale=100`. |
| `UWallChunkLayout` | `Environment/DestructableEnvActor/WallChunkDamage/WallChunkLayout.h`. Asset user data on the chunk mesh: chunks, debris pieces, the masked material per slot, the half thickness. Mask constants: UV channel 2, custom primitive data 8..12, 20 chunks per float, at most 100 chunks. |
| `RTS.WallChunks.ApplyLayout` | `WallChunkLayoutAuthoring.cpp`. Args: `Mesh=<chunk mesh> Json=<layout json> Damaged=<slot 0 mat>\|<slot 1 mat> [Debris=<folder>]`. Reads the JSON and stores the layout on the mesh. |
| `RTS.WallChunks.FinishMaskedMaster` | Same file. Args: `Material=<masked copy>`. Clears the opacity-mask constant that a duplicated opaque master keeps, then re-derives `bCanMaskedBeAssumedOpaque`. Without it the mask compiles to a constant (PS 322 instead of 337) and nothing is hidden. Python cannot reach that flag. |
| Materials | `M_RusBunker_Concrete_WallChunks` is a masked copy of `M_RusBunker_Concrete` (the material the Blueprint puts on the wall), with the chunk mask (Custom node, CPD 8..12), `WallChunkTint` and `WallChunkRoughnessAdd`. `MI_RusBunkerConcrete_05_WallChunks` is for the slab faces; `..._WallChunksBroken` is for the fracture faces (tint 0.6, roughness +0.3). Both carry SandStrength 0.568, as on `MI_Rus_BunkerConcrete_05`. Used with Niagara mesh particles. |
| `UWallChunkDamageComponent` | Same folder. `ADestructableEnvActor::TakeDamage` forwards each hit to it, before the death check. On death it binds `OnDestructableEnvActorDied`. |
| Debris | The add-on armour system `NS_AddOnArmorPanelDebris`, unchanged: one burst per debris piece, at most 20 per burst, uniform scale per chunk. |
| Hook | `AProjectile::GetWeaponCalibre()` (new getter). |

## Behaviour

- **The intact wall is opaque.** The mesh slots and the Blueprint override stay `MI_Rus_BunkerConcrete_05`. The first
  hit that removes a chunk swaps both slots to the masked instances, once. An undamaged wall pays nothing for the
  mask.
- **Hit position.**
  - Point damage gives the impact. Shells set `HitInfo.Location`.
  - Other damage events use the causer's position (splash). Splash further from the slab makes a smaller hole.
  - Distances are measured on the wall face (mesh X/Z), so both faces hole through alike.
- **Hole radius.**
  - From a projectile: calibre x 0.85 cm. 37 mm gives 31 cm, 75 mm 64 cm, 150 mm 128 cm, capped at 220 cm.
  - From anything else: sqrt(damage) x 5 cm.
  - Below 20 mm nothing happens.
  - Chunks whose centre is inside the radius go. A direct hit also takes the nearest chunk when the radius is 45 cm or
    more, and by chance (radius / 45) when it is smaller. That only happens when the chunk centre lies within the
    radius + 75 cm: shrapnel reuses its shell's old impact point, which can be far along the wall.
- **Health cap.** While the wall stands, at most ceil(lost health x 0.65 x 100) chunks are gone, and never the last
  one. The hit that kills the wall removes nothing itself; the collapse throws everything.
- **Debris.** The pieces leave the face the shell went through, spread away from the impact, and get an upward kick
  and tumble. They bounce on the ground plane, lie flat and shrink away after 3.6 s. A wall hidden by fog of war
  throws nothing.
- **Death.** Every chunk left is thrown at 1.3x speed along the last shot. The chunk mesh loses its collision and is
  hidden. `Collapse_Cue` plays with `Att_Collapse` and `SC_Collapse`. The actor is destroyed after 4.5 s. The
  Blueprint's explosions and `StoneDestroyed` decals are unchanged. Its `CollapseMesh` call, the make-struct nodes
  feeding it and the `GeometryCollection` component were removed.
- **Not visual:** collision, nav and line of sight do not change while the wall stands. The hole is only a picture.

## Checks (PIE on `UT_VehicleObstacle`, 2026-11-09)

- `Debug_FireTestHit` hits of 37 / 75 / 88 / 150 / 105 mm removed 1 / 2 / 2 / 6 / 3 chunks. Holes and flying chunks
  show. The neighbours' fracture faces show as a thin broken edge. Pictures: `Saved/WallChunks/holes_rts_view.png`,
  `holes_closeup.png`, `collapse_0.png`.
- Point damage through `TakeDamage` (no projectile) of 60 / 200 / 600 removed 0 / 1 / 3 chunks. The wall has 1400
  health at run time and a reduction of 9.
- Death by `ApplyDamage`: all 100 chunks thrown, life span 4.5 s, nothing left after it but the decals.
- **Not tested:**
  - a real shell, which is the calibre path through `AProjectile`;
  - splash without a point event;
  - fog of war;
  - a packaged build: the debris folder is always cooked, but the `Chunks` folder is referenced normally.

## A + B: procedural holes (`Rus_Wall_Concrete_Long_Holes`)

The plain `SM_Wall_14` (12 tris). No chunk mesh is needed: the holes are computed in the material from the wall's
local position.

| Piece | Where |
|---|---|
| `UWallHoleDamageComponent` | `WallChunkDamage/WallHoleDamageComponent.h/.cpp`. Finds its mesh by `WallMeshComponentName` (default `StaticMesh`). `DamagedMaterials` and `DebrisMeshes` are set on the Blueprint component. |
| Material | v2 library `Tools/WallChunks/make_wall_holes_material.py`. `masked_master_for` builds `<Master>_WallHoles` next to any opaque master (for this wall `RusBunkers/Materials/M_RusBunker_Concrete_WallHoles`). `make_holes_instance` makes the instance per slot and mesh, here `Meshes/Holes/MI_Rus_BunkerConcrete_05_WallHoles`: a copy of `MI_Rus_BunkerConcrete_05` reparented onto the masked master, with the grid shape of `SM_Wall_14` (along X, -2260 to 0, Z 0 to 500, cell 150, 15 x 3). `two_sided=True` is for thick or hollow meshes. `make_debris_pieces` makes the chunk pieces in a wall's own material. |
| Custom primitive data | 8-10: grid cells, one bit per cell, 20 per float. 11-34: 6 holes as (mesh X, mesh Y, mesh Z, radius); radius 0 means the slot is unused. The holes are spheres in mesh space, so they work on towers and thick walls. |
| Hooks | `ADestructableEnvActor::TakeDamage` (environment walls). `AHPActorObjectsMaster::TakeDamage` (building expansions, towers) passes the health before and after, plus the max. `AHPActorObjectsMaster::OnHealthChanged` with healing calls `OnOwnerHealed`: the newest holes and then cells close until the damage fits the health. |
| Rollout | `Tools/WallChunks/AGENT_BRIEF.md`: how the other Soviet walls and towers were converted. |

- **B, holes.** A hit cuts a round hole at the impact, using the same radius rule as D. The noise warps its edge, and a
  scorched, rougher rim surrounds it.
  - A hit inside an existing hole widens that hole instead (distance + 0.7 x the radius).
  - There are 6 slots. When they are full and the wall has a grid, the oldest hole is baked into the grid cells it
    covers, which frees its slot. Without a grid (`GridAxis = NoGrid`, used for towers), the oldest hole merges with
    its nearest neighbour into one sphere around both.
  - The grid runs along mesh X or Y (`GridSettings.GridAxis`, which must match the instance's `WallGridAlongY`). When
    the mesh changes (a building expansion finishing construction), the component re-reads the bounds and grid on
    the next hit and drops damage it recorded on the old mesh.
- **Death on building expansions:** the pieces fly, but the mesh stays for the building's own death (vertical sink).
  `CollapseIntoPieces(false)` runs from `OnUnitDies`.
- **A, cells.** The grid cells are 1.5 m. A hole at least 0.75 of a cell wide (113 cm, so shells of about 133 mm and up)
  also knocks out the whole cell it hit. A knocked-out cell keeps a ragged strip of concrete, up to 30 cm wide, along
  its border.
- **Health cap:** the removed area is at most lost health x 0.65 x the wall area. A hit that would pass the cap gets a
  smaller hole.
- **Debris:** D's 8 chunk pieces, one per 0.45 m² removed (at most 12 per hit), scaled to the area each stands for.
- **Collapse:** the pieces sit on a 1.1 m grid over what is still standing. The rest matches D.
- **Shader:** PS 437 against 319 for the opaque master (+118, v2). The intact wall stays opaque until the first hole.
- **v2 check (PIE, debug hits):** 8 hits of 37-150 mm on `_Holes` gave 6 holes and 6 baked cells. The 150 mm hit
  broke its cell, and the overflow baked the oldest hole into cells. Picture: `Saved/WallChunks/holes_v2_check.png`.

### Closed shapes: towers and trees (2026-11-09, v3)

On the walls, debris flew out along the mesh's Y axis, with the shot, and started at random points in the hole
sphere. On a closed tower that sent pieces into the hollow inside, or along an axis unrelated to the face that was
hit, so many hits seemed to throw nothing. Changes:

- **Debris on closed shapes** (`GridAxis = NoGrid`, so towers and trees): pieces start on the struck surface,
  `AnchoredPieceSurfaceOffset` (8 cm) outside it and spread over 0.6 x the hole radius. They fly out along the
  outward direction, which mixes the direction away from the mesh's vertical axis with the direction back toward the
  shooter (`FWallHolePieceAnchor`, `MakePieceAnchor`, `ThrowDebrisPieces(..., OutwardDirection)`). Walls with a grid
  are unchanged.
- **Every hit bites.**
  - A hit inside an existing hole now grows it by at least 0.25 x its own radius.
  - A hole already at `MaxHoleRadius` still throws pieces chipped off its outer rim band (0.8 to 1 x the radius).
  - Before, such hits threw nothing.
- **`HoleCentreOutsetShare`** (`GridSettings`, default 0): moves the hole sphere back toward the shooter by this
  share of its radius. It bites a scoop of radius x (1 - share) deep instead of a ball centred on the surface. Trees
  use 0.5, so a hole doesn't cut a trunk through.
- **Already-masked sources:** `make_wall_holes_material.py` multiplies the source's own opacity mask with the holes
  (`keep_source_opacity`) and keeps its clip value. This covers the camo-net material.

**Guard towers** (all 6):
- **Hole materials on every slot.** The camo slots (`M_Russian_CamoPattern_Tent` and `_Opaque`) had none, so shells
  hitting the nets left no hole. They now use `MI_M_Russian_CamoPattern_Tent_WallHoles` and `..._Opaque_WallHoles`
  in `RusBunkers/Holes`.
- **Soviet MG towers** keep their own `MI_SM_Rus_GuardMgTower_*_Soviet_WallHoles` in an inherited-component override.
  `Rus_BXP_GuardTowerMG` → `_Soviet` → `_Soviet_Propaganda` is a Blueprint chain. Edit inherited components with the
  UECP `edit_component_property` action; Python `SubobjectDataSubsystem` edits on a child write the parent's template.
- **Hole size:** `HoleRadiusPerCalibreMm` 1.1 (was 0.85), so 37 mm gives about 41 cm and 75 mm about 82 cm.
- **Debris, user tuned:** first raised to `AreaPerDebrisPiece` 1200 / max 14. The user found that too much, so it is
  now 3600 / max 5 (two thirds fewer). More visible holes, not more pieces, was what was missing.

**Dead trees** (`BP_Scorch_Dest_DeadTree`, `_DeadTree_Radixite`, `BP_Scorch_Dest_DeadWillow`):
- **Component:** a `WallHoleDamage` component on `StaticMesh`, with:
  - `NoGrid`;
  - `HoleCentreOutsetShare` 0;
  - `HoleRadiusPerCalibreMm` 0.4 (max 40 cm);
  - `AreaPerDebrisPiece` 300, max 8 per hit;
  - `bCollapseIntoChunksOnDeath = false`, so the trees keep their geometry-collection death.
- **Materials:** `MM_RTS_Env_WallHoles` and `MM_RTS_EnvRadixite_WallHoles`, built with `inner_tint`. Back faces are
  darkened toward charred wood (`WallHoleInnerDarken` 0.75, `WallHoleInnerColor` (0.14, 0.085, 0.05)). On the
  Radixite master, back faces emit nothing.
- **Instances:** `DeadTrees/Materials/Holes/MI_Env_DeadTree_1_WallHoles` and `MI_Env_DeadTree_Radixite_WallHoles`.
  The willow twig slot is untouched. PS: 322 → 453 and 333 → 466 (two-sided).
- **Debris:** 6 wood pieces cut from the real trunk (`DeadTrees/Debris/SM_DeadTree_Debris_0..5`, plus Radixite
  copies), made by `Tools/WallChunks/Trees/make_tree_debris.py`.
  - Bark faces keep the tree's UVs; broken faces map to the palest wood strip of the atlas.
  - They render with the masked instances, which carry the Niagara usage flag.
- **Trees die fast** (300 health × 0.7–1.2, damage reduction 5), so a tree shows one to three holes before its
  geometry-collection death.
- **Outset is 0 on trees.** The trees collide with 6 convex hulls (simple as complex), and in places the hull sits
  10 to 20 cm outside the bark. With outset 0.5, a hole centred back from the hull point bit only air there: at 2 m
  on SM_RTS_DeadTree_2, nothing showed. Centred on the hit with radius 0.4 x calibre, all three test holes (75, 88
  and 75 mm at 1.1, 2.0 and 2.9 m) showed without cutting the trunk. Picture:
  `Tools/WallChunks/Trees/tree_holes_v4.png`. Keep the outset for meshes whose collision matches the render surface.
- **Verified in PIE, 2026-11-09:** the live values of all 6 towers are right. Test hits on Rus_BXP_GuardTower made
  large holes through the camo net with the net pattern kept (`Tools/WallChunks/Trees/guardtower_holes_v3.png`).
- **Build:** the tower debris change (anchor, minimum growth, rim chips) is in the full editor build of
  2026-11-09 23:26.

## D on the Soviet bunkers (baked concrete chunks, 2026-11-09)

The user picked A + B for walls and towers, and the cheaper D for the bunkers, which are large. The mega bunkers are
out of scope; they get their own destruction.

- **Only concrete is chunked.** `Tools/WallChunks/fracture_concrete_chunks.py` (Blender 4.0, headless) imports the
  Unreal FBX export and keeps LOD0 only. It cuts the faces of the named concrete materials into 3D Voronoi cells
  (farthest-point seeds on the surface, `bisect_plane` per neighbour plane), caps closed cuts on a new last slot
  `ConcreteBroken`, and leaves every other slot untouched. It writes the chunk id into a new UV layer (127 for
  non-concrete faces), passes the UCX hulls through, and exports up to 8 typical chunks, solidified, as debris
  pieces.
- **Unreal side:** `Tools/WallChunks/bunker_chunks_ue.py`.
  - `import_chunked` makes a fresh import: a reimport into a copy merges slots by name and scrambles them. It removes
    LODs (the mask is LOD0-only), turns Nanite off, and copies simple collision, trace flag, sockets and lightmap
    settings from the source.
  - `make_chunk_instance` builds `<Master>_WallChunks_UV<n>` masked masters, finished by
    `RTS.WallChunks.FinishMaskedMaster`.
  - `build_layout` runs `RTS.WallChunks.BuildLayoutFromMesh`. That reads the chunk ids back from the mesh, so there is
    no coordinate conversion, and picks the debris piece per chunk by area.
- **`UWallChunkDamageComponent` v2:**
  - 3D distances;
  - building-expansion hooks for hits and repairs (repairs bring chunks back, newest first);
  - the construction gate;
  - the chunk mesh found by `ChunkMeshComponentName`, re-checked per hit;
  - `DamagedMaterialOverrides` per Blueprint;
  - `ThrowDeathPieces(MaxPieces, SpeedScale)`, the bunker death: a handful of the bunker's own chunks is shot out
    while the bunker sinks (`VerticalDestruction` with destruction smoke). No crumble.
- **Prototype:** `Rus_BXP_MGBunker` / `SM_RusBunker_MG_Chunks`, 40 chunks, 2.0k → 3.7k tris, PS 338 against 319.
  Debug hits and real fire hid chunks in PIE.
- **Rollout:** `Tools/WallChunks/BUNKER_AGENT_BRIEF.md`, one agent per bunker mesh.

### Rollout result (2026-11-09)

- **Converted:** `Rus_BXP_MGBunker` (40 chunks), `Rus_BXP_AutoCn`, `Rus_BXP_Bunker02_ZIS`, `Rus_BXP_Bunker05`, the Dome
  bunker and its Garrison child, `Rus_BXP_LongCamoBunker`, `Rus_BXP_MGBunkerOneCamoed`, `Rus_BXP_RusBunker03` and its
  Soviet child, and `Rus_Building_AdminBunker`. Most have 100 chunks, the cap.
- **Each one has:**
  - the chunk mesh on `BuildingMeshComponent` and in `BP_InitBuildingExp`;
  - `WallChunkDamage` with no auto collapse;
  - the stage-mesh swaps removed;
  - the geometry collections removed;
  - death by `VerticalDestruction`, destruction smoke and `ThrowDeathPieces`.
- **Runtime check:** every Blueprint was spawned in one PIE session to read the aim points and chunk mesh.
  `Rus_BXP_MGBunker` still showed the C++ default points, because its points were set on the class default and those
  don't apply at runtime. It now sets 8 points in its construction script, and they show at runtime. All the others
  were already right. Set aim points in the construction script, not the class default.
- **Agent fixes:**
  - Copy-paste init meshes (`SM_RusBunker_MG` / `SM_BXP_37mm_BuildingMesh`) fixed on several Blueprints.
  - On ZIS, MGBunker and Admin, the wreck or resource spawn moved after the sink.
  - `Rus_BXP_MGBunkerOneCamoed`'s sink `DeltaZ` changed from 300 to 500.
  - Admin's `MaxHiddenShareWhileAlive` is 0.4.
  - A stray Make Array in `Rus_BXP_RusBunker03`'s construction script was removed.
- **Still open:**
  - Several construction (build-up) meshes are still the 37 mm one.
  - The Admin resource spawn now waits about 7 s.
  - No ruin stays after RusBunker03's sink.

## A + B on the garrison bunkers (2026-11-09)

`BP_Garrison_Bunker_1`, `BP_Garrison_Bunker_2` and `BP_Garrison_RusBunker_1` (`GarrisonableBuildings/`) use the
procedural holes, not D.

- **Hook (C++).** `AGarrisonableBuilding` keeps its health in a `UHealthComponent` and overrides `TakeDamage`, so the
  base forwarding never ran. Its `TakeDamage` now reads the health before the hit and calls
  `ForwardDamageToWallHoles`: `SetOwnerMaxHealth` + `OnOwnerDamaged` with the health actually lost (after the health
  component's armour), health after = 0 on the killing hit. `OnHealthChanged` with healing calls `OnOwnerHealed`. The
  component is looked up per hit (`FindWallHoleDamage`), not cached, so the class layout did not change.
- **Component** `WallHoleDamage` on each Blueprint: `WallMeshComponentName = Bunker`, `NoGrid`,
  `HoleRadiusPerCalibreMm` 1.1, `AreaPerDebrisPiece` 3600 / max 5 (the tower values),
  `bCollapseIntoChunksOnDeath = false`. The death stays the Blueprint's own `VerticalDestruction` sink.
- **Graphs:** the `_At50` `SetStaticMesh` swaps (and their switch) were removed from Bunker_1 and Bunker_2.
  RusBunker_1 still sets the destroyed materials on death, so its holes vanish while it sinks.
- **Materials** (all `two_sided`):
  - German: new masters `M_GerCamoConcretePalet_Atlas_WallHoles` (masked source, its mask kept) and
    `M_GerSandBagScaffold_Atlas_WallHoles`; per-mesh instances in `Bunkers/Holes/` (`MI_SM_GarisonBunker_<n>_GerCamoConcrete_WallHoles`,
    `..._GerSandBag_WallHoles`). PS 457 vs 339 and 438 vs 319.
  - Rus: `RusBunkers/Holes/MI_SM_RusInfBunker_03_RusConcrete_WallHoles` (on `M_RusBunker_Concrete_WallHoles`, the
    Blueprint overrides slot 0 with the plain master) and `..._RusCamo_WallHoles`. PS 437 vs 319, 427 vs 310.
- **Debris:** `Bunkers/Holes/SM_GarisonBunker_Debris_0..7` (shared by both German bunkers),
  `RusBunkers/Holes/SM_RusInfBunker_03_Debris_0..7`.
- **Aim points** are the C++ defaults, (0, 0, 100) and (0, +-50, 100). The bunkers are centred on their origin, so
  shells hit the face toward the shooter around 1 m up; holes spread by the shooter's angle, not by the points.
- **PIE check** (`UT_VehicleObstacle`): debug hits cut holes on all three; real fire from 9 player tanks raised the hole
  counts (Bunker_1 2 -> 6) and killed Bunker_2 and RusBunker_1, which sank normally. Pictures:
  `Saved/WallChunks/garrison_bunkers/holes_overview.png`, `rusbunker_close.png`.

## Side-by-side vehicle test (PIE, `UT_VehicleObstacle`, 2026-11-09)

Both walls stood on flat ground about 25 m south of the map's player vehicles: D at (13000, -3300), A+B at
(15700, -3300). Each wall was attacked by one Tiger II, one Panzerjäger I and one T-26 (`AttackActor`), with 5000
test health set in PIE only. Real shells went through `AProjectile`: both walls lost material where the shells hit and
threw debris. After a few volleys D had lost 7 chunks and A+B had 5 holes. Pictures:
`Saved/WallChunks/vehicles_side_by_side_1.png` and `_2.png`. With their real 1400 health, both walls also died to
fire and collapsed (`vehicles_3.png`).

Not seen: the A cell break needs a hole of 113 cm or more, so a 133 mm+ shell (or holes filling all 8 slots), and none
of the test guns is that big.

**Aim points.** Weapons aim at the target's `AimOffsetPoints`. `Rus_Wall_Concrete_Long`'s construction script set
three points at the wall's end: (-450, 0, 0), (-240, 0, 150) and (-600, 0, 180). The actor origin is the end of
`SM_Wall_14`, so every shell hit the last 6 m at the base. Both prototype walls now use (-500, 0, 300),
(-1130, 0, 200) and (-1760, 0, 320), which spreads the hits over the face. The Soviet child inherits them. The other
concrete walls still have their own construction-script points.

## Ampulomet pit (A + B, 2026-11-10)

`Rus_BXP_AmpulometPit` (`BuildingExpansions/RusAmpulometPit/`) has the same setup as the 53K pits, on its sandbag ring
only (r 160 to 270 cm, z 0 to 71, plus the sandbags around the gun base).

- **Component** `WallHoleDamage`: `BuildingMeshComponent`, `NoGrid`, `DamagedMaterials = [None, None, MI]`, so the
  earth berm (`MI_RTS_DesertGround_Stones`) and the wood keep their materials and get no holes. The hole, grid and
  collapse settings match the 53K pits: 0.85 cm per mm, 4500 / max 12, collapse on death.
- **Material:** `RusAmpulometPit/Meshes/Holes/MI_SM_AmpulometPit_Rus_SandBag_01_WallHoles`, two-sided, on
  `MM_Rus_SandBag_01_WallHoles`. PS 427 against 308.
- **Debris:** `RusWalls/Meshes/Holes/SM_Rus_SandBag_Debris_0..7`.
- **Unchanged:** the graphs, the death (`DestroyAndSpawnActors`) and the aim points (the C++ defaults, 1 m above the
  centre). The mesh's collision (27 convex hulls) sits on the bags: debug traces hit the ring at r 211 to 287.
- **PIE check** (`UT_VehicleObstacle`): debug hits of 37 to 105 mm cut holes through the bags, with the dark inside of
  the bags showing. Real tank fire swapped the slot and cut holes, and killed one pit normally. Pictures:
  `Saved/WallChunks/AmpulometPit/debug_holes_close.png`, `debug_holes_closeup2.png`, `real_fire_3.png`.

**`Rus_BXP_DShKPit`** (`BuildingExpansions/RusDShKPit/`) has the same setup on its sandbag ring (r 120 to 230 cm, z up
to 97): `RusDShKPit/Meshes/Holes/MI_SM_DShKPit_Rus_SandBag_01_WallHoles` (two-sided, PS 427 against 308) on slot 2, earth
(slot 0) and wood (slot 1) untouched. Graphs, death and aim points unchanged. PIE: debug hits of 37 to 105 mm cut holes
in the bags; real tank fire swapped the slot, threw sandbag pieces and killed the pit at its real 720 health, which
then died normally. Pictures: `Saved/WallChunks/DShKPit/debug_holes.png`, `real_fire.png`. Test note: changing
`RTSComponent.owning_player` on a spawned pit at runtime left it taking no damage afterwards; use a fresh actor.

## Large tent (A + B, 2026-11-10)

`Rus_Building_LargeTent` and its child `Rus_Building_LargeTent_Soviet` (`BuildingExpansions/RusBuildings/`), on
`SM_RusLargeTent` (one slot, 19 x 9 x 2.75 m).

- **Component** `WallHoleDamage` on the parent, inherited by the Soviet child: `BuildingMeshComponent`, `NoGrid`,
  `HoleRadiusPerCalibreMm` 1.1 (the tower value, see below), `AreaPerDebrisPiece` 3600 / max 5, collapse on death.
  The Soviet child overrides `DamagedMaterials`, `DebrisMeshes` and `HoleSettings` on the inherited component. The
  override is a full copy, so later changes to the parent's settings do not reach it.
- **Materials** in `RusBuildings/Tent/Holes/`, two-sided on the existing `MM_RTS_Env_WallHoles` (the dead-tree master),
  with the inside darkened (`WallHoleInnerDarken` 0.6, colour (0.06, 0.055, 0.045)): `MI_SM_RusLargeTent_RusTent_WallHoles`
  (copy of `MI_RusTent`) and `MI_SM_RusLargeTent_RusTent_Soviet_WallHoles` (copy of `MI_RusTent_Soviet`, parented to
  the first, so it keeps only its own camo diffuse). PS 437 against 318.
- **Debris:** thin fabric scraps, the 8 hay-shed sheet shards in tent material: `SM_RusLargeTent_Debris_0..7` and
  `SM_RusLargeTent_Soviet_Debris_0..7`.
- **Graph changes:** the empty `GeometryCollection` component (no rest collection, unused) was removed. The
  `BP_InitBuildingExp` building mesh was `SM_RusBunker_MG`, a copy-paste slip that turned the tent into an MG bunker
  whenever construction finished; it is now `SM_RusLargeTent`. The construction mesh is still `SM_BXP_37mm_Construction`.
  The death (`DestroyAndSpawnActors` + rubble obstacle) is unchanged.
- **Aim points** set in the construction script: (0, 0, 120), (±600, 0, 110), (±300, 0, 160), spread along the tent
  instead of the C++ default cluster at the centre.
- **Collision (tightened 2026-11-10):** `SM_RusLargeTent` had 4 convex hulls that wrapped the guy ropes and stakes,
  up to 1.9 m outside the fabric low down, so holes centred on those hits missed the cloth. It now has 3 boxes on the
  fabric itself: the body (x ±721, y ±235, z 0 to 202) and two 60 cm roof slabs (±17.26° roll) up to the
  ridge at 275. Traces land on the walls, roof and ends exactly; the ropes and stakes no longer collide. All 5 test
  hits of 37 to 105 mm now show. The old hulls are in `Saved/WallChunks/LargeTent/SM_RusLargeTent_collision_backup.txt`.
  The hole size stays at 1.1 cm per mm (the tower value): fabric tears wider than sandbags.
- **PIE check** (`UT_VehicleObstacle`): runtime values right on both. Real tank fire tore 6 large holes along each tent
  (dark inside, scraps on the ground), and a Soviet tent at its real 4000 health died normally. Pictures:
  `Saved/WallChunks/LargeTent/real_fire_2.png`, `soviet_real_fire.png`, `base_debug_r11.png`.

## Open points

- The collapse reads as a blast, not a falling wall. Tune `CollapseSettings.DeathSpeedScale`, or add a slumping
  variant. Nothing is left on the ground after 3.6 s; the geometry collection left pieces for 30 s. A rubble mesh
  could stay.
- The holes have clean polygon outlines. The rim band (scorch and cracks) would cost hit positions in CPD; see the
  shader numbers in `WallHitMasking.md`.
- Chunks can float once everything under them is gone.
- Other walls: `SM_Wall_13` (corner, two slabs) needs its own run of the script (two slabs, its own outline). The
  sandbag wall and the refinery walls need a different cut.
- Save/load does not restore holes. Environment walls do not heal.
- **Debris sizing (fixed 2026-11-09).** `BuildLayoutFromMesh` compared a chunk's triangle area, which counts every
  surface the chunk spans, with a debris piece's one-sided bounds area. So nearly every chunk got the largest piece,
  at up to 2×.
  - **Fix:** both are now measured the same way: the product of the two largest bounds sides. The pieces are copies of
    real chunks, so a chunk now maps to a piece of its own size at about 1.0×. `FWallChunk.Area` is unchanged.
  - **Applied:** all 9 bunker layouts were rebuilt with `Tools/WallChunks/rebuild_bunker_layouts.py`. Every bunker now
    uses all 8 pieces, at an average scale of 0.96 to 0.99 (before: one piece took 53 to 99% of the chunks).
  - **Checked:** PIE test hits on the Admin bunker threw chunk-sized pieces (`Tools/WallChunks/Bunkers/admin_debris_fix.png`).
- **LODs:** chunk meshes are LOD0-only (ZIS has 28k triangles at every distance). They need reduced chunk LODs, or the
  original LODs at range with the mask off.
- **Chunk budget:** the fracture tool also cuts undersides and hidden inner faces, so large bunkers get coarse 2 to 4 m
  chunks. Either skip faces that point down or are hidden, or raise the 100-chunk cap with more CPD floats.
- A white dome shows where the smoke renders on `UT_VehicleObstacle`. It may come from the map.
