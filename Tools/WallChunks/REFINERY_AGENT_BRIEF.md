# Refinery buildings: procedural wall holes on the main concrete only (agent brief, 2026-11-10)

The user wants the procedural shell holes (`UWallHoleDamageComponent`, "A + B") on the four refinery building
expansions `Rus_Zone_Refinery1..4` in `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/`,
**on their main concrete material only**. You get **one** Blueprint. Read first:
- `Source/Tools/WallChunks/AGENT_BRIEF.md`: the original rollout brief. Its "Testing" and "Shared-editor rules"
  sections apply in full: one PIE at a time via `Saved/WallChunks/pie.lock`; never close, restart or build the
  editor; save only what you changed; commit nothing.
- `Source/Docs/Environment/WallChunkDamage.md`: sections "A + B: procedural holes", "53K gun pits" and "German gun
  pits", the closest precedents (building expansions, NoGrid, own death kept, repair).
- `Source/RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h`.
- Memories in `C:\Users\basbl\.claude\projects\D--UE5Projects-RTS-Survival\memory\`: `wall-chunk-damage.md`,
  `uecp-editor-bridge-pitfalls.md`, `pit53k-wall-holes.md` (has the "live struct" gotcha below).

## Your assignment (slot decided by the lead; do not change it)

The main concrete is the slot each Blueprint overrides with Russian bunker concrete (measured surface share):

| Blueprint | Mesh (`Meshes/Industrial/Zone/Meshes/SteelRef/`) | Holes slot | `DamagedMaterials` |
|---|---|---|---|
| `Rus_Zone_Refinery1` | `SM_Steel_refinery_09` (9 slots) | 4 (BP override `MI_SovietConcrete`, 11 %) | index 4 = `MI_Refinery_SovietConcrete_WallHoles`, 0-3 None |
| `Rus_Zone_Refinery2` | `SM_Steel_refinery_08` (5 slots) | 2 (BP override `M_RusBunker_Concrete`, 39 %) | index 2 = `MI_Refinery_RusBunkerConcrete_WallHoles`, 0-1 None |
| `Rus_Zone_Refinery3` | `SM_Steel_refinery_07` (5 slots) | 2 (BP override `M_RusBunker_Concrete`, 79 %) | same as Refinery2 |
| `Rus_Zone_Refinery4` | `SM_Steel_refinery_03` (5 slots) | 2 (BP override `M_RusBunker_Concrete`, 3 %) | same as Refinery2 |

Every other slot stays None, so it keeps its material and gets no holes. That includes the industry atlas, metal,
`M_RusIndust_Red` and Refinery4's slot 3: `M_Concrete` uses material attributes, which the library refuses.

## Already built for you (shared, do not rebuild or edit)

- `/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Refinery/Holes/`:
  `MI_Refinery_RusBunkerConcrete_WallHoles` and `MI_Refinery_SovietConcrete_WallHoles` (+ its `_Parent`).
  NoGrid, two-sided, on `M_RusBunker_Concrete_WallHoles`. PS 437 vs 319 opaque.
- Debris: `/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Meshes/Chunks/SM_Wall_14_Chunks_Debris_0..7`
  (concrete pieces; the Military fences use them too).
- C++: hits arrive through `AHPActorObjectsMaster::TakeDamage`. Repairs close holes through
  `UHealthComponent::GetOnHealthHealed()`, and a full repair puts the intact materials back. No C++ change is needed.
  If you think one is, stop and report.

## Per Blueprint

1. **Component** `WallHoleDamage` (`UWallHoleDamageComponent`) on your Blueprint:
   - `WallMeshComponentName = BuildingMeshComponent`;
   - `DamagedMaterials` as in the table (an array as long as your holes slot + 1);
   - `DebrisMeshes` = the 8 pieces above;
   - `GridSettings`: `GridAxis = NoGrid`, `HoleCentreOutsetShare` 0, `AreaPerDebrisPiece` 3600, `MaxPiecesPerHit` 5;
   - `HoleSettings.HoleRadiusPerCalibreMm` 0.85;
   - `CollapseSettings.bCollapseIntoChunksOnDeath = false` (keep the Blueprint's own death), collapse cue /
     attenuation / concurrency copied from `Rus_Wall_SandbagRound`'s component.
   **GOTCHA:** `get_editor_property('GridSettings')` (or any struct) on a component *template* returns a LIVE
   reference. Editing it to copy into your Blueprint writes the SOURCE Blueprint. Build fresh structs, or
   `import_text` / `export_text`, and check that the source's dirty flag stays clear.
2. **Graphs:** read `Bp_OnHealthChanged` and `BP_OnUnitDies`. Health notify levels are empty, so
   `Bp_OnHealthChanged` never fires today. Report what it would do (mesh or material swaps), but don't change it. If
   the death or any other graph swaps `BuildingMeshComponent`'s mesh or sets its materials while the building is
   alive, report it; the holes depend on the mesh and slot order. Don't touch the pipe spline spawners or
   `HealthLevelDecal`.
3. **Aim points** (`AimOffsetPoints`, actor-local): check that they spread over the concrete part that faces
   shooters. If they all sit on atlas or metal surfaces, holes will rarely show. Report them, and propose better
   points if needed; only change them if they are clearly broken, such as all at the origin, and set them in the
   construction script (class-default edits don't apply at runtime).
4. Compile and save only your Blueprint.

## Testing (required, PIE lock first)

Map `UT_VehicleObstacle`. Spawn with `spawn_pie_actor`, place behind the south wall on flat ground around
(12500..15500, -7000..-8800). Trace for the ground, ignoring other actors. Another session's three German pits are
saved in that map near there; leave them alone.
- **Debug hits** on the concrete: trace from outside toward the building, keep only hits whose component is
  `BuildingMeshComponent`, then `debug_fire_test_hit(point, dir, 75)`. Hits on non-concrete slots show nothing, by
  design. Take a picture showing holes in the concrete, the inside (two-sided) and the debris.
- **Repair:** set health to 30 % (`HealthComponent.set_current_health`), make holes, then heal through the real path:
  repeat `BehaviourComp.remove_behaviour(SingleHealBehaviour)` + `add_behaviour(SingleHealBehaviour)` (10 HP each)
  up to 100 %. Expect `get_hole_count() == 0` and every slot back on its original material. Compare with the
  materials read *before* the first hit, since the Blueprint overrides some slots.
- Pictures: `D:/UE5Projects/RTS_Survival/Saved/WallChunks/Refinery/<your-refinery>/`. Look at them with Read.
- Stop PIE and release the lock within about 5 minutes.

## Report back (short)

- what you changed (component values, anything else);
- the `Bp_OnHealthChanged` / death findings and the aim points (current and proposed);
- debug-hit and repair results with numbers, picture paths and what they show;
- anything doubtful or left undone.
