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
| Material | `Meshes/Holes/M_RusBunker_Concrete_WallHoles`, built by `Tools/WallChunks/make_wall_holes_material.py` and then `RTS.WallChunks.FinishMaskedMaster`. It is a masked copy of `M_RusBunker_Concrete` with one Custom node. The instance is `MI_RusBunkerConcrete_05_WallHoles`: SandStrength 0.568, plus the grid shape of `SM_Wall_14` (MinX -2260, MaxX 0, MinZ 0, MaxZ 500, cell 150, 15 x 3). |
| Custom primitive data | 8-10: grid cells, one bit per cell, 20 per float. 11-34: 8 holes as (mesh X, mesh Z, radius); radius 0 means the slot is unused. |

- **B, holes.** A hit cuts a round hole at the impact, using the same radius rule as D. The noise warps its edge, and a
  scorched, rougher rim surrounds it.
  - A hit inside an existing hole widens that hole instead (distance + 0.7 x the radius).
  - There are 8 slots. When they are full, the oldest hole is baked into the grid cells it covers, which frees its slot.
- **A, cells.** The grid cells are 1.5 m. A hole at least 0.75 of a cell wide (113 cm, so shells of about 133 mm and up)
  also knocks out the whole cell it hit. A knocked-out cell keeps a ragged strip of concrete, up to 30 cm wide, along
  its border.
- **Health cap:** the removed area is at most lost health x 0.65 x the wall area. A hit that would pass the cap gets a
  smaller hole.
- **Debris:** D's 8 chunk pieces, one per 0.45 m² removed (at most 12 per hit), scaled to the area each stands for.
- **Collapse:** the pieces sit on a 1.1 m grid over what is still standing. The rest matches D.
- **Shader:** PS 432 against 319 for the opaque master (+113). The intact wall stays opaque until the first hole.

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
