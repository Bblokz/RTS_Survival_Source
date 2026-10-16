# Gun pit and fieldwork meshes: no Nanite, cut sandbags

The game does not render Nanite. A static mesh with Nanite on draws only its coarse fallback mesh, which falls apart at a distance: bags go missing and the Flak pit's deck disappeared. Several pit and RedLine meshes had been imported with Nanite on, because the Interchange importer's default turns it on. They now all have Nanite off.

With Nanite off, a mesh draws its full source mesh, and the generated sandbag walls are heavy:
- every bag is a full copy of a marketplace sandbag, about 155 triangles (the German loose bags about 137);
- every ammo prop is about 100 triangles;
- a pit wall came to 30k–110k triangles.

So those meshes are cut first.

## The cut

`Marketplace/Scritps/PitOptimize/pit_mesh_optimize.py` (Blender 4.0) takes a generator's exported FBX and writes `<name>_Opt.fbx` next to it. The generator's own export stays the full-detail source.

How it cuts:
- **Per material.** Edges are collapsed only inside the faces of the named materials (Decimate Geometry on that material's faces).
  - The UVs are interpolated.
  - Each bag stays its own island, so no bag merges with a neighbour.
  - Timber, earth, concrete, steel and the German atlas are not touched.
- **Normals.** The original mesh's split normals are copied back onto the cut mesh (nearest face, interpolated), so the bags shade as before.
- **Sockets.** `SOCKET_*` empties are kept (RedLine `crouch_N`, `cargo_N`, `TurretMount`).

| Material | Ratio | Per bag / prop |
|---|---|---|
| `RL_Sandbag`, `MI_B4Pit_Sandbag` (Soviet pack bags) | 0.35 | about 54 triangles |
| `GerPit_Sandbag` (German loose bags) | 0.4 | about 55 triangles |
| `GerPit_Ammo` (ammo boxes and rounds) | 0.5 | about 50 triangles |

A Blender render at a few centimetres shows slightly more faceted bag ends; at game camera distance before and after look the same. The 0.25 ratio was visibly coarser in the close-up.

`Tools/GunPits/optimize_pit_meshes.py` then imports each `_Opt` FBX over its asset and restores what the import would change:
- the material per slot;
- the collision trace flag;
- the LOD group (LargeProp: LODs at 50, 25 and 12.5 %).

It also turns Nanite off through `StaticMeshEditorSubsystem.set_nanite_settings(..., apply_changes=True)`. Setting the property alone does not rebuild the mesh, and it keeps drawing the Nanite fallback until the asset is reloaded.

| Mesh | Source triangles | LOD0 now |
|---|---|---|
| `SM_GerPit_Howitzer` | 85,238 | 36,004 |
| `SM_GerPit_Flak` | 60,476 | 25,559 |
| `SM_GerPit_Pak` | 42,522 | 17,970 |
| `SM_B4Pit_Sandbags` | 74,826 | 26,188 |
| `SM_BS3Pit` | 109,618 | 38,366 |
| `SM_53KPit` | 75,514 | 26,428 |
| `SM_53KSectorPit` | 52,554 | 18,392 |
| `SM_DShKPit` | 29,690 | 10,444 |
| `SM_RL_MGNest` | 19,146 | 6,750 |
| `SM_RL_GunBay` | 11,030 | 3,918 |
| `SM_RL_Trench_Straight` / `_Bend` / `SM_RL_TrenchEnd` | 5,636 / 3,118 / 3,240 | 2,036 / 1,174 / 1,138 |
| `SM_RL_Dugout` / `SM_RL_TurretPillbox` | 2,250 / 2,542 | 992 / 974 |

The light meshes only have Nanite turned off; they draw in full:
- the German pit rounds and Flak clip;
- the B-4 pit props: turntable, emplacement, rack, rammer, cart handle, trolley, crane and cartridge;
- the ML-20 pit shell;
- the ML-20, BS-3 and 53-K wrecks;
- `SM_RL_CommTrench`.

## After rebuilding a pit mesh

The import scripts still bring in the full mesh, and Interchange turns Nanite on. After re-running a generator and its import:
1. Run `blender -b --factory-startup -P pit_mesh_optimize.py -- <export>.fbx <Material>=<ratio>[,...]` with the ratios above.
2. Run `python Tools/GunPits/optimize_pit_meshes.py heavy` (or `light` for the small meshes).

New pit meshes should be imported with Nanite off from the start, as `Tools/AmpulometPit` and `Tools/GerSmallPits` do, and added to the script's `HEAVY` or `LIGHT` list.

## Checked

`-At53KSectorPitValidate` and `-GerPitValidate` pass after the swap. Their captures match the earlier ones, except that the Flak pit's far view is now whole.

Not covered (Nanite still on, not pit meshes):
- vehicle hulls, collision hulls and wrecks: Kettenkrad, Soviet tanker, ZiL;
- `SM_steel_tower_*` and their chunks;
- `SM_Molotov`;
- `Box3`.
