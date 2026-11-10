# German gun pits: procedural wall holes (A + B), agent brief (2026-11-10)

The user wants the procedural shell holes (`UWallHoleDamageComponent`, the same as the Soviet walls, towers and the
German garrison bunkers) on the eight German pit building expansions in
`/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/`. You get **one** Blueprint. Read first:
- `Source/Tools/WallChunks/AGENT_BRIEF.md` (the original rollout brief; its "Shared-editor rules" apply in full).
- `Source/Docs/Environment/WallChunkDamage.md`, sections "A + B: procedural holes", "Closed shapes" and
  "A + B on the garrison bunkers" (the closest precedent: German atlas materials, BXP, own death kept).
- `Source/RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h`.
- Memories in `C:\Users\basbl\.claude\projects\D--UE5Projects-RTS-Survival\memory\`: `wall-chunk-damage.md`,
  `uecp-editor-bridge-pitfalls.md`, `uecp-synthesize-graph-quirks.md`.

## User requirement (verbatim intent)

**Holes go on the pit static mesh only** (`BuildingMeshComponent`). Do NOT touch the infantry crew skeletal meshes,
the animations, the gun / weapon (`Howitzer` ChildActorComponent, anything in `Turrets/`, `Crew/`, `Anims/`), or
any other component's materials.

## Already built for you (shared, do not rebuild or edit)

Folder `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Meshes/Holes/`. All are NoGrid and
two-sided, so they are valid for every pit mesh:

| Slot material on the pit mesh | Hole instance (`DamagedMaterials` entry) |
|---|---|
| `M_GerCamoConcretePalet_Atlas` (slot name `GerPit_Atlas`) | `MI_GerPit_GerCamoConcrete_WallHoles` |
| `M_GerSandBagScaffold_Atlas` (`GerPit_Sandbag`) | `MI_GerPit_GerSandBag_WallHoles` |
| `M_Ger_AmmoBox` (`GerPit_Ammo`) | `MI_GerPit_GerAmmoBox_WallHoles` |
| `M_Flak88_Ammo` (`Flak88Pit_Shell`, Flak 88 pit only) | `MI_GerPit_Flak88Ammo_WallHoles` |

Debris: `SM_GerPit_Debris_0..7` (sandbag look) in the same folder. PS masked vs opaque: 457/339, 438/319, 441/323,
409/290. No C++ change is needed: `ABuildingExpansion` inherits `AHPActorObjectsMaster::TakeDamage`, which already
forwards hits and repairs to a `UWallHoleDamageComponent` found on the actor.

## Per Blueprint

1. **Which mesh is final.** The component template mesh may be a placeholder. Read `BP_InitBuildingExp` (and any
   other `SetStaticMesh` on `BuildingMeshComponent`) for the mesh the pit actually ends up with. Known oddity: the
   Mortar pit's component shows `SM_GerFlak88Pit` while `SM_GerMortarPit` exists; report which one it really uses
   and whether it looks like a copy-paste bug (do not change the mesh unless it is clearly a bug, and say so).
2. **Component.** Add a `WallHoleDamageComponent` named `WallHoleDamage` to the Blueprint (each pit is a direct
   `BuildingExpansion` child, there is no shared parent). Settings, copied from the garrison bunkers:
   - `WallMeshComponentName = BuildingMeshComponent`
   - `DamagedMaterials`: one entry per slot of the **final** mesh, in slot order, from the table above.
     Check the slot list yourself (Panzerschreck has no Atlas slot; Flak 88 has a 4th shell slot). If the Blueprint
     overrides a slot material on `BuildingMeshComponent`, report it and make sure the hole instance matches the
     look.
   - `DebrisMeshes = SM_GerPit_Debris_0..7`
   - `GridSettings.GridAxis = NoGrid`, `HoleCentreOutsetShare = 0`
   - `HoleSettings.HoleRadiusPerCalibreMm = 1.1`, `AreaPerDebrisPiece = 3600`, max debris per hit 5 (the tower /
     garrison values; find the exact property names in the header).
   - `CollapseSettings.bCollapseIntoChunksOnDeath = false`; copy sound, attenuation and concurrency from
     `/Game/RTS_Survival/Blueprints/Buildings/GarrisonableBuildings/Bunkers/BP_Garrison_Bunker_1`'s `WallHoleDamage`.
3. **Graphs.** Look at `Bp_OnHealthChanged`, `BP_OnUnitDies` / death graphs and the construction script:
   - Remove any `_At50` / damage-stage `SetStaticMesh` swaps of `BuildingMeshComponent` (they change the bounds and
     would undo the holes). Keep explosions, sounds and other effects on those health levels.
   - Remove geometry-collection components and their `CollapseMesh` calls if any exist.
   - Keep the pit's own death. If it runs `DestroyAndSpawnActors` or spawns a wreck, keep it and report it. If the
     death swaps materials on the pit mesh (holes vanish), report it.
4. **Aim points.** All pits use the class default `AimOffsetPoints` (0,0,100), (-50,0,100), (0,+-25,100): the
   centre of the open pit, where the gun stands. Shells then hit the gun or fly over to the far rim. Move them onto
   the pit's rim (sandbag ring / parapet) at wall height, 3 to 4 points spread round the ring, from the final mesh's
   bounds and a look at the mesh. Set them in the **construction script** (append: `make_array` of vectors +
   set `AimOffsetPoints`), because class-default edits did not apply at runtime on an earlier bunker. If the
   construction script already sets them, edit those values instead. Check in a non-PIE way that the graph compiles.
5. **Compile and save only your Blueprint.** Check `get_dirty_content_packages` before saving; never save others.

## Testing

Do NOT run PIE. The lead runs one PIE session with all eight pits afterwards. Instead, before you finish:
- compile with no errors, save;
- print the CDO / component template values you set (DamagedMaterials paths in slot order, mesh component name,
  grid axis, radius, debris count, collapse flag) and the construction-script aim points.

## Shared-editor rules (strict)

Never close, restart or build the editor or its C++; never launch a second editor; no PIE. Save only your Blueprint.
Do not touch the other seven pits, the shared hole instances, masters or debris. Commit nothing. If something needs
a C++ change or a shared asset change, stop and report.

## Report back (short)

- Final mesh and its slots; DamagedMaterials set.
- Graph nodes removed (name them) and anything kept that matters (wreck spawn, material swap on death).
- Aim points before / after.
- Anything doubtful.
