# Oder-Warthe Werk targets (tunnel mouths and command post)

Built 2026-11-22 for `Ger_OderWarthe` (decisions in `OderWartheMissionFlow.md` section 11). Not placed in the map and
no mission scripting; this page lists the assets and where each one goes.

## Picked designs

The concept sheets are in `D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/OderWarthe/Targets/Concepts/`.

| Target | Pick | Look |
|---|---|---|
| Tunnel mouth, collapsed | G, whole tunnel | The cut-and-cover roof caves in along the tunnel: a trough of broken slabs through the berm. The lintel and coping break into three pieces and the wing-wall tops crumble. The blast doors are thrown down and crushed. The mouth and apron are packed with concrete and earth. Same pivot, frame and OW kit materials as `SM_OW_TunnelPortal`. |
| Command bunker entrance | A, gas-lock porch | Closes the Werkeingang alcove (between the door and its splinter wall) with a roof slab and a wall with an armoured side door. It also has earth on the roof, a sandbag blast wall and a camo net over the forecourt. It uses existing shaders: the Werke's faction concrete (`MI_BF_SootedFortress_UVTile`), the faction net (`MI_BF_CamoNet_Flutter`), the faction sandbag atlas, and OW kit steel and earth. |
| Jammer transmitter | F, umbrella | A clamp collar near the Werk mast top. Eight wires run down to short posts with insulators, in two of the three sectors between the guy anchors. A sod-covered tuning cellar with a downlead sits under a camo net. The mast (`SM_OW_WerkMast`) stays as it is. |
| Tunnel railway entrance | A, inner rail gate | An armoured two-leaf sliding gate across the tunnel, 3 m inside the portal mouth, with a frame, a wicket door and a buffer beam over the rails. |

## Assets (`/Game/RTS_Survival/Blueprints/Environment/OderWarthe/Targets/`)

| Blueprint | Intact mesh | Destroyed mesh | Health | Off per hit |
|---|---|---|---|---|
| `Blueprints/BP_OW_TunnelMouth` | `Kit/Meshes/SM_OW_TunnelPortal` (existing) | `Meshes/SM_OW_TunnelPortal_Collapsed` | 2000 | 150 |
| `Blueprints/BP_OW_Target_CommandPorch` | `Meshes/SM_OW_Target_CommandPorch` | `Meshes/SM_OW_Target_CommandPorch_Destroyed` | 1500 | 20 |
| `Blueprints/BP_OW_Target_Jammer` | `Meshes/SM_OW_Target_Jammer` | `Meshes/SM_OW_Target_Jammer_Destroyed` | 900 | 0 |
| `Blueprints/BP_OW_Target_RailGate` | `Meshes/SM_OW_Target_RailGate` | `Meshes/SM_OW_Target_RailGate_Destroyed` | 1500 | 20 |

Also in this folder:
- `Blueprints/DT_OW_WerkTargets`: Health rows, `FRTS_DestData`.
- `Materials/MI_OW_Target_PorchConcrete`: child of `MI_BF_SootedFortress_UVTile`, with `BF_UseCurvatureMap` off.

### How the Blueprints work

- **Base:** each Blueprint is a duplicate of `BP_Industrial_Dest_Container` (parent `BP_DestructableEnvActor` ->
  `ADestructableEnvActor`). The literals were retargeted with `RTS.Blueprint.SetPinDefaults`.
- **Construction script:** runs `SetupComponentCollisions` on `StaticMesh`, then reads Health from `DT_OW_WerkTargets`.
- **`BP_OnUnitDies`:** `CollapseMeshWithSwapping` swaps `StaticMesh` to the destroyed mesh and plays
  `NS_DestructionSmoke` and a collapse cue. The weapon trace channels are then ignored.
- **The wreck stays.** The actor is not destroyed, so it stays in the world as a dead target.
  - `USecureBaseMission::GetIsTargetDead` (`not IsUnitAlive()`) counts it as destroyed.
  - The archive squad (M10) can still walk to the porch.
- **Heavy weapons only:** this is the flat `DamageReduction` per hit in `ADestructableEnvActor::TakeDamage`. The tunnel
  mouth has 150, so these fired values apply:

  | Gun and shell | Damage to the tunnel per hit |
  |---|---|
  | 57 mm and below, MGs, infantry | none |
  | KwK 40 AP (221.4) | 71 |
  | KwK 40 HE (354) | 204 |
  | 88 mm KwK 36 AP (301.8) | 152 |
  | 88 mm L71 (377.3) | 227 |
  | 122 mm D-25T (601.8) | 452 |
  | Howitzers, rockets and bombs | most of their damage |

- **Navigation:** the map uses `DynamicModifiersOnly`.
  - The inherited `RTSNavCollisionEnvObstacle` keeps the parent's `NavArea_Obstacle` on the porch, jammer and gate.
  - On the tunnel mouth it is set to `NavArea_Default`, a no-op, so the open mouth stays walkable for the waves.
  - The collapsed mouth is blocked by its collision plug, but the baked navmesh does not change when it collapses.
  - The collapsed mouth keeps blocking shells. The other targets let shells pass once they are dead.
- **Hidden until the fix:** this is mission scripting.
  - The porch, jammer and gate need `SetActorHiddenInGame(true)` and `SetActorEnableCollision(false)` when spawned,
    and the reverse at the fix.
  - The tunnel mouths are never hidden.

## Where each target goes (Unreal, Werk frame: cm, X = the Werk's front)

The Werk meshes' `HQ_*` sockets are plan markers. They are all at z 0 with yaw 0, and the ground under them is 0 to
13 m higher. On import every socket also lost its yaw, including `Entrance`, which has 180 in the FBX for 8b and the
Peilturm. Spawn the targets at these transforms (add the Werk actor's transform):

| Target | Socket | 7c Kasemattennase (W1-W3) | 10 Peilturm (W4) | 8b Flankierend (W5) |
|---|---|---|---|---|
| `BP_OW_Target_CommandPorch` | `Entrance`, not `HQ_Command` | (1580, -120, 5), yaw 0 | (-2480, -80, 1039), yaw 180 | (-1680, -480, 605), yaw 180 |
| `BP_OW_Target_Jammer` | `HQ_Transmitter` XY, at the mast's z | (-3000, 1600, 31), yaw 0 | (-1600, 200, 987), yaw 0 | (-2000, 1000, 406), yaw 0 |
| `BP_OW_Target_RailGate` | `HQ_Rail` XY, at the portal's z | (-5000, 0, 0), portal yaw | (-5000, 0, 72), portal yaw | (-5000, 0, 43), portal yaw |
| `BP_OW_TunnelMouth` | replaces the placed `SM_OW_TunnelPortal` | same transform | same transform | same transform |

Notes on these spots:
- **Porch:** `HQ_Command` lies inside the bunker door on 7c, 6 m below the yard on 8b and inside the tower on the
  Peilturm, so the porch goes on `Entrance`.
  - It fits the alcove of all three designs: `werk_parts.eingang` is the same module on each.
  - On 8b and the Peilturm the stamp ground beside the door is up to 1.2 m off the sill. The porch footings reach 0.9 m
    down and the sandbags start 0.45 m down.
- **Jammer:** the mast stands at `HQ_Transmitter` on every Werk (mast yaw 0).
  - The third sector (150 deg in the mast frame) is left empty because the Peilturm's tower annex stands there.
  - A clearance check against the Werk meshes found no other overlap on any design.
- **Rail gate:** `HQ_Rail` is the tunnel portal's own spot, so the gate sits inside the command Werk's portal. Spawn it
  with the portal's transform.
  - If that Werk's tunnel mouth is collapsed before the fix, the gate is buried under the rubble. The mission must
    decide what happens then (the open question below).

## Open questions for mission scripting

- The command Werk's tunnel mouth and its rail gate share the portal. The test shows that a collapsed mouth buries the
  gate, which then cannot be hit. Decide whether collapsing that mouth early also counts as the rail target, or whether
  that Werk's mouth cannot be collapsed before the fix.
- The porch closes the Werkeingang alcove. Garrison units should not path into that alcove once it is revealed.

## Rebuild and checks

Tools live in `Source/Tools/OderWartheMap/targets/`:

| Script | What it does |
|---|---|
| `ow_targets_parts.py`, `ow_targets_pieces.py`, `ow_targets_concepts.py`, `ow_targets_sheets.py` | The concept sheets |
| `ow_targets_build.py` | Production meshes: FBX with UCX hulls, `.blend` files, renders (`-- all --preview`) |
| `ow_targets_fit.py`, `ow_targets_clearance.py` | Each target placed on the three real Werk meshes and stamps (renders and a column clearance test) |
| `ow_targets_import.py` | Headless import and slot binding |
| `ow_targets_blueprints.py` | Duplicates the template, retargets the pin defaults, sets Health, reduction and aim points |
| `ow_targets_test.py` + `run_targets_test.sh <run>` | Throwaway DebugGame PIE test on `RTS_ToCopySmall`; writes `Saved/OderWarthe/targets/test/<run>/` |

The PIE test (`ow_targets_test.py`) checks:
- Player tanks fire real shells.
- A Pz III's 50 mm does nothing to a tunnel mouth (a control target proves it was firing).
- A Pz IV takes 71.4 per shell off a mouth and a Tiger 151.8 per shell.
- Every target dies to tank fire, swaps to its destroyed mesh and stays in the world.

**Last run (2026-11-22, `r2`): all passed.**

| Target | Gun | Damage per hit | Result |
|---|---|---|---|
| Tunnel mouth | Pz III J 50 mm (125-134 per shell, plus MG) | 0 | The same tank took 36 hits off a control jammer, so it was firing |
| Tunnel mouth | Pz IV KwK 40 | 71.4 × 19 | Exactly as expected |
| Tunnel mouth | Tiger KwK 36 | 151.8 × 14 | Collapsed and swapped to `SM_OW_TunnelPortal_Collapsed`; the actor stayed |
| Porch | Pz IV, 8 shells (201.4 each) | | Swapped and stayed |
| Rail gate | Pz IV, 8 shells (201.4 each) | | Swapped and stayed |
| Jammer | Pz IV, 48 hits (shells plus MG, which hurts it because it has no reduction) | | Swapped and stayed |

**In place (`ow_targets_test_inplace.py`, run `r7`): all passed.**

| Lane | Result |
|---|---|
| Rail gate inside a live tunnel mouth (same transform) | A Pz IV took the gate down in 8 shells; the mouth took 0 hits. |
| Rail gate when the mouth collapses first | 0 hits on the gate, which is buried by the collapsed mouth. The mouth keeps blocking shells after the collapse, since `bNoLongerBlockWeaponsPostCollapse` is false for the tunnel only. |
| Porch on a real `SM_OW_Werk_Kasemattennase` Entrance | Destroyed by shells from the front and from the alcove's open end, 8 × 201.4 each way. |
| Jammer hidden (`SetActorHiddenInGame(true)`, `SetActorEnableCollision(false)`) | No hits in 25 s of attack orders. After the reveal it took hits and died. |

The jammer also has a collision hull and an aim point on the mast collar, so it can be hit from below when its
cellar sits out of sight on a hilltop bench. That case was not tested on the real landscape.

`ow_targets_look.py` (run `look1`) captured lit and base-colour pictures in Unreal of the porch on the Werk, the gate
in the mouth and the jammer, intact and destroyed. The base-colour captures show the faction concrete, earth and the
masked faction net. The lit ones are dark because of `RTS_ToCopySmall`'s own lighting.

The same "dead" check `USecureBaseMission` uses (`IsUnitAlive` false) held on every target after it died.
`FRTS_AOE` also damages through `TakeDamage`, so the flat 150 applies per blast. Whether a given ability's overlap
includes environment actors was not tested. In the first run every
target spawned as a shipping container, because the template's `SetRandomStaticMesh` list still held containers. The
builder now sets all five list entries to the intact mesh, and the test checks the intact meshes.

```
Blender5.0.exe -b --factory-startup -P ow_targets_build.py -- all --preview
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<targets>/ow_targets_import.py -unattended -nullrhi -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
UnrealEditor-Cmd.exe ... -script=<targets>/ow_targets_blueprints.py ...
sh run_targets_test.sh <run>
```
