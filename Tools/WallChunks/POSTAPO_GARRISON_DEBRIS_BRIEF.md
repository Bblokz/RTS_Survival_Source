# Garrison PostApo highrises: debris pieces for procedural wall holes (agent brief, 2026-11-11)

The lead is putting the procedural shell holes (`UWallHoleDamageComponent`, "A + B") on three garrisonable ruin
Blueprints (`GarrisonableBuildings/PostApoNuclear/Garrison_PostApo_HighriseLarge1`, `_HighriseLarge2`,
`_LargeHorizontal`). When a shell cuts a hole, a few pooled Niagara mesh pieces fly off. **You build those pieces in
Blender, offline only.** Never touch the running Unreal editor (no UECP calls, no UnrealEditor launches); never commit.
Another session is doing the same for seven other PostApo ruins (`Saved/PostApoHoles/`,
`D:/Blender/BlenderProjects/BlenderTanks/PostApoDebris/`): read its files freely, do not write to them.

Background (skim): `D:/UE5Projects/RTS_Survival/Source/Docs/Environment/WallChunkDamage.md` ("A + B: procedural
holes", "Wire fence and electricity pylons" -> Debris), and the closest precedent script
`D:/Blender/BlenderProjects/BlenderTanks/IndustrialDebris/build_industrial_debris.py` (pieces cut from the props' own
FBX so UVs match; its header documents units, export settings and the C++ contract). Copy its helpers, don't import
across folders.

## Sources

FBX exports (UE StaticMeshExporterFBX, LOD0, no collision; check units/axes yourself, the peer found UE mesh space =
(100 x, -100 y, 100 z) cm from Blender metres): `D:/UE5Projects/RTS_Survival/Saved/PostApoGarrisonHoles/export/`
- `SM_DestroyedHighrise_Large_1.fbx` (11.4 x 8.3 x 16.4 m mesh units; the BP scales it x1.4)
- `SM_DestroyedHighrise_Large_2.fbx` (11.3 x 8.1 x 16.3 m; BP x1.4)
- `SM_DestroyedHighrise_HorizontalLarge_1.fbx` (20.9 x 5.2 x 5.2 m; BP x2.5)
All one slot, `M_ScorchAtlas` (scorched concrete / brick / plaster atlas). Diffuse for previews:
`D:/Blender/BlenderProjects/BlenderTanks/AA_LaptopWork/PostApo/AbaondonNuclearSet/T_ScorchAtlas_Diffuse.PNG` (4k).
Blender: `D:/Blender/BlenderSoftware/4.0/blender.exe -b -P <script> -- ...`.

## Your set (the lead assigns one per agent)

- **Set A, highrise:** 8 pieces cut from `SM_DestroyedHighrise_Large_1` and `_Large_2` (e.g. 4 + 4), output
  `SM_GarrisonHighrise_Debris_0..7.fbx`. Used by both highrise Blueprints.
- **Set B, horizontal block:** 8 pieces cut from `SM_DestroyedHighrise_HorizontalLarge_1`, output
  `SM_GarrisonHorizontal_Debris_0..7.fbx`.

## Debris contract (same as every other hole rollout)

- 8 pieces, each < ~250 tris, closed or at least two-sided-safe (they tumble).
- Cut them from the facade (outer wall panels around windows, floor-slab edges, balcony slab pieces, a corner
  chunk) so the outer faces keep the source's original UVs (UV channel 0 only; drop the lightmap UV). Broken / cut
  faces get a planar UV at the source's texel density, anchored in an atlas area that looks like broken concrete
  (look at the atlas and pick it; say which UV rect you used).
- Orientation: X = longest side, Y = thinnest, origin at the bounds centre (the C++ sizes a piece by bounds X * Z).
- Material slot named exactly as in the source FBX (one slot per piece).
- Sizes in MESH units (before the Blueprint scale): 25-120 cm on the long side; a mix of flat wall slabs (10-25 cm
  thick), chunky blocks, a window-frame corner and a slab edge. A piece or two with a re-bar stub look if cheap.
- Export exactly like the precedent (`export_fbx`) so it imports into UE at scale 1.0. State the import scale you
  expect.
- Write a preview render (`preview_<set>.png`, all 8 pieces laid out, textured with the atlas) and look at it with
  Read before reporting. Fix anything that reads as a featureless grey block or a wrong atlas area.
- Output: `D:/UE5Projects/RTS_Survival/Saved/PostApoGarrisonHoles/debris_out/`. Scripts:
  `D:/Blender/BlenderProjects/BlenderTanks/PostApoGarrisonDebris/` (create it; one script per set is fine, name it
  after the set so two agents never write the same file).

## Report back (short)

File names, tri counts, bounds (cm) per piece, which atlas rect the cut faces use, import scale, preview path,
anything doubtful.
