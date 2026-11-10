# Bunker chunk rollout: shared brief for agents (2026-11-09)

The user wants the Soviet bunkers on the **cheaper baked-chunk approach (option D)**: the concrete is cut into Voronoi
chunks once, baked into the mesh, and a hit hides the chunks around it through a cheap masked material. Bunkers are
large, so this is cheaper than the procedural holes the walls use.

## The user's requirements (all of them, exactly)

1. **Only the concrete shader parts are chunked and masked.** Sandbags, camo nets, tents, metal, guns, crates and any
   other slot stay exactly as they are, with their own materials, no mask and no chunks. The fracture tool only cuts
   faces whose material you name, and only concrete slots get masked materials.
2. **The damage-stage meshes go.** Remove every At33/At50/At66 (and similar) `SetStaticMesh` swap from
   `Bp_OnHealthChanged`. The chunks are the damage now. Keep explosions and other effects on those health levels. Also
   remove the "100%" `SetStaticMesh` back to the original mesh: it would swap the chunk mesh out after a repair.
3. **Death is a vertical sink, not a crumble.**
   - Use `VerticalDestruction` (on `ABuildingExpansion`) with a `CollapseFX` that plays the destruction smoke. Look
     for `NS_DestructionSmoke`; `Rus_BXP_LargeGuardTower`'s `BP_OnUnitDies` already does this and is the reference.
   - In the same death graph, call `WallChunkDamage.ThrowDeathPieces(MaxPieces, SpeedScale)`. A handful of the
     bunker's own chunks is shot out of it as debris (pieces cut from its own mesh) while the rest sinks.
   - **Do not crumble or hide the bunker.** Set `CollapseSettings.bCollapseIntoChunksOnDeath = false`, so the
     component's automatic throw-everything collapse never runs.
   - Remove `GeometryCollection` components and their `CollapseMesh` calls, with their make-struct nodes.
   - Death currently often runs `DestroyAndSpawnActors` (an explosion plus a `..._Destroyed` wreck actor). Replace it
     with the sink, keeping explosion sound and FX where they fit. Say in your report what happened to the wreck spawn
     and whether it is still wanted (for example spawned after the sink). Don't silently drop gameplay.
4. **Mega bunkers are out of scope** (`Rus_BXP_MegaBunker_*`). Don't touch them.

## What exists (built and verified; no C++ changes by you)

- `UWallChunkDamageComponent` (`Source/RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/`). Read its
  header.
  - Building expansions forward hits and repairs to it (`AHPActorObjectsMaster`). Repairs bring chunks back, newest
    first.
  - Hits are ignored while the bunker is being built, placed or packed.
  - Distances are in 3D, in mesh space.
  - Settings:
    - `ChunkMeshComponentName`: set it to `BuildingMeshComponent`.
    - `DamagedMaterialOverrides`: per-Blueprint masked material by slot, for variants such as the Soviet ones that
      override the concrete.
    - `HoleSettings`: hole sizes.
    - `CollapseSettings`: set `bCollapseIntoChunksOnDeath = false`.
  - `ThrowDeathPieces(MaxPieces = 24, SpeedScale = 1.4)` is BlueprintCallable.
  - `Debug_FireTestHit(world point, shot direction, calibre)` is for tests.
- `UWallChunkLayout` is asset user data on the chunk mesh: chunk centres and areas, the debris piece per chunk, and
  the damaged material per slot. It is built by `RTS.WallChunks.BuildLayoutFromMesh`.
- **Prototype:** `Rus_BXP_MGBunker` with `RusMgBunkerMeshes/Chunks/SM_RusBunker_MG_Chunks`. It has 40 chunks, chunk ids
  in UV1, the masked instances `MI_RusBunker_MG_Concrete_Chunks` and `MI_RusBunker_MG_ConcreteBroken_Chunks`, and
  debris `SM_RusBunker_MG_Chunks_Debris_0..7`. Its Blueprint already has: the chunk mesh on `BuildingMeshComponent`
  and in `BP_InitBuildingExp`, the component (`BuildingMeshComponent`, no auto collapse), and the At50/100 swaps
  removed. Its death is NOT converted yet. Picture: `Saved/WallChunks/Bunkers/mg_bunker_chunks_1.png`.

## Pipeline per bunker mesh

Work folder: `D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Bunkers/<Mesh>/` (create it with `mkdir -p` from a
shell). Put Unreal assets in a `Chunks` subfolder next to the source mesh.

```python
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/bunker_chunks_ue.py', encoding='utf-8').read())
export_source('/Game/.../SM_X', 'D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Bunkers/SM_X')
```

```bash
D=D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/Bunkers/SM_X
/d/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/fracture_concrete_chunks.py -- \
  $D/SM_X_Source.fbx $D SM_X_Chunks <concrete material names, comma separated> chunks=<n> seed=1
```

```python
chunked, debris = import_chunked('/Game/.../SM_X', D, '/Game/.../Chunks')
concrete = make_chunk_instance(<concrete slot material, as the Blueprint shows it>, folder, uv, 'MI_X_Concrete_Chunks', two_sided=True)
broken = make_chunk_instance(<same>, folder, uv, 'MI_X_ConcreteBroken_Chunks', broken=True, two_sided=True)
cap = [str(s.material_slot_name) for s in chunked.static_materials].index('ConcreteBroken'); chunked.set_material(cap, broken)
set_debris_material(debris, concrete); print(check_mask_live(concrete.get_base_material()))
build_layout(chunk_mesh_path, [<concrete slot indices>, cap], [debris paths], [damaged material per slot, '' for others], uv)
```

- **Concrete material names** are the FBX material names, which are the slot materials' asset names, for example
  `MI_Rus_BunkerConcrete_05` or `MI_SovietConcrete`. Check the report JSON: `concrete_slots`, `slots`,
  `chunk_uv_channel` (pass that as `uv`), `triangles_after`.
- **Chunk count:** aim for chunks of about 1 to 1.5 m (use `concrete_area_m2` in the report) and at most 100. The
  prototype has 164 m² of concrete in 40 chunks.
- **Which material to mask:** the one the bunker actually shows. If the Blueprint overrides the concrete slot (for
  example `M_RusBunker_Concrete` or `MI_SovietConcrete`), make the instance from that override. Variants that
  override it differently get their own instance in `DamagedMaterialOverrides`.
- `import_chunked` makes a **fresh import** every run. Slots used only by LOD1+ drop out and the chunk mesh has one
  LOD, so slot indices after them can shift (the warning prints both lists). **Re-check every Blueprint material
  override index** against the chunk mesh's slot list and fix them. Simple collision and sockets are copied from the
  source; check the printed hull and socket counts against the source.
- **Two-sided:** the concrete is mostly open shells, so cap faces only appear on closed cuts. Without two-sided, a
  missing chunk lets you see through to the far side's back faces. Keep `two_sided=True` unless pictures say
  otherwise.

## Per Blueprint

1. `BuildingMeshComponent` mesh = the chunk mesh (`set_static_mesh_component`), and `BP_InitBuildingExp`'s
   `NewBuildingMesh` pin = the chunk mesh (`set_pin_default`). If that pin held a different mesh, report it: earlier
   agents found copy-paste bugs where towers pointed at `SM_RusBunker_MG`.
2. Add `WallChunkDamageComponent` named `WallChunkDamage`. Set `ChunkMeshComponentName = BuildingMeshComponent` and
   `bCollapseIntoChunksOnDeath = false`; add `DamagedMaterialOverrides` on variants.
3. Remove the stage-mesh swaps (requirement 2), convert the death (requirement 3), and remove the geometry
   collections.
4. **Aim points:** check `AimOffsetPoints` (set in BeginPlay's Make Array, the construction script or the class
   default) and spread them over the shootable concrete faces in actor-local space.
5. Compile and save only your assets.

## Testing (required), shared editor

- **The PIE lock comes first.** Take it with `mkdir D:/UE5Projects/RTS_Survival/Saved/WallChunks/pie.lock`; if that
  fails, wait 30 s with PowerShell `Start-Sleep` and retry. Release it with `rmdir` right after `stop_play_in_editor`.
  Keep each session under 5 minutes.
- **Never call `take_pie_screenshot` while a message box is open.** That crashed the shared editor earlier today. Run
  `get_editor_dialog` first and dismiss any dialog. If the "Bomb tracker" box keeps coming back, use another map or
  stop.
- **Starting PIE:** load `/Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle` if it isn't open. Call
  `begin_play_in_editor(new_window=true, ignore_blueprint_errors=true)`. Answer the ARTSFireManager "Suppress errors?"
  dialog with Yes, then `editor_ui_click(locator={"role":"button","name":"Press To Start"})`.
- **Placing actors:** `spawn_pie_actor('/Game/.../X.X_C', location, rotation)`. Flat ground is at y -3300, x 11000 to
  16000. The map's player tanks attack enemy bunkers on their own (real fire), and can be ordered with
  `pawn.attack_actor(target, True)`. A `CameraActor` with `set_view_target_with_blend` gives a close view.
- **Required checks:**
  - intact look unchanged;
  - debug hits on several faces, with chunks gone and debris from the bunker's own pieces;
  - real fire;
  - the death: sink, smoke, and shot-out pieces, with no crumble;
  - non-concrete slots untouched.
- **Pictures:** `take_pie_screenshot(file_path='D:/UE5Projects/RTS_Survival/Saved/WallChunks/Bunkers/<tag>/<name>.png')`,
  then look at each one with Read.

## Shared-editor rules (strict)

- Never close, restart or build the editor or the C++. Never launch a second editor. If you need a C++ change, stop
  and report.
- Save only packages you created or changed. Never save or discard anything else; the user edits in this editor.
- Never edit, in the editor world, a map that isn't yours to change. Spawn test actors in PIE only.
- Don't touch other agents' Blueprints. Masked masters (`<Master>_WallChunks_UV<n>`) are shared: create them, never
  edit them.
- Commit nothing.

## Report back

For each Blueprint, give:
- what changed: components, mesh, the death graph, the removed stage swaps, aim points, material override fixes;
- the assets created, with chunk count, triangles before and after, and the PS counts;
- the picture paths and what they show;
- every gameplay change, such as the wreck spawn or death timing;
- anything left undone or doubtful.
