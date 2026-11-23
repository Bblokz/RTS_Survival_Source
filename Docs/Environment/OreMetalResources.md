# Ore Mountains metal resources

Four red / yellow versions of the metal resource for `Ger_2ndOreMountains`. They behave like the stock metal
resources (same parent, same graph, same collapse); only mesh, collection, material and amount differ.
Built 2026-11-22. On `Ger_2ndOreMountains` since the same day (section "On the map").

| Blueprint (`/Game/RTS_Survival/Blueprints/Resources/ChildBps/Metal/OreMountains/`) | Look | Triangles | Pieces in collection | WorthMlt | Amount |
|---|---|---|---|---|---|
| `Resource_Metal_Ore_Blutbarren` | red ingot stacks on a rusted sled | 3,444 | 63 | 1.875 | 1500 - 1725 |
| `Resource_Metal_Ore_Glaskopf` | mirror-red hematite domes on a dark red ore block with red metal veins | 8,066 | 49 | 1.5 | 1200 - 1380 |
| `Resource_Metal_Ore_Katzengold` | pyrite cubes on a brown ochre rock with pyrite veins | 3,348 | 54 | 1.5 | 1200 - 1380 |
| `Resource_Metal_Ore_Gelbguss` | gold-yellow girders and pipes, dark ochre crust at the ends | 6,844 | 22 | 1.25 | 1000 - 1150 |

All: `ResourceType` Resource_Metal, 50 per harvest, 4 harvesters, health 700, nav box 9 x 9 m (unchanged from the
stock barrels), one material slot, no Nanite, two box collision shapes.

Assets under `/Game/RTS_Survival/Blueprints/Resources/Metal/OreMountains/`:
- `Materials/M_RTS_MetalResource_Ore`, `MI_OreMetal_<Name>`
- `Meshes/SM_OreMetal_<Name>`, `Meshes/Fractured/SM_OreMetal_<Name>_Fractured` (source of the collection; no Blueprint uses it,
  not checked whether the collection keeps a reference that pulls it into a cook)
- `Geometry/GC_OreMetal_<Name>`

## How the amount works (stock behaviour)
The Blueprints are duplicates of `Resource_Metal_Barrels`. Its construction script does not read variables: it sets
the mesh, the collection and `WorthMlt` with literals, then reads row `Metalbarrels` of `DT_ResourceSettings`
(base capacity 800, 50 per harvest):

    amount = ceil(800 * WorthMlt) * random scale (1.00 - 1.15), rounded to 5

So the amount is tuned with `WorthMlt` in the duplicate; the data table is untouched. To change an amount, change
`worth` in `ue_metal_build.py` and rebuild that resource.

## Material
`M_RTS_MetalResource_Ore` is a copy of the stock `M_RTS_MetalResource`. The stock base colour, metallic (0.85),
roughness (0.10 - 0.30 from the base texture alpha and the macro noise) and normal chains are untouched and feed
new nodes, so clean metal is exactly as shiny as the silver resources:
- `Tint` multiplies the stock base colour (which averages 0.52 grey), so `Tint` = wanted colour / 0.52.
- A matte crust (red oxide dust / ochre) is blended over it: base colour `CrustColorA`..`CrustColorB`, metallic 0,
  roughness `CrustRoughness` (0.85).
- The crust mask is vertex colour, authored in Blender: G = amount (0 clean, 1 full crust; in between the material
  breaks it up with the macro noise: `saturate((G * (1 + noise) - 0.5) * CrustSharpness) * CrustAmount`),
  B = tone (blend from colour A to colour B). R is the section index the collection builder reads.
- No new texture: the noise is the `T_RTSNoise_MacroVariation` the stock material already samples.
- Vertex colour was chosen over a baked mask texture: the meshes tile the stock metal texture with box-projected
  UVs (no unique unwrap), one material instance and one draw call per resource is enough (rock, sled and metal
  differ only in vertex colour), and it costs no texture memory. The price is a mask as coarse as the vertices.
- `bUsedWithGeometryCollections` is set on the copy (the collection builder, which sets and logs the flag when it is
  missing, found nothing to do). The collection stores vertex colours linear and converts them back for rendering
  (`bConvertVertexColorsToSRGB`, default on), so the crust mask should match the intact mesh; not seen rendered.

## Look pass (2026-11-22, after the first Unreal render)
The first render showed a flat salmon Glaskopf block, a coarse untextured Katzengold rock and flat yellow Gelbguss.
- Rocks (`blob_bm`): icosphere level 4, ridged noise and 16 - 18 chisel planes (flat broken faces with crisp edges),
  smooth shaded. `rock_color` darkens creases and undersides and leaves veins where the crust amount drops, so the
  metal shows through the rock.
- Colours: Glaskopf crust (0.22, 0.028, 0.012) / (0.035, 0.009, 0.006); Katzengold tint (1.83, 1.23, 0.38), crust
  (0.26, 0.12, 0.03) / (0.08, 0.035, 0.014); Gelbguss tint (1.60, 0.98, 0.17), crust (0.30, 0.14, 0.03) / (0.13, 0.06, 0.02).
  Gelbguss crust now starts at 70 - 74 % of the bar length (was 45 - 50 %) and the girder bevel is 13 mm.
- Material: `patch_crust_detail` adds a second, finer sample of the same macro noise (`CrustDetailTiling`, 11) that
  multiplies the crust colour by 0.5 - 1.4. It patches the existing graph in place; `build_material` never rebuilds an
  existing material.
- Rendered test `rev2b` (pictures in `Saved/OreMountains/metal/shots`, result `test/result_rev2b.json`): all four and
  the stock control pass. At the collapse the stock barrels barely move, Glaskopf domes roll out up to about 2.7 m,
  the others stay within about 1 m. Blutbarren still loses at least one piece through the ground about 1 s after the
  collapse (collection bottom reaches -490 m; nothing visibly missing; gone with the actor). Katzengold no longer does
  since its rock was rebuilt. Cause not found.
- Test script: no Python callable on `OnChaosBreakEvent` (it crashed the editor in the delegate thunk when an event
  fired). The fog of war post process is switched off for pictures. One camera serves all stations, so collapse
  pictures of stations that empty at the same moment can show the wrong station.

## On the map
`Source/Tools/OreMountainsMap/ore_metal_place.py` (throwaway full editor) swapped the five stock metal resources of
`Ger_2ndOreMountains` for the Ore ones at the same spot and yaw, and updated `Ore_dress.json` and `ore_dress_gen.py`
to match (`step_resources()` reproduces it):

| Label | Was | Where |
|---|---|---|
| `ORE_Res_Blutbarren_30` | Barrels | bench, by the mine railway |
| `ORE_Res_Gelbguss_31` | Rails | bench |
| `ORE_Res_Glaskopf_32` | BrokenWaterTank | west valley |
| `ORE_Res_Katzengold_33` | Barrels | west valley |
| `ORE_Res_Blutbarren_34` | Rails | west valley |

Nav check 0 problems; all four nav agents reach all 8 sides of every resource from the main ramp. The map holds more
metal than before: the five now total about 6,400 - 7,360 (each stock barrels was 800 - 920; the amounts of the
stock rails and water tank were not read). Pictures and the map backup from before the swap:
`Saved/OreMountains/metal/place`. No harvest was driven on this map.

## Rebuild
1. Models (Blender 4.0, headless):
   `blender.exe -b --factory-startup -P D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreMetalResources/Models/build_ore_metal.py -- [--only Blutbarren]`
   writes `fbx/SM_OreMetal_<Name>.fbx`, `..._Fractured.fbx` (every ingot, cube, dome, girder, pipe, sled part and rock
   chunk is its own closed island), `blend/<Name>.blend` and `build_report.json` (triangles, bounds, collision boxes).
   `render_ore_metal.py -- --name <Name> --out <dir>` renders a CPU preview with the PBR values of the material.
2. Unreal (headless commandlet, works with `-nullrhi`):
   `powershell -File Source/Tools/OreMineralFlora/trees/run_ue_cmd.ps1 -Script <abs>/Source/Tools/OreMetalResources/ue_metal_build.py -ScriptArgs "Blutbarren" -Tag ore_metal_build -NullRhi`
   (no arguments = all four; `MaterialsOnly` = only rewrite the material instances). Per resource it imports both
   meshes, sets the box collision, builds the collection and makes or retargets the Blueprint.
   - Collection: `RTS.FarmBuildings.BuildCollection` (one piece per island), then
     `RTS.Apartment.FlattenGeometryCollection` (all pieces directly under the root), then the stock metal thresholds
     1000 / 800 / 500 and no removal on sleep. The farm defaults (root threshold 1,000,000) would never break under the
     stock collapse strain of 5000.
   - Blueprint: `RTS.Blueprint.SetPinDefaults <json>` (new, `Source/RTS_Survival/Utils/BlueprintRepair/BlueprintPinDefaultCommands.cpp`)
     sets the six literals in the duplicate's construction script (two `SetStaticMesh`, two `Geo Collection`, two
     `WorthMlt`: the stock graph picks one of two barrel sets at random, both branches point at the new assets).
     The command needs the DebugGame editor module built (`build_debuggame_editor.ps1`).
3. Test: `powershell -File Source/Tools/OreMetalResources/run_metal_test.ps1 -Resources "Blutbarren,Gelbguss,Glaskopf,Katzengold" [-NullRhi]`
   runs `metal_harvest_test.py` in a throwaway editor on `Maps/UnitTests/RTS_ToCopySmall` with the stock
   `Resource_Metal_Barrels` as control. Without `-NullRhi` it also takes pictures (needs about 4 GB of free video memory).

## Test result (2026-11-22, `-nullrhi`, run `metal7`)
All four and the stock control: full amount harvested to zero (875 - 1525 in that run), `BP_OnResourceStorageEmpty`
hid the mesh and set the right collection, the collection root was released 0.3 - 0.4 s later
(`UGeometryCollectionComponent::IsRootBroken`, same timing as the stock barrels; how many pieces separate and where
they go cannot be read headless) and the actor destroyed itself
11.7 - 12.1 s after the last harvest. Result: `Saved/OreMountains/metal/test/result_final_run7.json`.
What the test stands in for: the test map has no drop-off building, so a harvester with a full cargo (75) is removed
and replaced; without a renderer the harvest montage notify never fires, so the test calls
`UHarvester::OnHarvestAnimationFinished` in its place.
Not verified: the look in Unreal (no rendered run was possible, the video memory was in use) including whether the
material compiles as intended, and how the loose pieces fly after the collapse. Preview renders from Blender with
the same PBR values are in `Saved/OreMountains/metal/shots/blender/`.

## Known limits
- The Blender FBX files are in metres with `FBX_SCALE_NONE`; Unreal imports them at the right size with
  `import_uniform_scale = 1` (100 made them 100 times too large).
- The collection component sits 50 cm above the mesh (stock construction script), so the pile hops up 50 cm when it
  collapses, exactly like the stock barrels.
- Collapse strain is 5000 with a linear falloff over 150 cm around the pile centre: pieces further out than about
  1.2 m may not be released by the strain itself (expected from the numbers, not observed).
- A collapse simulates 22 - 63 rigid bodies for 12 s per depleted resource. That is the stock behaviour; the cheap
  faked-damage route used elsewhere in the project was not taken here on purpose.
- Every piece is flat under the root (no cluster levels), so the 800 / 500 thresholds of the lower levels are unused.
- Glaskopf domes and Katzengold cubes overlap each other and their rock. The collision hulls are shrunk by 12 %
  (farm building setting), which may not be enough to stop overlapping pieces pushing each other apart at the
  collapse. If they fly too far, raise `CollisionObjectReductionPercentage` in the collection's size specific data.
- One LOD only. The largest mesh has 7,106 triangles (the stock ones have 468 - 11,612).
- The inherited Niagara component and the unused `SideGeometry` default still point at stock assets, as in the
  stock Blueprint.
