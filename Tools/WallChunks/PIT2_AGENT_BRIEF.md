# Remaining weapon pits: procedural wall holes, agent brief (2026-11-12)

The user asked for the procedural shell holes (`UWallHoleDamageComponent`) on every weapon-pit building expansion
that still lacks them. 12 pits already have them (8 `Ger_BXP_GerPit_*`, `Rus_BXP_53KPit`, `Rus_BXP_53KSectorPit`,
`Rus_BXP_AmpulometPit`, `Rus_BXP_DShKPit`). Those are your references. Read first:
- `Source/Tools/WallChunks/PIT_AGENT_BRIEF.md`, the German pit brief. Its per-Blueprint steps 1-5 (final mesh, component,
  graphs, rim aim points in the construction script, compile + save only your BP) apply here, **except where this brief
  says otherwise**.
- `Source/Docs/Environment/WallChunkDamage.md`, sections "53K gun pits", "Ampulomet pit", "German gun pits".
- `Source/RTS_Survival/Environment/DestructableEnvActor/WallChunkDamage/WallHoleDamageComponent.h` and `.cpp`
  (`OnOwnerDamaged`, `ApplyHit`). The hit point comes from the damage event and is clamped to the target mesh's bounds;
  there is no hit-component filter and no trace.
- Memories in `C:\Users\basbl\.claude\projects\D--UE5Projects-RTS-Survival\memory\`: `pit53k-wall-holes.md`,
  `german-pit-holes.md`, `ampulomet-pit-holes.md`, `uecp-editor-bridge-pitfalls.md`, `uecp-synthesize-graph-quirks.md`,
  `test-headless-not-shared-editor.md`, `uecp-second-editor-instance.md`.

User rule (as for the German pits): holes go on the pit's static meshes only. Never touch crew skeletal meshes,
animations, the gun child actors (`Howitzer`, `MG42Right`), `Turrets/`, `Crew/`, `Anims/`.

## Shared assets (already built by the lead; use, never edit or rebuild)

| Slot material | Hole instance |
|---|---|
| `/Game/SandBagPack/Materials/MI_Rus_SandBag_01` (every Soviet sandbag slot) | `/Game/SandBagPack/Materials/Holes/MI_RusPit_SandBag_01_WallHoles` (NoGrid, two-sided, on `MM_Rus_SandBag_01_WallHoles`) |
| `M_GerSandBagScaffold_Atlas` | `GerPits/Meshes/Holes/MI_GerPit_GerSandBag_WallHoles` |
| `M_Ger_AmmoBox` | `GerPits/Meshes/Holes/MI_GerPit_GerAmmoBox_WallHoles` |

Debris: Soviet pits `/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Meshes/Holes/SM_Rus_SandBag_Debris_0..7`;
TwinMG42 `GerPits/Meshes/Holes/SM_GerPit_Debris_0..7`. Slots with other materials (earth, RVT ground, wood, concrete,
ammo crates on Soviet meshes) get `None` in `DamagedMaterials` (no holes there) unless the lead says otherwise. Never
create or rebuild a `_WallHoles` master; if you think you need a new instance, stop and report.

## Component settings (build fresh structs; never copy a struct read from another BP's template, it is a LIVE reference)

- `HoleSettings`: MinCalibreMm 20, HoleRadiusPerCalibreMm 0.85, HoleRadiusPerSqrtDamage 5, GuaranteedChunkRadius 45,
  MaxHoleRadius 220, MaxHiddenShareWhileAlive 0.65.
- `GridSettings`: GridAxis NoGrid, CellSize 150, CellBreakRadiusShare 0.75, BakeReachShare 0.35, GrowRadiusShare 0.7,
  MinHoleRadius 12, HoleCentreOutsetShare 0, AreaPerDebrisPiece 4500 / MaxPiecesPerHit 12 (Soviet sandbags, 53K values)
  or 3600 / 5 (TwinMG42, German values), CollapsePieceSpacing 110, MaxCollapsePieces 120.
- `CollapseSettings`: DestroyOwnerDelaySeconds 4.5, DeathSpeedScale 1.3, CollapseSound
  `/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue`, CollapseAttenuation
  `/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse`, CollapseConcurrency
  `/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse`. **`bCollapseIntoChunksOnDeath` per pit, from its real death
  path:** if the pit dies via `DestroyAndSpawnActors` (actor gone, wreck spawned) set true (53K precedent); if it dies by
  the `UPitDefenseComponent::HandlePitDeath` sink (mesh stays while sinking) set false (German precedent). Read the death
  graph and `PitDefenseComponent.cpp` to decide which actually runs; report it.
- Python can't `set_editor_property` some EditDefaultsOnly struct fields on copies: use `export_text()`/`import_text()`.

## Collision ("modelling" part)

Holes centre on the hit point, so the collision must sit on the visible bags. Hulls that wrap far outside the bags
(the tent ropes case) or one hull over the whole ring (a dome over the gun) put holes in the air.
- Check each target mesh: trace rays from outside at the ring (mesh space, several directions and heights) against the
  collision and compare with the render surface (`Tools/WallChunks/Refinery/collision_gap.py` does this comparison).
- `CTF_USE_COMPLEX_AS_SIMPLE` meshes (trimesh) are already exact; leave them.
- Coarse hulls: rebuild with V-HACD (`StaticMeshEditorSubsystem.set_convex_decomposition_collisions`, 32-64 hulls as in
  the refinery/PostApo precedent) or, if hulls can't follow the ring, switch the mesh to complex-as-simple. Back up the
  old collision description first (`Saved/WallChunks/Pits2/<mesh>_collision_backup.txt`) and say which you chose. Keep
  the overall blocking footprint similar (units path around pits).
- Save only meshes you changed.

## Testing (headless, not PIE in the shared editor)

The shared editor (UECP 9855) is used by the user and other sessions: no PIE there, never close or build it.
Test in a throwaway **Development** editor, because the DebugGame DLL (12:11) is older than today's Development rebuild
(12:46):
```
D:/UnrealEngine/UE5.5_ReleaseBranch/UnrealEngine/Engine/Binaries/Win64/UnrealEditor.exe D:/UE5Projects/RTS_Survival/RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/UT_VehicleObstacle -ExecCmds="py <script>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
```
(`-DisablePlugins=Fab,BpGeneratorUltimate` keeps it off the UECP broker.) Templates: `Tools/WallChunks/pit_repair_test.py`
(PIE in a throwaway editor on UT_VehicleObstacle), `Tools/WallChunks/Refinery/refinery1_headless_test.py`.
- **One throwaway editor at a time across both agents:** take `mkdir D:/UE5Projects/RTS_Survival/Saved/WallChunks/headless.lock`
  (atomic; if it exists wait in 30 s steps), remove it when your editor has exited. Keep each run under ~6 minutes and
  make the script quit the editor itself (also on error/timeout).
- Save your assets in the shared editor before a run, and don't edit them while it runs (file locks).
- Required per pit: runtime values on a spawned instance (mesh component found, DamagedMaterials, collapse flag, aim
  points), debug hits (`debug_fire_test_hit`, 37-105 mm) on the shooter-facing rim showing holes, and **real tank fire**:
  pits spawned in PIE are player-owned and the player's tanks won't shoot them; spawn them into the editor world with
  `RTSComponent` `owning_player` 2 before starting PIE (never save the map), and log `LogWallHoleDamage Verbose`. Count
  hits and how many produced a visible hole (the hole centre within ~radius of the bag surface).
- Pictures to `D:/UE5Projects/RTS_Survival/Saved/WallChunks/Pits2/<pit>/`; look at them with Read.

## Shared-editor rules (strict)

Never close, restart or build the editor or C++; no PIE in it; never `save_dirty_packages`; save only your own assets
with `save_asset`/`save_loaded_asset`, right after each edit. Another session edits `Mission_Intro_SovietPatrols` and the
intro mission there; don't touch them. Touch only your assigned Blueprints and meshes. Commit nothing. C++ change
needed -> stop and report.

## Report back (short)

Per Blueprint: final mesh(es) and slots, DamagedMaterials, collapse flag + why, graph nodes removed/kept, aim points
before/after, collision changes (before/after hull counts or flag, trace-vs-surface numbers), test results (real-fire
hits vs visible holes) with picture paths, anything doubtful.
