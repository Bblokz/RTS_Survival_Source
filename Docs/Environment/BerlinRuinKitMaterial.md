# Berlin ruin kit material (`M_BerlinRuin`, 2026-11-12)

One cheap shared master for every Berlin ruin kit piece (`KIT_CONTRACT.md` in
`D:\Blender\BlenderProjects\BlenderTanks\CustomMapItems\Environment\BerlinRuinKit\`), and its procedural shell-hole
variant. It replaces the DBK-based `MI_Berlin_Ruin_*` instances on the kit (those are left untouched).

Assets are in `/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Materials/`:

| Asset | What |
|---|---|
| `M_BerlinRuin` | Opaque, default lit, one-sided master. |
| `MI_BerlinRuin_Wall` | Slot 0 (`M_BerlinRuin_Wall`) on undamaged actors. `Ruin_IsRubble` off. |
| `MI_BerlinRuin_Rubble` | Slot 1 (`M_BerlinRuin_Rubble`). `Ruin_IsRubble` on. |
| `M_BerlinRuin_WallHoles` | Masked hole copy made by `Tools/WallChunks/make_wall_holes_material.masked_master_for(..., inner_tint=True)`. |
| `MI_BerlinRuin_Wall_Holes` | `make_holes_instance(MI_BerlinRuin_Wall, NoGrid, two_sided=True, inner_tint=True)`. The `UWallHoleDamageComponent` swaps slot 0 to this on the first hit (`DamagedMaterials`). |
| `T_BerlinRuin_Plaster_D`, `_Brick_D`, `_N`, `_Rubble_D` | 2048 px, BC7, LOD group World, 1 tile = 4 m. |

## Inputs

- **UV0**: the contract's world-scale box projection, 1 UV unit = 4 m. The tile scalar is applied in the vertex shader (a
  Vertex Interpolator; Customized UVs are not reachable from Python).
- **Vertex colour (linear)**:
  - R: soot (walls) / charred timber (rubble).
  - G: plaster loss, 0 = intact plaster, 1 = bare brick. Wall only.
  - B: dust and ground grime.
- **Custom primitive data 0-3** (the hole system owns 8-34). Unset CPD reads 0 in the shader, and the hole component
  zero-fills 0-7 when it grows the array on the first hit. So 0 means "unset" here:
  - 0, 1, 2: tint RGB multiplier. All three 0 uses `RuinTintFallback` (white). Write e.g. (1.05, 1.0, 0.9) for a warm
    building.
  - 3: soot amount. 0 uses `RuinSootAmountFallback` (1). To switch soot off, write a **negative** value (e.g. -1).
  - Read by one CPD vector parameter, `RuinCPD_TintSoot` (index 0, 4 floats).

## Texture packing (3 samples on the wall, 1 on rubble)

All wall textures share one UV (`UV0 x RuinWallTile`, 4 m), so there is one UV and three fetches.

| Texture | RGB | A |
|---|---|---|
| `T_BerlinRuin_Plaster_D` | Weathered grey-beige lime plaster (sRGB). | Chip height, uniform 0..1. Plaster shows where A > vertex G, so the loss edge follows flakes and cracks and looks chipped. This is the noise for the breakup; no extra fetch. |
| `T_BerlinRuin_Brick_D` | Dull red-brown brick, lime mortar, Blockverband. 52 courses (7.7 cm including the joint) x 16 stretchers per 4 m (sRGB). | Cavity: 1 = brick face, 0 = deep joint. Drives roughness, plus soot and dust breakup. |
| `T_BerlinRuin_N` | R, G: brick normal XY. B, A: plaster normal XY. Linear BC7, DirectX green, 0.5 = flat. | |
| `T_BerlinRuin_Rubble_D` | Brick, plaster and mortar rubble with charred timber, cavity shading baked in (sRGB). | Height, uniform 0..1. |

- **Brick tile.** The brief suggested about 2 m. Instead the brick is unique over the full 4 m tile at the same texel
  density (1.95 mm per px): the same UV as the plaster, so one UV and less visible repetition. Brick and plaster
  normals share one texture and are blended by the plaster mask before Z (the shader normalises the normal). That is
  why the normal costs no second fetch.
- **Rubble** has no normal map. Cavity shading is baked into its albedo.
- **Brick shimmer:** a course is 39.4 px, about 2.5 px at mip 4. The mortar is kept dirty and close to the brick mean (luminance 0.40 against 0.33). That halved the 52-cycle course energy, and the 12-cycle alias band that showed at mip 5 is gone (measured on the row-mean luminance spectrum).
- **Hole variant:** adds the library's `LowResBlurredNoise` sample, so the wall has 4 samples once it has holes.

## Shader (per pixel)

```
t       = (G + RuinPlasterLoss) * (1 + 2w) - w            w = RuinPlasterEdgeWidth
d       = (Plaster.A - t) / w ;  plaster = saturate(d)
plasterCol = Plaster.rgb * RuinPlasterColor * (1 + saturate(2 - d) * plaster * RuinPlasterEdgeLight)   // fresh lime rim
brickCol   = Brick.rgb * RuinBrickColor * (1 - saturate(1 + d/2) * (1 - plaster) * RuinPlasterEdgeShadow) // plaster thickness
base    = lerp(brickCol, plasterCol, plaster) * tint
rough   = lerp(lerp(RuinBrickRoughJoint, RuinBrickRoughFace, Brick.A), RuinPlasterRoughness, plaster)
normal  = (lerp(N.rg, N.ba, plaster) * 2 - 1) * RuinNormalStrength, z = 1
noise   = lerp(Brick.A, Plaster.A, plaster)                          (rubble: Rubble.A)
soot    = saturate(R * sootAmount * (RuinSootStrength + RuinSootBreakup * (noise - 0.5)))
base    = lerp(base, base * RuinSootColor, soot)
dust    = saturate(B * (RuinDustStrength + RuinDustBreakup * (0.5 - noise))) * RuinDustOpacity
base    = lerp(base, RuinDustColor, dust);  rough = lerp(rough, RuinMatteRoughness, max(soot, dust))
```

The rubble branch (`Ruin_IsRubble`) uses `Rubble.rgb * RuinRubbleColor`, `RuinRubbleRoughness`, a flat normal, and the
same tint, soot and dust.

## Parameters (group `BerlinRuin`)

| Parameter | Default | Notes |
|---|---|---|
| `Ruin_IsRubble` (static switch) | false | true on `MI_BerlinRuin_Rubble` |
| `RuinWallTile` | 1.0 | UV0 multiplier for the wall textures (1.0 = 4 m) |
| `RuinRubbleTile` | 1.333 | 3 m rubble tile |
| `RuinPlasterLoss` | 0 | added to vertex G (per-instance "more / less plaster") |
| `RuinPlasterEdgeWidth` | 0.04 | transition width in chip-height units; smaller is sharper |
| `RuinPlasterEdgeLight` | 0.22 | bright broken plaster rim |
| `RuinPlasterEdgeShadow` | 0.30 | darker brick just below the plaster edge |
| `RuinPlasterRoughness` | 0.90 | |
| `RuinBrickRoughFace` / `RuinBrickRoughJoint` | 0.80 / 0.97 | by brick cavity |
| `RuinNormalStrength` | 1.0 | |
| `RuinSootAmountFallback` | 1.0 | used when CPD 3 = 0 |
| `RuinSootStrength` / `RuinSootBreakup` | 1.0 / 0.9 | |
| `RuinSootColor` | (0.075, 0.068, 0.064) | multiplier at full soot (keeps some texture) |
| `RuinDustStrength` / `RuinDustBreakup` / `RuinDustOpacity` | 1.0 / 0.8 / 0.75 | |
| `RuinDustColor` | (0.235, 0.212, 0.180) linear | |
| `RuinMatteRoughness` | 0.97 | under soot and dust |
| `RuinRubbleRoughness` | 0.90 | |
| `RuinTintFallback` | (1, 1, 1) | used when CPD 0-2 are all 0 |
| `RuinPlasterColor` / `RuinBrickColor` / `RuinRubbleColor` | (1, 1, 1) | per-instance colour multipliers |
| `RuinCPD_TintSoot` | CPD 0-3 | not an instance knob |

The hole instance also has the library's `WallGrid*` parameters (NoGrid: `WallGridColumns` 0) and `WallHoleInnerDarken`
0.65 / `WallHoleInnerColor` (0.085, 0.06, 0.05).

Defaults live in `Source/Tools/BerlinMap/ruinkit/ruin_params.py`. Both the builder and the texture preview read them.

## Hole variant

- Built only by the shared library, with nothing edited afterwards:
  - `masked_master_for(M_BerlinRuin, inner_tint=True)`: a masked copy with the WallHoles Custom node. It reads CPD 8-34
    through the same parameters, and `RTS.WallChunks.FinishMaskedMaster` reports `masked RESULT DONE ... assumedOpaque=0`.
  - `make_holes_instance(MI_BerlinRuin_Wall, grid_axis=None, two_sided=True, inner_tint=True, name='MI_BerlinRuin_Wall_Holes')`.
- The builder then re-applies the wall instance's overrides, so the holed wall matches the intact one.
- **NoGrid:** one instance serves every kit mesh, as on the PostApo ruins. The placeholder mesh passed for the unused
  grid bounds is `/Engine/BasicShapes/Cube`.
- **Two-sided:** a hole shows the inside of the solid wall. Back faces are darkened toward sooty brick (inner tint).
- **Rim:** the library's scorched rim (base x 0.3, roughness to 1) is hardcoded and can't be tuned per material without editing the shared library. It, the inner tint, the CPD fallbacks and the normal direction are right by construction but have **not been rendered in engine yet**; only the numpy preview has been looked at.
- **Contract check:** in the stats run, the `WallHole*`/`WallGrid*` scalar parameters are a superset of
  `M_RusBunker_Concrete_WallHoles`' (35 shared, plus `WallHoleInnerDarken`).
- **Rebuild:** each build deletes `M_BerlinRuin_WallHoles` and makes it again, so it never goes stale against the master.
  `MI_BerlinRuin_Wall_Holes` is kept and reparented, so Blueprint references stay valid. The master graph is rebuilt in
  place, so mesh slots and instances stay valid.

## Stats (SM6, `MaterialEditingLibrary.get_statistics`, 2026-11-12)

The project's baseline, an empty default-lit material, is **284 PS instructions and 3 samplers** (lightmap and
lighting samplers). Every count below includes it.

| Material | PS instr. | Own (minus 284) | Samplers (own) |
|---|---|---|---|
| `MI_BerlinRuin_Wall` | 351 | 67 | 6 (3) |
| `MI_BerlinRuin_Wall_Holes` | 483 | 199 | 7 (4) |
| `MI_BerlinRuin_Rubble` | 315 | 31 | 4 (1) |
| `MI_Berlin_Ruin_Brick` (DBK) | 392 | 108 | 9 (6) |
| `MI_Berlin_Ruin_Cinderblock` (DBK) | 408 | 124 | 10 (7) |
| `MI_Berlin_Ruin_Concrete` (DBK) | 400 | 116 | 10 (7) |
| `M_RusBunker_Concrete` / `_WallHoles` | 319 / 438 | 35 / 154 | 7 / 8 |

- **Absolute target not reachable:** the brief's 150-190 can't be reached on this project, because the baseline alone
  is 284. The material's own cost is 67 instructions and 3 samples, against 108-124 and 6-7 for DBK.
- **Holes:** 13 of the hole instance's extra 132 instructions come from the two-sided override and the inner-tint back-face path it activates (470 for the masked master alone).
- Neither permutation has compile errors.

## Rebuild

```
python Source/Tools/BerlinMap/ruinkit/ruin_textures_gen.py           # PNGs + Saved/Berlin/ruinkit_textures/ruinkit_preview.png
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/ruinkit/ruin_material_build.py -unattended -nosplash
set RUIN_MODE=stats  (or all)  + -AllowCommandletRendering            # stats -> Saved/Berlin/ruinkit_textures/ruinkit_stats.txt
```

- Run Unreal headless only, never in the shared editor.
- Don't wrap the commandlet in a pipe that closes early (`| Select-Object -First`): that kills it mid-run. Redirect to a
  file instead.
- `MaterialEditingLibrary.delete_all_material_expressions` removes only about half the nodes per call; the builder loops
  until the graph is empty.

## Notes for meshes and integration

- **Vertex colour is most of the look.**
  - An unpainted mesh (white vertex colour) reads as fully sooty, bare, dusty brick.
  - Black vertex colour reads as clean plaster.
  - Paint G deliberately; a smooth gradient is enough, because the texture makes the edge.
- **UV:** keep box projection at 1 unit = 4 m. The normal maps assume V runs down the wall, as in Unreal's default
  projection; a flipped V only inverts the faint bump lighting.
- **Hole component:**
  - `DamagedMaterials[0] = MI_BerlinRuin_Wall_Holes`; slot 1 (rubble) gets none.
  - NoGrid; the PostApo ruin settings are a good start.
  - Debris pieces should use `MI_BerlinRuin_Wall_Holes`, which carries the Niagara mesh usage flag.
- **Per actor:** `SetCustomPrimitiveDataFloat` 0-3, or the Custom Primitive Data defaults on the mesh component; see
  the encoding under Inputs.
