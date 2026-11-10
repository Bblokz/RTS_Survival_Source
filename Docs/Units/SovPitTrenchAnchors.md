# Soviet field trenches and pit trench anchors

The Soviet side has its own field trench kit and its own trench anchors on the Soviet weapon pits. The system is the German one ([GerPitTrenchAnchors.md](GerPitTrenchAnchors.md), "Trench anchors" in [../UI/FieldDefenses.md](../UI/FieldDefenses.md)). Only the look and the pits differ.

## The Soviet kit

`EFieldTrenchKit` (`Buildings/FieldTrench/FieldTrenchLayout.h`) picks the meshes a trench lays: `German` or `Soviet`.
- It is a property (`Kit`) on `ARTSFieldTrench`, so both the runtime trench and the spline trench have it.
- `FieldTrenchLayout::GetPieceMeshPath(Piece, Kit)` returns `Trenches/SovFieldTrench/Meshes/SM_SovFieldTrench_*` for the Soviet kit.
- The planner, ports, cover sockets, joins and anchors are shared. The Soviet kit has exactly the German kit's pieces, ports and `cargo_N` sockets (checked in the manifests), so the planner serves both.
- Changing `Kit` on a placed spline trench rebuilds it in the other look.

**Look.** The German trench uses pallets stood on their side, duckboard slats and the German loose bags. The Soviet trench uses:
- horizontal logs of silvered, debarked pine, five per wall, held by round stakes;
- a corduroy floor of darker, mud-stained poles laid across the trench;
- the Soviet SandBagPack bags (`MI_Rus_SandBag_01`, the bags of every Soviet pit). They are flatter, so a wall has four courses, which keeps the rim at the German height.

The logs are lighter and cooler than both the dark olive-brown burlap and the warm German pallets, so the revetment reads against the bags and the two factions' trenches are told apart from the RTS camera.

**Map design.** Use `Trenches/SovFieldTrench/BP_SovFieldTrenchSpline`.
- It is a child of `BP_FieldTrenchSpline` with `Kit = Soviet`. Draw the spline and the trench is rebuilt as you edit it, exactly like the German one.
- Its ends snap to pit `TrenchPort_` sockets and to other spline trenches.
- It was made by `Tools/FieldTrench/create_sov_field_trench_spline_bp.py`, which also self-checks on the open map: 31 Soviet pieces on a 55 m bent spline, stable across reruns, 0 cm ground gap, kit switch on an instance.

**Assets** in `Trenches/SovFieldTrench`:
- `Meshes/SM_SovFieldTrench_*`, 13 pieces. Complex collision, LOD group LargeProp, no Nanite. A 250 cm straight is 2,234 tris (German 1,633).
- `Materials/M_SovFieldTrench_Wood`
- `Textures/T_SovFieldTrench_Wood_D` / `_N`, a 2048 atlas.

The mesh folder is in `DirectoriesToAlwaysCook`, because the pieces are loaded by path.

**Rebuilding** (Blender 4.0, in `CustomMapItems/SovietsStuff/FieldTrenches`):
- `generate_sov_trench_wood.py` (plain Python with numpy and Pillow): the wood atlas.
- `generate_sov_field_trenches.py`: the kit. It imports the German generator and swaps only the dressing (log bays for pallets, poles for slats, Soviet bags with their seams welded and faces wound outward before the cut).
- `validate_sov_field_trenches.py`: the German validator on the Soviet kit. 521 checks pass.
- `preview_sov_field_trenches.py`: renders to `Previews/`.

Then copy `Exports/sov_field_trench_kit.json` to `Tools/FieldTrenches/` and run `Tools/FieldTrenches/ue_sov_field_trench_import.py` in the editor.

## Soviet pit anchors

Every Soviet pit has one anchor, at its existing crew entrance. No pit mesh changed: there are no new wall gaps and no plugs; only a socket was added to each pit mesh.

The DShK and Ampulomet pits are small (about 7 m). Under the German rule they would get no anchor, but they have one so that every Soviet pit can be joined. The mortar pit's only entrance is in its side wall. The mortar fires over its walls, so its anchor is there instead of at the rear.

| Pit | Socket | Location (cm, Unreal) | Yaw | Neck | Tris |
|---|---|---|---|---|---|
| `SM_53KPit` | `TrenchPort_Rear` | (−625, 0, 0) | 180 | `SM_53KPit_TrenchNeck_Rear` | 1,454 |
| `SM_53KSectorPit` | `TrenchPort_Rear` | (−500, 0, 0) | 180 | `SM_53KSectorPit_TrenchNeck_Rear` | 720 |
| `SM_BS3Pit` | `TrenchPort_Rear` | (−662.5, 0, 0) | 180 | `SM_BS3Pit_TrenchNeck_Rear` | 1,219 |
| `SM_DShKPit` | `TrenchPort_Rear` | (−285, 0, 0) | 180 | `SM_DShKPit_TrenchNeck_Rear` | 1,058 |
| `SM_AmpulometPit` | `TrenchPort_Rear` | (−335, 0, 0) | 180 | `SM_AmpulometPit_TrenchNeck_Rear` | 1,203 |
| `SM_Rus120MortarPit` | `TrenchPort_Left` | (−5, −540, 0) | −90 | `SM_Rus120MortarPit_TrenchNeck_Left` | 664 |

**Necks.** They are built from the Soviet kit's own logs and bag rows. Each row starts where a ray first meets the pit's bags or berm and reaches 8 cm into them.
- Each neck brings a mud and pole floor through the entrance, because these pits have no deck tongue. That is why they are longer than the German necks.
- The hidden-triangle cull counted the pit as an occluder, so a neck seen alone misses a few buried bag ends.
- The rows are fitted to the current pit meshes (the `_Opt` cuts, except the Ampulomet's full mesh). If a pit is regenerated, rerun its neck script.

**Rebuilding.** Blender: `CustomMapItems/SovietsStuff/TrenchAnchors/<Pit>/<pit>_trench_anchors.py -- preview` writes the neck FBX, `anchors.json` and previews. The mortar pit has no Blender source: it was assembled in Unreal, and `Source/SM_Rus120MortarPit.fbx` is an export of the Unreal mesh.

Unreal: copy each `anchors.json` to `Tools/SovPits/anchors/<Pit>.json`, then run `Tools/SovPits/ue_sovpit_trench_anchors.py` in an editor that has `UFieldTrenchAnchorComponent`. It does three things:
- adds the socket to the pit mesh;
- imports the neck next to the pit mesh;
- gives the pit's `Rus_BXP_*` Blueprint a `TrenchAnchors` component that lists the neck.

## Runtime Soviet trenches: the enemy AI digs them

There is no Soviet player (`ERTSFaction` has only German factions; the Soviets are always the AI), so the Soviet runtime trench is the enemy AI's. Its bases dig Soviet trenches out of their pits' anchors and along an arc in front of the base. See [../EnemyAI/EnemyBaseTrenches.md](../EnemyAI/EnemyBaseTrenches.md).

- The AI spawns the usual C++ `ARTSFieldTrench` and calls `SetKit(EFieldTrenchKit::Soviet)` on it before any piece exists. `SetKit` also swaps the meshes of pieces that already exist.
- The pits keep their Blueprint owner, player 2. Anchors only join their own player's trenches, so the player's trench never snaps to a Soviet pit, and the AI's never to a German one.
- **The player's HQ trench is always German.** `StartFieldTrenchPlacement` has no kit argument, and `UFieldTrenchPlacementComponent` sets `EFieldTrenchKit::German` on its trench right after spawning it, whatever `TrenchClass` carries.

## Still to do

- Anchors do not check the kit, so a German trench can join a Soviet pit; its neck is then Soviet.
- The bigger pits (53-K, BS-3) could get rear-diagonal anchors like the German howitzer pit. That needs new wall gaps, plugs and a pit reimport.
