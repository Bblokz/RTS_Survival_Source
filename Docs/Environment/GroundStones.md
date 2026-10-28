# Ground stones

Low-poly scattered stones for the RTS landscapes, made for Ger_Trains (green and dry grass, orange sand).

## Assets

`/Game/RTS_Survival/Blueprints/Environment/GroundStones/`

- `Meshes/SM_GS_*`: 16 stones, 280–480 triangles each.
  - Fieldstones: `Field_A`–`D`.
  - Angular chunks: `Chunk_A`–`D`.
  - Flat slabs: `Slab_A`–`C`.
  - Long stones: `Long_A`–`B`.
  - Pebble clusters: `Cluster_A`–`C`.
  - Every stone is about 0.5–1.2 m wide at scale 1, with its pivot at the bottom centre.
  - Nanite is off, there is no collision and no distance field, so tanks and infantry drive over them.
- `Textures/T_GroundStone_BC`: 2048 px 4×4 atlas, one 512 px cell per stone. The albedo is a near-neutral grey so the material can colour it.
- `Textures/T_GroundStone_N`: DirectX normal map, baked from a high-poly sculpt of each stone (20k–140k triangles).
- `Textures/T_GroundStone_ORM`: R = ambient occlusion, G = roughness, B = curvature.
- `Materials/M_GroundStone` + `MI_GroundStone`: Default Lit, opaque, Instanced Static Meshes usage on.
- `PCG_GroundStones`: the scatter graph.

## Material

- **Palette:** six colours (`Palette0_Ochre` … `Palette5_IronStained`). Large-scale world noise sampled at each instance's position picks one, so neighbouring stones share a rock type.
  - `Palette_WorldSize` sets the size of those areas and `Palette_Contrast` sets how evenly the six colours are used. With lower contrast the greys in the middle bins dominate.
  - This works for PCG, foliage-tool and hand-placed stones without any per-instance data.
- **Per-instance variation:** `PerInstanceRandom` adds brightness jitter (`ValueJitter`) and a cool/warm hue shift (`HueCool` / `HueWarm`).
- **Ground contact:** within `Contact_Height` cm of the ground, measured with `RVT_Height`, the stone takes the landscape colour from `RVT_Environment` and a flatter normal. Stones therefore sink visually into sand or grass.
- **Top dust:** up-facing areas collect a lightened, desaturated copy of the ground colour (`Dust_Strength`).
- **Lighting:** AO and roughness come from the ORM texture, and Specular is 0.35. Stones work with the Lumen and VSM setup: their shadows are static and cached, and there are no distance fields.

## Placement (PCG)

`PCG_GroundStones` runs on a `PCGVolume` labelled `PCG_GroundStones`. Generation is on demand, so press Generate on the volume's PCG component. The instances are saved with the level. The volume's brush has collision set to NoCollision. With its default query collision, its top face caught world-static ground traces: the crop fields put every plant on that face, 24 m above the farms. `ue_pcg_spawn.py` sets NoCollision every time it runs.

There are two branches, each with its own cluster noise so boulder chunks and pebble chunks do not line up:

| Branch | Sampler | Cluster noise (Perlin, 5 octaves) | Thinning | Ground | Slope | Scale | Cull start–end |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Boulders (fieldstones, chunks, slabs, long stones) | 0.014 / m² | ~70 m features, seed 4242, rotated 23°, keep ≥ 0.52 | density × random, keep ≥ 0.28 | `GrassCastle` ≥ 0.6 | ≤ 28° | 0.7–1.9, sunk 4–14 cm | 180–240 m |
| Pebbles (clusters + small stones) | 0.05 / m² | ~32 m features, seed 9001, rotated −37°, keep ≥ 0.52 | density × random, keep ≥ 0.3 | `GrassCastle` ≥ 0.6 | ≤ 32° | 0.7–1.4, sunk 1–5 cm | 90–130 m |

The random thinning step breaks up the sampler's jittered grid, and the grass filter keeps stones on the green `GrassCastle` layer only (the landscape provides layer weights as point attributes). On Ger_Trains this gives about 1,800 instances: about 330 boulders and the rest pebbles. Raise the sampler densities or lower the thresholds for more.

Stones are spawned as HISM components with density scaling on, so `foliage.DensityScale` (the Stones setting in the graphics menu) thins them in game.

**Exclusions** (tags set by `ue_tag_exclusions.py`):
- `PCG_NoStones`: the actor's component bounds are subtracted. Covers buildings, roads, rails, crop fields, fences and water meshes.
- `PCG_NoStonesPath`: ground path splines. The graph samples the spline every 1.5 m and subtracts a band 3.5 m wide on each side.
- `PCG_NoStonesBounds`: path decals (junctions, fades). The actor bounds are subtracted.
- To tag a level, run `ue_tag_exclusions.py` (in the pipeline folder). It skips volumes, lights, FX, markers and units.
- Re-run it after adding actors, or add the tag by hand.

## Pipeline

All scripts are in `D:/Blender/BlenderProjects/BlenderTanks/GroundStones/`:

- `build_stones.py` (Blender 4.0): sculpts and bakes all 16 stones.
  - Run: `blender -b -P build_stones.py`. It takes about 1.5 minutes.
  - The albedo is triplanar-projected from the scanned `T_Ground_Rock_*_Near_01_BC`.
- `compose_atlas.py`: builds the atlas, with pull-push filling between islands so mips stay clean.
- `render_preview.py`: renders `renders/overview.png` and `renders/closeup.png`.
- `ue_import.py`, `ue_material.py`, `ue_tag_exclusions.py`, `ue_pcg_spawn.py`: UE side. Run them through the UECP Python bridge with `sys.path.insert(0, folder)`, then import the module.

Warning: never call UECP `spawn_pcg_actor` without explicit bounds. It walks the landscape and crashes the editor. Use `ue_pcg_spawn.run(center, extent)` instead.
