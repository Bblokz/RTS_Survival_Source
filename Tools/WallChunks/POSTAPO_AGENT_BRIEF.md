# PostApo ruins: debris pieces + collision for procedural wall holes (agent brief, 2026-11-11)

The lead is putting the procedural shell holes (`UWallHoleDamageComponent`, "A + B") on seven PostApo ruin
Blueprints (`/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/BP_Dest_PostApo_*`). The lead does
all Unreal work (materials, components, imports, tests). **You do Blender / offline work only.** Never touch the
running Unreal editor (no UECP calls, no UnrealEditor launches); never commit.

Background (skim): `Source/Docs/Environment/WallChunkDamage.md` ("A + B: procedural holes", "Wire fence and
electricity pylons"), and the closest precedent script
`D:/Blender/BlenderProjects/BlenderTanks/IndustrialDebris/build_industrial_debris.py` (pieces cut from the props' own
FBX so UVs match; header comment documents units, export settings and the C++ contract). Reuse its helpers by copying.

## Sources

- FBX exports of the seven meshes (UE StaticMeshExporterFBX, LOD0, no collision; cm units, one root scaled like the
  industrial exports — check): `D:/UE5Projects/RTS_Survival/Saved/PostApoHoles/export/<Mesh>.fbx`
  - `SM_NuclearDestroyed` (13.3 x 13.3 x 19.6 m), `SM_HighRiseConstruction_1` (9 x 4.8 x 16 m),
    `SM_DestroyedHighrise_1` (6.6 x 2.4 x 20 m), `SM_DestroyedHighrise_2` (6.6 x 2.4 x 10.8 m),
    `SM_DestroyedChimney` (4.6 x 4.6 x 16.8 m): one slot, `M_ScorchAtlas` (scorched concrete / brick atlas).
  - `SM_DestroyedHighrise_Crane` (8.5 x 1.3 x 20 m), one slot `M_ScorchAtlas`.
  - `SM_Scorch_Crane_Destroyed` (6.6 x 16 x 7.4 m): slots `M_Scorch` (M_ScorchAtlas), `M_Container`
    (M_container_blue), `M_Glass`.
- Textures for previews: `D:/Blender/BlenderProjects/BlenderTanks/AA_LaptopWork/PostApo/AbaondonNuclearSet/T_ScorchAtlas_Diffuse.PNG`
  (4k). Container colour: `T_Building_Green` is not on disk; a flat blue-green is fine for previews.
- Blender: `D:/Blender/BlenderSoftware/4.0/blender.exe -b -P <script> -- ...` (4.0 is what the precedent used).

## Debris contract (same as every other hole rollout)

- 8 pieces per set, `SM_<Source>_Debris_0..7.fbx`, each < ~250 tris, closed or at least two-sided-safe (they tumble).
- Surface faces keep the source's original UVs (UV channel 0 only; drop the lightmap UV). Broken / cut faces get a
  planar UV at the source's texel density, anchored in an atlas area that looks like broken concrete / rust (look at
  the atlas and pick it; say which UV rect you used).
- Orientation: X = longest side, Y = thinnest, origin at the bounds centre (the C++ sizes a piece by bounds X * Z).
- Material slot names exactly as in the source FBX (one slot per piece unless stated).
- Export exactly like the precedent (`export_fbx`): imports into UE at scale 1.0. State the import scale you expect.
- Sizes: concrete chunks roughly 25-120 cm on their long side, a mix of slabs, chunky blocks and a piece or two with a
  re-bar stub look (if cheap); steel pieces 60-250 cm long.
- Write a preview render per set (`preview_<Source>.png`, all 8 pieces laid out, textured) and look at it with Read
  before reporting. Fix anything that reads as a featureless grey block or a wrong atlas area.
- Output folder: `D:/UE5Projects/RTS_Survival/Saved/PostApoHoles/debris_out/`. Script(s) go in
  `D:/Blender/BlenderProjects/BlenderTanks/PostApoDebris/` (create it).

## Report back (short)

Per set: file names, tri counts, bounds (cm) per piece, which atlas rect the cut faces use, import scale, preview
path, anything doubtful. For collision: the JSON path, box count, and the gap numbers before/after.
