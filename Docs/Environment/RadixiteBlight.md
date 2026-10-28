# Radixite blight (dead ground around radixite crystals)

Around radixite crystals the landscape is dead:

- the grass turns ash-grey,
- the soil darkens, with a faint radixite tint near the crystals,
- landscape grass doesn't grow, apart from a ring of dead straw at the edge.

It's drawn by the landscape material, not by decals or meshes, so the crystals' own radixite decals stay on top.

| Part | Where |
|---|---|
| Data | `URadixiteBlightSubsystem` (`Source/RTS_Survival/Environment/RadixiteBlight`) writes up to 8 circles into `MPC_RadixiteBlight` (`/Game/RTS_Survival/Materials/ground/Landscape`) |
| Shader | `MF_RTS_LS_RadixiteBlight`, built by `Source/Docs/Landscape/ls_blight_build.py` |
| Masters | `M_RTS_Landscape_DryCastle` and `M_RTS_Landscape_DesertPaintable`, behind static switch `LS_RadixiteBlight`, just before the colour variation (so the RVT holds it); wired by `ls_master_build.run()` |
| Grass | `MF_RTS_LS_GrassWeights`' `BlightMask` input: green grass and flowers die, dead straw rings the edge |

## How the circles are made

1. **Find the crystals.** Every actor with a `UResourceComponent` of type `Resource_Radixite` counts, whether hand-placed or spawned by `PCGCreateRadixiteField`.
2. **Size each crystal's circle.** The radius is 2.5× the crystal's colliding footprint, clamped to 5–16 m. Colliding components only, because a crystal's destruction geometry spans tens of metres.
3. **Merge down to 8.** While there are more than 8 circles, the pair whose enclosing circle is smallest is merged. The log (`LogRTSRadixiteBlight`) reports how many crystals went into how many circles.
4. **Write the collection.** The subsystem writes `RadixiteBlight0–7` (centre x, y, radius, 1 / radius²), `RadixiteBlightBounds` (all circles plus a 50% margin for the edge noise) and `RadixiteBlightCount`.

**When it runs:** when a game or PIE world begins play. In the editor it also runs when a map opens, and again whenever a radixite actor is added, moved or deleted, one frame later. So the editor view and the landscape grass bake both see the current blight.

## Keeping the grass in step (editor)

Landscape grass is baked into the level, so the dead ground loses its grass only after a grass rebuild. The subsystem tracks this the way the editor tracks lighting:

1. **Fingerprint.** It fingerprints the current circles, rounded to whole centimetres.
2. **Warning.** If the landscape's material shows both `LS_RadixiteBlight` and `LS_GroundGrass`, and the fingerprint differs from the one stored on the landscape actor, every viewport shows in red: *"RADIXITE GRASS NEEDS TO BE REBUILT (radixite crystals changed): run RTS.Radixite.RebuildGrass, then save the map"*. The stored fingerprint is an actor tag, `RadixiteBlightGrass_<hex>`.
   - A map with crystals that was never baked warns.
   - A map without crystals never warns.
3. **Fix.** `RTS.Radixite.RebuildGrass` (or `unreal.RadixiteBlightSubsystem.rebuild_blight_grass(world)`) refreshes the circles, rebuilds the grass maps (`ULandscapeSubsystem::BuildGrassMaps`, the same as Build → Build Grass Maps Only) and stores the fingerprint. Save the map afterwards.

**Sculpting and painting** in Landscape mode rebuild the touched components' grass with the current blight, so they don't make the grass stale. If anything reads the RVT, `r.VT.Flush` redraws its pages.

**The editor needs a Development DLL with this code.** On an older DLL the editor shows no blight, never warns, and every grass rebuild (automatic ones included) puts grass back on the dead ground.

**Headless rebake:** a DebugGame commandlet can do it while no editor has the map open: `-run=pythonscript -AllowCommandletRendering`, `load_map`, `rebuild_blight_grass`, `save_map`. It was tested on Ger_Trains; the save failed only because an editor had the map open.

**Why a collection:** the landscape instances are shared by many maps, so per-map data can't live in instance parameters.

The blight is static: harvesting a crystal doesn't heal the ground, and the baked grass stays valid.

## Shader

`MF_RTS_LS_RadixiteBlight(Attributes, DirtColor optional) -> Attributes, Blight`

- **The custom node.** One node finds the nearest circle, measured in circle radii.
  - Pixels outside `RadixiteBlightBounds`, or on maps without radixite (`RadixiteBlightCount` 0), return after a bounds test.
  - The edge noise (`T_RTS_PackedNoise`, 6 m tile) is only sampled close to a circle. It's taken with gradients computed before the branch, so the mip selection stays correct.
- **The look:**
  - The ground is desaturated (`Blight_AshTint`), mixed partly with bare soil (`Blight_SoilAmount`, using the breakup's DirtColor or `Blight_DirtColor`) and darkened (`Blight_Darken`).
  - A radixite tint fades in near the centres (`Blight_RadixiteTint`, `Blight_TintAmount`, `Blight_CoreSize`).
  - The dead ground is rough (`Blight_Roughness`).
  - The edge is set by `Blight_EdgeNoise` and `Blight_EdgeWidth`.

## Gotchas (2026-10-28)

- **The Python name is `unreal.MaterialLibrary`.** `UKismetMaterialLibrary` is not `unreal.KismetMaterialLibrary`; use `unreal.MaterialLibrary.set_vector_parameter_value(world, mpc, name, value)` to fill the collection from Python.
- **Use colliding bounds for crystals.** Their full bounds include a destruction geometry collection about 40 × 80 m.
