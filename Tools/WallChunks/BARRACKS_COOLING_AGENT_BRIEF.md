# Barracks and cooling towers: procedural wall holes (A + B), agent brief (2026-11-10)

The user wants the procedural shell holes (`UWallHoleDamageComponent`) on two Soviet building expansions in
`/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/`, **on one shader only**:

| Blueprint | Mesh | Slot that gets holes | Slots that stay untouched |
|---|---|---|---|
| `Rus_Building_Barracks` | `BarracksMeshes/SM_Rus_Barrack` | 0 = the barracks shader, `MI_Barrack` (BP override; base `M_MasterPropMaterial`, already Masked) | 1 sandbags (`MI_Rus_SandBag_01`) |
| `Rus_Building_CoolingTowers` | `CoolingTowersMeshes/SM_Rus_IndustrialCooling` | 0 = the cooling-tower shader, `M_CoolingTowers` (BP override) | 1 KV-1E turret head, 2 sandbags |
| `Rus_Building_CoolingTowers_Soviet` (Blueprint child of the above) | same | 0 = `MI_CoolingTowers_Soviet` (BP override, base `M_CoolingTowers`) | same |

Untouched slots get **None** in `DamagedMaterials` (that slot keeps its material and gets no holes).

Read first:
- `Source/Tools/WallChunks/AGENT_BRIEF.md` (the main rollout brief: component, graphs, aim points, materials, testing,
  and the strict **shared-editor rules**, which apply in full).
- `Source/Docs/Environment/WallChunkDamage.md`: "A + B: procedural holes", "Closed shapes: towers and trees",
  "German gun pits" (latest precedent).
- `Source/RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h`.
- Memories in `C:\Users\basbl\.claude\projects\D--UE5Projects-RTS-Survival\memory\`: `wall-chunk-damage.md`,
  `german-pit-holes.md`, `uecp-editor-bridge-pitfalls.md`, `ue-python-material-gotchas.md`.

## Settings (as the towers and German pits)

- `WallHoleDamage` component, `WallMeshComponentName = BuildingMeshComponent`.
- `GridSettings.GridAxis = NoGrid` (closed buildings, not flat walls), `HoleCentreOutsetShare = 0`,
  `AreaPerDebrisPiece = 3600`, `MaxPiecesPerHit = 5` (these two live in `GridSettings`).
- `HoleSettings.HoleRadiusPerCalibreMm = 0.85` (the user's pick for the pits).
- Materials: `make_wall_holes_material.py` (`make_holes_instance(..., grid_axis=None, two_sided=True)` unless pictures
  say otherwise). Put instances and debris in a `Holes` folder next to the source materials. Name instances
  `MI_<Mesh>_<Material>_WallHoles`. A new `<Master>_WallHoles` master is built once next to its source; never rebuild
  or clear an existing master. `M_MasterPropMaterial` is a shared prop master: building its `_WallHoles` copy is fine
  (additive), editing the original is not. Check `check_mask_live` and that the masked master has
  `used_with_niagara_mesh_particles` (debris renders with it).
- Debris: `make_debris_pieces(folder, '<Mesh>', mi)` in the holed shader's look.
- Death: follow AGENT_BRIEF for building expansions. Both Blueprints have a `GeometryCollection` component: remove it
  and its `CollapseMesh` calls and make-struct nodes; keep explosions, smoke, decals, sounds. Use
  `bCollapseIntoChunksOnDeath = true` (pieces fly, the mesh stays for the BXP's own sink) unless the death graph makes
  that wrong; report what the death does now. Remove `_At50` / damage-stage `SetStaticMesh` swaps in
  `Bp_OnHealthChanged` (keep the effects). If the death or a health stage swaps materials on the mesh, report it.
- Aim points: both use the class default (0,0,100)... in the middle of the building. Spread 3-4 points over the
  shootable faces of the holed shader in actor-local space, set in the **construction script** (append make_array +
  set `AimOffsetPoints`; class-default edits did not apply at runtime earlier).
- Check `BP_InitBuildingExp`'s mesh pins: the final mesh must be the one you set materials for. Report copy-paste bugs.

## Testing (required)

PIE in the shared editor on `/Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle`, PIE lock first (AGENT_BRIEF
"Testing"). Debug hits (`debug_fire_test_hit`) on several faces of the holed shader, pictures with
`take_pie_screenshot` (check `get_editor_dialog` first; never screenshot with a dialog open), looked at with Read:
holes visible, the inside reads sensibly (two-sided), untouched slots unchanged, debris colour right. Keep each PIE under
5 minutes and release the lock right after `stop_play_in_editor`. The lead runs the real-fire check afterwards.
Pictures to `D:/UE5Projects/RTS_Survival/Saved/WallChunks/<Barracks|CoolingTowers>/`.

## Report back (short)

Per Blueprint: component + settings read back, DamagedMaterials by slot, materials/debris created with PS counts,
graph nodes removed, death behaviour now, aim points before/after, picture paths and what they show, anything doubtful.
