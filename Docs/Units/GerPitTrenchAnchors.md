# German pit trench anchors

Where a field trench ([../UI/FieldDefenses.md](../UI/FieldDefenses.md), Barriers tab) can join a German weapon pit. Each anchor is a socket on the pit mesh plus a short neck mesh that closes the gap between the straight trench end and the pit wall. `UFieldTrenchAnchorComponent` on each pit Blueprint makes the sockets work: the trench placement snaps to them and the neck and plug swap when a trench joins (see "Trench anchors" in [../UI/FieldDefenses.md](../UI/FieldDefenses.md)).

## Rules

- **Never at the front.** A pit's front is +X (the gun's rest direction). Anchors sit only at the rear and the rear quarters, so the side facing the enemy stays closed.
- **Only big pits get anchors, and more of them as the pit grows:**

| Pit | Footprint | Anchors | Where |
|---|---|---|---|
| Panzerschreck foxhole, Püppchen pit, Comm MG42 nests | 400 to 500 | 0 | |
| Pak 40 pit | 1000 | 1 | rear (crew entrance) |
| Flak 37 pit | 1100 | 1 | rear (crew entrance) |
| Mortar pit | 1000 | 1 | right end of the rear dogleg corridor (behind the blast wall) |
| Twin MG42 pit | 900 (12 m wide) | 2 | outer flank of each nest |
| Flak 88 emplacement | 1200 | 2 | rear entrance, rear half of the right wall |
| leFH 18 / sFH 18 pits | 1400 | 3 | rear, and the two rear diagonals (±135° from the front) |

The mortar pit's rear entrance opens into a corridor behind a blast wall, open at both ends; a trench meets the entrance only through that corridor, so the anchor sits at the corridor's end and faces right.

## The socket

`TrenchPort_<Name>` on the pit's static mesh (`TrenchPort_Rear`, `TrenchPort_RearLeft`, ...).

- **Location:** on the trench's centreline, at ground level (z 0). For a round pit it sits where the deck tongue through the entrance ends: deck radius (inner radius + 10) plus 118 cm.
- **Rotation:** pure yaw, its +X pointing away from the pit.
- **Joining:** this is the field trench kit's port contract (`Tools/FieldTrenches/field_trench_kit.json`). A kit piece joins with one of its ports on the socket, at the opposite yaw. The planner can treat the socket like the open port of an already placed piece and start `FieldTrenchLayout::PlanAlongPath` from it.

| Pit | Socket | Location (cm) | Yaw | In Unreal |
|---|---|---|---|---|
| `SM_GerPit_Pak` | `TrenchPort_Rear` | (−528, 0, 0) | 180 | yes |
| `SM_GerPit_Flak` | `TrenchPort_Rear` | (−558, 0, 0) | 180 | yes |
| `SM_GerPit_Howitzer` (leFH and sFH) | `TrenchPort_Rear` | (−648, 0, 0) | 180 | yes |
| | `TrenchPort_RearLeft` | (−458.2, −458.2, 0) | −135 | yes |
| | `TrenchPort_RearRight` | (−458.2, 458.2, 0) | 135 | yes |
| `SM_GerMortarPit` | `TrenchPort_RearRight` | (−388.5, 405, 0) | 90 | yes |
| `SM_GerTwinMG42Pit` | `TrenchPort_Left` | (40, −655, 0) | −90 | yes |
| | `TrenchPort_Right` | (40, 655, 0) | 90 | yes |
| `SM_GerFlak88Pit` | `TrenchPort_Rear` | (−536.5, 110, 0) | 180 | yes |
| | `TrenchPort_Right` | (−200, 536.5, 0) | 90 | yes |

What changed on each pit's own mesh:
- **Pak 40, mortar:** nothing; only the socket.
- **Flak 37:** the clip crate at 160° moved to 136°, clear of the rear neck.
- **leFH / sFH:** gaps on the rear diagonals with entrance cheeks. The ammo stacks moved off the diagonals: the long boxes now sit between the trenches, the square ones at the sides. The net drapes now span 25–70°, 100–114°, 246–260° and 295–335°. Every other bag is unchanged.
- **Twin MG42:** an opening through each nest's outer flank, cut by the generator's own opening rule (no bag within 62 cm of the lane). Before the cut, pit plus plugs add up to exactly the old triangle count; in the game the pit (`TwinMG42Pit/Meshes/SM_GerPit_TwinMG42_Sandbags`) is 19,070 triangles.
- **Flak 88:** a 100 cm gap at x −250 to −150 in the right wall, its cheeks stepping back above course 6 like the entrance. The right wall's bags are laid again; every other wall keeps its bags.

Unreal space is Blender's with Y mirrored (Unreal yaw = −Blender yaw), the same as for the trench kit. Rear anchors lie on the X axis, so their values are the same in both.

## Neck and plug meshes

In `GerPits/Meshes` (the twin MG42's in `GerPits/TwinMG42Pit/Meshes`). Each has its origin on the socket, with +X pointing out of the pit, so a component attached to the socket with no offset is in place.

- **`<Pit>_TrenchNeck_Rear` / `_Side`:** the trench stub from the pit's outer wall to the socket, about 1 m long. It is built from the kit's own pallet revetment and bag rows, and each bag row starts where it meets the round wall, so the 400 cm-wide trench end (165 cm between the pallets since the kit was widened on 2026-11-09) closes against the curved wall. The game shows it only while a trench is attached. Rear necks reuse the plank tongue the crew entrance already has as their floor; side necks bring their own tongue.
  - Materials: `M_GerCamoConcretePalet_Atlas` and `M_GerSandBagScaffold_Atlas`.
  - Complex collision, LOD group LargeProp, no Nanite.
- **`<Pit>_TrenchPlug`:** only for anchors cut into a wall that was closed. It holds the wall bags taken out of the gap and is shown while no trench is attached, so the pit stays closed. Rear anchors use the existing entrance and have no plug.

Side necks also bring what a closed wall lacks: the round pits a deck tongue, the twin MG42 nests a mud floor through the nest wall, the Flak 88 a landing at deck height and three steps down to the trench.

| Mesh | Triangles |
|---|---|
| `SM_GerPit_Pak_TrenchNeck_Rear` | 854 |
| `SM_GerPit_Flak_TrenchNeck_Rear` | 702 |
| `SM_GerPit_Howitzer_TrenchNeck_Rear` / `_Side` | 682 / 798 |
| `SM_GerPit_Howitzer_TrenchPlug_RearLeft` / `_RearRight` | 1,352 / 1,328 (cut to 0.4 like the pit's bags) |
| `SM_GerMortarPit_TrenchNeck_RearRight` | 560 |
| `SM_GerTwinMG42Pit_TrenchNeck_Left` / `_Right` | 1,028 / 1,024 |
| `SM_GerTwinMG42Pit_TrenchPlug_Left` / `_Right` | 862 / 964 (cut to 0.4) |
| `SM_GerFlak88Pit_TrenchNeck_Rear` / `_Right` | 498 / 592 |
| `SM_GerFlak88Pit_TrenchPlug_Right` | 1,797 |

## Rebuilding

Blender 4.0, in `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/GerDefenses/Scripts`:

```bash
blender.exe -b --factory-startup -P gerpit_trench_anchors.py -- Pak preview
```

The pit keys are `Pak`, `Flak`, `Howitzer`, `Mortar`, `TwinMG42` and `Flak88`. The round pits are rebuilt with `gerpit_build.py`. The other three use their own generators: the mortar pit's FBX is only loaded, while the twin MG42 and Flak 88 generators are run with their bag layer intercepted.

`Tools/GerPits/ue_gerpit_trench_anchors.py` (in the editor, with `PITS_TO_RUN` set) does the Unreal side:
- it reimports changed pits from the `_Opt` cut;
- it sets every socket to its pure-yaw transform;
- it imports the necks and plugs.
- with `NECKS_ONLY = True` it imports only the necks and leaves the pit, its sockets and the plugs alone (used when only the kit's width changed).

The script:
- rebuilds the pit with `gerpit_build.py`'s own builders and seeds, so the pit keeps its bags. A new gap takes its bags out after they have used up the generator's random numbers.
- writes the pit with its `SOCKET_TrenchPort_*` empties, the necks, any plug and a `.blend` to `Exports/TrenchAnchors/<Pit>`.
- with `preview`, renders the pit with a kit trench laid out of every socket into `Exports/TrenchAnchors/<Pit>/Preview` (with plugs: the closed state too).

In Unreal:
- The socket was added to the existing pit mesh with `StaticMeshSocket` + `add_socket`, so the pit was not reimported. The pit geometry has not changed (same bags, same triangle count); only the socket is new.
- The neck was imported like the trench kit (`Tools/FieldTrenches/ue_field_trench_import.py` settings), with the pit's material slots copied by name.
- A pit whose wall gets a new gap does change geometry. It goes through the normal pit pipeline: `pit_mesh_optimize.py`, then `Tools/GunPits/optimize_pit_meshes.py` (see [GunPitMeshes.md](GunPitMeshes.md)). After that, add the socket again.

## In the game

`Tools/GerPits/ue_gerpit_trench_anchor_components.py` adds a `TrenchAnchors` component (`UFieldTrenchAnchorComponent`) to the seven pit Blueprints and lists each socket's neck and plug. The leFH and sFH pits use the `_Side` neck on both diagonals. The component needs a build that has it: run the script headlessly on a DebugGame build (see the script). Opening and saving a pit Blueprint in an editor built before the component existed drops it.

Each socket is found on the pit mesh itself, so a pit needs no list entry for an anchor without neck or plug. The plug and neck are attached to the socket and copy the pit mesh's collision, without affecting the navigation mesh.

## Breakthrough bunkers

Not in the build menu yet; placed in a level, their anchors work from the start (an expansion nobody took over a tick after
its BeginPlay counts as placed in the level). Sockets only, no neck or plug meshes. The import scripts set them on the intact
and the damaged mesh (`TRENCH_PORTS` in `Tools/BreakthroughBuildings/ue_flak88_bunker_import.py` and
`Tools/GerPits/ue_flakvierling_bunker_import.py`), and `ue_gerpit_trench_anchor_components.py` gives both Blueprints the component.

| Bunker | Socket | Location (cm) | Yaw | Where |
|---|---|---|---|---|
| `SM_BF_Flak88Bunker` | `TrenchPort_Doors` | (460, 135, 0) | 0 | in the 5 m door bay, up to the two doors |
| | `TrenchPort_Stairs` | (535, -650, 0) | 0 | at the stair foot, just past the cheek wall's end |
| `SM_BF_FlakvierlingBunker` | `TrenchPort_Passage` | (260, -80, 0) | 0 | in front of the crew passage between the Ringstand and the wedge, clear of the ring |
| | `TrenchPort_Rear` | (-390, -150, 0) | 180 | against the closed back wall: no opening there yet |

Both ways into the Flak 88 bunker at grade face the front (the back and left are full-height walls), so its anchors break
the pits' "never at the front" rule. Checked with kit trenches laid out of each socket in Blender and in the game
(`-FieldDefenseValidateTrenchAnchor`).

## Still to do

- Plugs of pits whose own mesh is cut (round pits, twin MG42) go through `pit_mesh_optimize.py` (`GerPit_Sandbag=0.4`) before import; the import script's `CUT_EXTRAS` lists them.
- A trench joined to two pits at once.
- The abandoned capture pits (`BP_CaptureGerPit_*`) have no anchors; the working pit that replaces one does.
