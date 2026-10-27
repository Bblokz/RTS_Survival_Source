# Orchard trees (apple, pear, orange, cherry)

Art pipeline: `D:/Blender/BlenderProjects/BlenderTanks/OrchardTrees/`
(`build_atlas.py` -> `build_pbr.py` -> `blender -b -P build_meshes.py -- --render --mockup`; UE side:
`ue_import.py`, `ue_material.py`, `ue_blueprints.py`, staged in `Saved/OrchardTrees/` because UECP python can only
open files inside the project).
Content: `/Game/RTS_Survival/Blueprints/Environment/OrchardTrees/`.

## One atlas, one material

All four species, their litter, stumps and crush FX meshes share `T_Orchard_D/N/MAR` (2048) and `M_OrchardTrees`.

| Atlas region | Per species |
|---|---|
| `<sp>_leaves_0..7` (256 px) | leaf-cluster cards: a twig entering at the bottom with leaves radiating from it; tiles 4-7 carry fruit |
| `<sp>_bark` (256 x 512) | seamless bark (apple: flaky furrows, pear: blocky, orange: smooth grey-green + lichen, cherry: red with lenticel bands) |
| `<sp>_skin` (256) | fruit skin for the 3D fruit on LOD0 |
| `<sp>_litter` (512) | fallen leaves + bruised fruit under the canopy |
| `<sp>_fx_leaf` (128) | one leaf for the crush FX |
| `end_grain`, `fresh_wood`, `twig_bark` | stump top, splinters, thin branches |

MAR = R metallic, G AO, B roughness, A foliage transmission (subsurface colour of the two-sided foliage model).

## Meshes and cost

| Mesh | LOD0 / LOD1 / LOD2 tris | Notes |
|---|---|---|
| `SM_<Sp>Tree_A/B` | 0.9-2.2k / 0.5-0.6k / 0.2-0.25k | trunk with root flare, scaffold branches, twigs; LOD0 adds 3D fruit (not cherry) |
| `SM_<Sp>Tree_Litter` | 12 | ground disc, no collision |
| `SM_<Sp>Tree_Stump` | 66 | left behind when a tank pushes the tree over |

LOD screen sizes 1.0 / 0.25 / 0.1: LOD0 only at close zoom, LOD1 at mid zoom, LOD2 at max zoom (48 m arm).
Lower LODs use fewer, larger cards pulled inwards so the silhouette keeps its size, and more fruit tiles on their
outer clusters (no 3D fruit there) so the species still reads from the RTS camera.

Leaf clusters are one card tangent to the canopy ellipsoid (reads from the top-down camera) plus one crossing card.
Foliage normals point out of the canopy ellipsoid, so the crown lights as one soft volume.

Normals: feed the atlas normal map straight into Normal. The farm material's `Z * TwoSidedSign` back-face flip only
works when every card faces the camera from the front (horizontal crop cards). Tree clusters face every way, and the
flip darkened each card seen from behind (the sun side of the crown went dark).

## Blueprints and crushing

`BP_OrchardTree_<Apple|Pear|Orange|Cherry>_<A|B>` are children of `BP_WpoTree` (`AWpoTree` ->
`ADestructableEnvActor`). Each is a copy of `BP_WpoLeafTree_Large`: HP from the `Dest_TallTree` row of
`DT_RTS_DestructableEnvActorData`, random yaw and scale (pins narrowed to 0.9-1.08, XY x 0.95-1.1, because the trees
are modelled at real size). The base BP wires the crush on BeginPlay: `SetupCrushDestructionOverlap(Tree,
TankDestructionType)` + `ConfigureVerticalCollapse`; on death the tree falls in a random direction and despawns
after 5 s. `TankDestructionType = MediumOrHeavyTank` on every orchard BP.

| Component | Setup |
|---|---|
| `Tree` (inherited) | the species mesh; its only collision is a trunk capsule (radius trunk x 1.4, up to the fork + 0.6 m), so tanks touch the trunk and never the canopy; the inherited nav modifier is sized from it |
| `Litter` (own) | `SM_<Sp>Tree_Litter`, NoCollision, no shadow, no nav (B trees: 0.88 scale) |

PIE check on Ger_Trains (2026-10-27): a Panzer II Luchs pushed into an apple or cherry trunk leaves it standing; a
Panzer IV F1 topples the same tree (roll ~90 deg, life span 5 s), so light tanks cannot crush orchard trees.

Collision note: neither the Interchange nor the legacy FBX importer keeps the `UCX_` trunk hull from the FBX, so
`ue_import.py` writes the capsule into the body setup (`KAggregateGeom.import_text`) after every import.

Vertex colour (linear): R = wind sway weight (0 on the trunk, up to 1 on the outer leaves), G = canopy occlusion
(inner clusters darker), B = per-cluster random phase. `M_OrchardTrees` sways with R^2 * `WindStrength` (WPO, turned
off with distance by `UWpoDistanceManager`), darkens with G and varies brightness per cluster with B; every tree also
gets a tint from a hash of its position (`TreeTintLow/High`, foliage only).
