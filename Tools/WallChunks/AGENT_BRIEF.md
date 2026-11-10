# Wall holes rollout: shared brief for agents (2026-11-09)

The user picked the **procedural holes** prototype (A + B) for wall damage. Your job is to put it on your assigned
Blueprints. Read these first:
- `Source/Docs/Environment/WallChunkDamage.md`, the "A + B: procedural holes" section and "Side-by-side vehicle test".
- `Source/Docs/Environment/WallHitMasking.md`.
- `Source/RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h`. This is the
  component; its doc comments say what each setting does.
- `Source/Tools/WallChunks/make_wall_holes_material.py`. This is the material library; its module docstring is the
  manual.
- `Source/AGENTS.md`, the "Local Unreal Editor Connection" section, and the memories in
  `C:\Users\basbl\.claude\projects\D--UE5Projects-RTS-Survival\memory\`: `wall-chunk-damage.md`,
  `uecp-editor-bridge-pitfalls.md`, `ue-python-material-gotchas.md`.

**The reference implementation is `/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Rus_Wall_Concrete_Long_Holes`.**
Read its components, its `WallHoleDamage` settings and its graphs, and copy the pattern. Use the UECP tools
`get_blueprint_skeleton`, `get_graph_intent` and `execute_python`, plus `SubobjectDataSubsystem` to reach component
templates.

## How it works (already built; no C++ changes)

- `UWallHoleDamageComponent` holds up to 6 sphere holes in the wall mesh's local space, plus an optional grid of
  knock-out cells on (along, Z). Along means mesh X or Y. It writes them to custom primitive data 8 to 34. A masked
  `_WallHoles` material clips them.
- Hits are forwarded automatically:
  - environment walls, `ADestructableEnvActor` (the `Rus_Wall_*` Blueprints): through `TakeDamage`;
  - building expansions and towers, `AHPActorObjectsMaster`: through `TakeDamage`. Repairs close the newest holes
    (`OnHealthChanged` with healing).
- Death, with `CollapseSettings.bCollapseIntoChunksOnDeath`:
  - environment walls: every piece left flies off, the mesh hides, and the actor is destroyed after
    `DestroyOwnerDelaySeconds`;
  - building expansions: the pieces fly but the mesh stays for the BXP's own death (vertical sink). You can also call
    `CollapseIntoPieces(bHideAndDestroyOwner)` from the Blueprint yourself and turn the automatic one off.
- The intact wall stays opaque. The first hole swaps each slot to `DamagedMaterials[slot]`. An empty or None entry
  keeps that slot's material, and that slot then gets no holes.
- When the mesh changes (BXP construction mesh -> final mesh), the component re-reads the bounds and grid on the next
  hit.

## Per Blueprint

1. **Component.** Add `WallHoleDamageComponent` (name it `WallHoleDamage`) where the family's parent can carry it,
   once per class hierarchy; children inherit it. Set:
   - `WallMeshComponentName`: `StaticMesh` on `Rus_Wall_*`; on building expansions it is probably
     `BuildingMeshComponent` (check the runtime component name).
   - `DamagedMaterials`: one entry per mesh slot, in slot order.
   - `DebrisMeshes`: 8 pieces (see Materials).
   - `GridSettings.GridAxis`: `AlongX`, `AlongY` or `NoGrid`. Use the axis the wall runs along: the longest
     horizontal bounds axis. Use `NoGrid` for towers, L-shapes and other non-flat shapes.
   - `GridSettings.CellSize`, default 150; it must match the instance.
   - `CollapseSettings`: copy the sound, attenuation and concurrency from the reference.
   Children that override the mesh's materials, such as the Soviet variants, need their own `DamagedMaterials`
   override on their inherited component.
2. **Geometry collections out.** Remove `GeometryCollection` components and the `CollapseMesh` calls (and the
   make-struct nodes feeding them) from the death graphs. Keep explosions, decals and the other effects.
3. **`_At50` mesh swaps** (building expansions, `Bp_OnHealthChanged`): remove the `SetStaticMesh` to the damaged
   mesh, because the holes now show the damage and the swap would change the bounds. Keep the explosions.
4. **Aim points.** Weapons aim at `AimOffsetPoints`. They are set by a construction script, `BeginPlay` or
   `BP_InitBuildingExp`, or come from the class default. Check that the points spread over the shootable faces, in
   actor-local space. The actor origin may be at a mesh end or corner, and then every shell hits one spot. Fix them
   in place, the same way the reference wall's construction-script `Make Array` was edited.
5. Compile and save only the assets you changed.

## Materials

```python
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/make_wall_holes_material.py', encoding='utf-8').read())
mi = make_holes_instance(slot_material_path, folder, mesh, grid_axis='X'|'Y'|None, cell_size=150.0,
                         two_sided=False, name='MI_<Mesh>_<SlotMaterial>_WallHoles')
print(check_mask_live(mi.get_base_material()))   # (masked PS, opaque PS); masked must be ~100+ higher
pieces = make_debris_pieces(folder, '<Mesh>', mi)  # 8 concrete chunk pieces in this wall's look
```

- Grid parameters depend on the mesh, so name each instance after both mesh and material. Two agents may share a
  material, but never an instance.
- The masked master `<Master>_WallHoles` is built once next to its source and shared, for example
  `M_RusBunker_Concrete_WallHoles`. Never rebuild or clear a master that instances already use (that crashes the
  editor).
- **Thick or hollow meshes** (the walkable refinery walls, 2.6 m deep; towers): set `two_sided=True`, so you see the
  inside through a hole instead of the far side vanishing. Judge it in pictures.
- **Already-masked sources** (barbed wire, foliage, grilles): the library replaces the opacity mask. Leave those slots
  out of `DamagedMaterials` unless you merge the masks yourself.
- If a master uses material attributes or Substrate, the library raises an error. Report it, don't hack around it.

## Testing (required)

PIE in the shared editor, map `/Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle`:
- **The PIE lock comes first.** Only one PIE may run at a time across all agents. Take it with
  `mkdir D:/UE5Projects/RTS_Survival/Saved/WallChunks/pie.lock`; mkdir is atomic, and if it already exists another
  agent holds it. Wait in 30 s steps with PowerShell `Start-Sleep`, then try again. Release it with
  `rmdir D:/UE5Projects/RTS_Survival/Saved/WallChunks/pie.lock` right after `stop_play_in_editor`. Keep each session
  under about 5 minutes.
- **Starting PIE.**
  - Load the map with `unreal.EditorLoadingAndSavingUtils.load_map`, only if it isn't the open map.
  - Call `begin_play_in_editor(new_window=true, ignore_blueprint_errors=true)`.
  - An "Error, Suppress errors?" dialog (ARTSFireManager) appears; answer `dismiss_editor_dialog(button='Yes')`.
  - Then `editor_ui_click(locator={"role":"button","name":"Press To Start"})`.
- **Placing actors.** Use `spawn_pie_actor(actor_class='/Game/...X.X_C', location, rotation)`. Flat ground is at
  y = -3300, x from 11000 to 16000, z about 95. The player vehicles stand around x 14500 to 16500, y -1800 to 0. A
  `CameraActor` with `set_view_target_with_blend` gives a close view.
- **Debug hits:** `comp.debug_fire_test_hit(world_point, shot_dir, calibre)`.
- **Real fire:** `pawn.attack_actor(target, True)` on player tanks. Find them with
  `GameplayStatics.get_all_actors_of_class(world, Pawn)` filtered by an RTS component (Tiger II, Panzerjäger I,
  T-26). Raising the target's health in PIE (`Health` on environment walls; for BXPs use the health component) lets
  damage build up.
- **Pictures:** `take_pie_screenshot(file_path='D:/UE5Projects/RTS_Survival/Saved/WallChunks/<your-tag>/<name>.png')`,
  then look at them with Read. Check the holes, the inside of thick walls, the debris colour, the death, and that the
  intact wall looks unchanged.

## Shared-editor rules (strict)

- Never close, restart or build the editor or its C++. Never launch a second editor. Other agents and the user work
  in this editor. If you need a C++ change, stop and report exactly what and why.
- Save only packages you created or changed. Check dirty packages before saving. Never save or discard other
  packages; the user hand-edits voice lines and missions here.
- Don't touch the other agents' Blueprints. Masked masters are shared, so only create them, don't edit them.
- Commit nothing.

## Report back

For each Blueprint:
- what changed: components, graph nodes, aim points, materials;
- the instances and debris created;
- the PS counts from `check_mask_live`;
- the picture paths and what they show;
- anything left undone or doubtful. Name gameplay changes in particular, for example "the At50 swap was removed".
