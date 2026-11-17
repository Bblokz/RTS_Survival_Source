# Berlin rubble decals

Sixteen ground decals of loose rubble for Ger_Berlin, all on one 4 x 4 atlas and one material. Built 2026-11-17.

## Assets

`/Game/RTS_Survival/Blueprints/Environment/Berlin/RubbleDecals/`

| Asset | What |
|---|---|
| `T_BerlinRubbleDecal_BCO` | 2048 atlas, RGB albedo (sRGB), A opacity (BC3) |
| `T_BerlinRubbleDecal_NRS` | 2048 atlas, R/G normal (DirectX, +G = image down), B roughness, A piece mask (1 = rubble piece, 0 = dust bed), masks (BC3) |
| `M_BerlinRubbleDecal` | deferred decal, translucent (DBuffer colour, normal, roughness); 271 PS instructions, 3 samplers |
| `MI_BerlinRubbleDecal_<NN>_<Name>` | the 16 decals; each only sets the scalar `Cell` |

| Cell | Decal | Use |
|---|---|---|
| 00 | BrickSpillFan | fan of brick spilling from a wall: the atlas top edge is the wall side |
| 01 | BrickHeapSpread | round brick heap: collapsed plots, heaps |
| 02 | MasonryStrip | band of mixed masonry along a wall base (runs along the atlas left-right axis) |
| 03 | CornerPile | pile in an inside corner: the atlas top and left edges are the two walls |
| 04 | PlasterDust | pale plaster dust and stucco flakes |
| 05 | ConcreteRebar | broken concrete slabs and rusty rebar |
| 06 | RoofTiles | shattered Biberschwanz roof tiles and battens |
| 07 | SlateShards | dark slate roof shards |
| 08 | CharredTimber | burnt beams, planks and charcoal on ash |
| 09 | AshSoot | ash and soot patch with burnt paper |
| 10 | GlassShards | window glass (low roughness, it glints) |
| 11 | SettsTornUp | granite setts torn out of the carriageway on bedding sand |
| 12 | PavingSlabs | broken pavement slabs |
| 13 | SteelScrap | crumpled sheet, pipes, girder bits, rebar |
| 14 | SandstoneBlocks | broken ashlar and cornice pieces (Reichstag, Gate) |
| 15 | GritSpill | thin crushed brick and mortar grit, to blend heaps into the ground |

Every cell has the same texel density: one cell covers 400 cm at scale 1 (0.78 cm per texel), so a brick is the same
size in every decal. Keep placed scales between about 0.75 and 1.5.

## Placing one by hand

A plain DecalActor with the MI, rotation pitch -90, Decal Size (180, 200 x scale, 200 x scale).

- **Direction (measured, `rubble_orient_test.py`):** at yaw Y the atlas right edge points along world bearing Y and
  the atlas top edge (the wall side of the fan and corner cells) along bearing Y - 90. So for a fan against a wall that
  lies in direction B from the decal, use yaw B + 90.
- **Decal Color** (default white = as drawn): G = brightness (0 = 15 % darker), B < 0.5 mirrors the cell left-right
  (the normal map is flipped with it), A = opacity.
- MI parameters: `BedTint` / `PieceTint` tint the dust bed and the pieces separately, `BedOpacity` thins only the bed,
  `Desaturate`, `NormalStrength`, `RoughnessMul`, `Opacity`, `SlopeFadeStart/End` (keeps it off walls).

## Rebuilding the art

Scripts in `Source/Tools/BerlinMap/rubbledecals/`:

1. `rubble_table.py`: the cell recipes (shape, piece mix, sizes, colours, dust bed). Colours are taken from the ruin
   kit textures (brick, plaster, rubble) and the lit Berlin ground.
2. `rubble_blender.py` (Blender 4.0, headless, about 20 s for all 16): builds real meshes (bricks, half bricks, wall
   lumps, convex fragments from plane cuts, tiles, setts, slabs, timber, bevelled-curve rebar and pipes, crumpled
   sheets), scatters them by the cell's shape, and renders top-down orthographic AOV passes (albedo, roughness,
   height, normal, alpha) to `cellNN.npy`.

   ```bash
   D:/Blender/BlenderSoftware/4.0/blender.exe -b --factory-startup -P rubble_blender.py -- --out D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/r3
   ```
3. `rubble_atlas.py`: un-premultiplies the passes, adds the ragged dust bed and grit, bakes contact occlusion from the
   height, cuts a gutter at the cell borders, and packs the two atlases plus `preview_sheet.png`.

   ```bash
   python rubble_atlas.py --src D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/r3 --out D:/UE5Projects/RTS_Survival/Saved/Berlin/rubble_decals/r3/atlas
   ```
4. `rubble_ue_build.py` (in the editor): imports the two PNGs and builds the material (only when missing, or with
   `REBUILD_MATERIAL = True`) and the 16 MIs.

The atlas that is in the project is `Saved/Berlin/rubble_decals/final/` (round 3).
