# Landscape Data Texture setup

`ALandscapeDataManager` turns one-time PCG footprints into a transient CPU-authored
texture for the Landscape material. It does not spawn writer meshes and does not use
an RVT.

## Channel contract

| Texture channel | Data channel |
| --- | --- |
| R | Scorched |
| G | Gravel |
| B | Concrete |
| A | Extra Sand |

Contributions in the same channel are combined with `max`. Different channels remain
independent, so the Landscape material decides how competing surface layers are
prioritized or normalized.

## Required Landscape material parameters

Create these parameters in every material used by the target Landscape:

| Parameter | Type | Runtime value |
| --- | --- | --- |
| `LandscapeDataTexture` | Texture 2D | Immutable RGBA mask published by the manager |
| `LandscapeDataWorldMinAndInvSize` | Vector | RG = world minimum XY, BA = inverse world size XY |
| `LandscapeDataTextureMetrics` | Vector | RG = resolution XY, BA = world units per texel XY |

Use a custom 1x1 **transparent black** `(0, 0, 0, 0)` texture as the texture
parameter's default. The usual opaque black texture has alpha 1 and would enable
Extra Sand everywhere if setup validation prevents the runtime texture from binding.

Set the texture sample's sampler type to `Masks` (or `Linear Color` with sRGB off).
The runtime texture is non-sRGB, non-streaming, trilinear, and clamp-addressed.

Build the UV as:

```text
LandscapeXY = AbsoluteWorldPosition.xy
Mapping = LandscapeDataWorldMinAndInvSize
LandscapeDataUV = (LandscapeXY - Mapping.rg) * Mapping.ba
```

Feed `LandscapeDataUV` into `LandscapeDataTexture`, then use its R/G/B/A outputs to
blend the Landscape layers. The material parameter names can be changed on the
manager if the material already follows another convention.

The Landscape must have **Use Dynamic Material Instance** enabled before PCG
generation or Play begins. The manager deliberately validates this instead of
re-registering every Landscape component at runtime, which avoids a large and risky
BeginPlay hitch. `Validate Landscape Data Setup` checks that loaded Landscape MIDs
contain all configured parameters and that the published values reached them.

## Level setup

1. Place exactly one `LandscapeDataManager` in the level. Assign `Landscape` when the
   world contains more than one main Landscape actor.
2. Enable **Use Dynamic Material Instance** on the Landscape and its streaming
   proxies, and add the material parameters above.
3. Select the manager and run **Refresh Bounds From Landscape** after changing the
   Landscape extent. The complete bounds are also refreshed when the manager is
   constructed or saved in editor.
4. Run **Validate Landscape Data Setup** before cooking.

The manager class defaults to non-spatial loading, so a placed manager remains alive
while World Partition cells stream. In a partitioned world, shipping builds use the
complete bounds serialized in editor; they never reinterpret existing pixels from a
partial set of loaded proxies. Newly streamed Landscape proxies receive the current
texture and mapping through the level-added callback.

In a non-partitioned procedural map, the manager derives its bounds from
`ALandscape::GetLoadedBounds()`. For a map spanning `-25,500` to `+25,500` cm, the
default 25 cm/texel requests 2,040 texels and rounds to a 2048x2048 power-of-two
texture. If 25,500 cm is the entire span, it rounds to 1024x1024.

At 2048x2048, the authoritative RGBA8 CPU image is 16 MiB and the complete GPU mip
chain is about 21.3 MiB. Rebuilding temporarily holds the previous snapshot and
construction copies as well. The 4096 limit is intended only for maps that genuinely
need it; it has roughly four times the pixels and a substantially higher rebuild peak.

## PCG nodes

### Mark Points In Landscape

The required input is Point data. Each point writes its transformed local
`BoundsMin`/`BoundsMax` XY footprint. Point density becomes mask strength. Rotation
and negative XY scale are preserved, and the points pass through unchanged.

`Paint Mode` exposes only the settings relevant to the selected footprint:

- **Solid Bounds** preserves the original full rectangular footprint.
- **Radial** creates an ellipse inside the bounds. `Full Strength Radius` controls
  the solid center, `Edge Strength` controls the value at the perimeter, and
  `Falloff Exponent` shapes the fade between them.
- **Perlin Noise** generates thresholded fractal coverage inside the bounds.
  Frequency controls feature size; octaves, persistence, and lacunarity control
  detail; threshold and transition width control breakup; minimum strength can
  retain a continuous base coat beneath the noise.
- **Radial With Noisy Bounds** first chooses a deterministic per-point bounds scale
  from `Bounds Scale Minimum` through `Bounds Scale Maximum`, then applies the radial
  fade through a directionally distorted perimeter. Noise amount, frequency,
  octaves, and persistence control the organic edge.

Noise phases and noisy bounds scales use the PCG node seed, each point seed, and the
point's stable input order. The chosen values are copied into the manager-owned
stamp, so rebuilding the texture does not randomize existing paint. Changing the PCG
seed intentionally produces a new variation.

For Scorched City, branch or pass the final building points through this node with
`Channel = Scorched`, then continue to the building spawner. The marker uses exactly
the same point bounds that describe the spawned building footprint; a separate HISM
plane output is unnecessary.

### Mark Volumes In Landscape

The required input is Volume data. Each volume is rasterized only inside its own
cropped bounds, and its PCG spatial density becomes mask strength. Inputs are sampled
synchronously at the volume bounds-center Z and are never retained. Exact brush
volume sampling requires query-capable collision; a `NoCollision` brush may produce
zero density at runtime.

Both nodes expose the data-channel enum, show the chosen channel in their node title,
and pass input data through unchanged. A managed PCG resource gives each node-stack
invocation a stable contribution:

- successful graph completion atomically commits staged replacements;
- cancellation discards staged replacements, preserves surviving resources' previous
  values, and removes any resource that PCG already hard-released during abort;
- PCG cleanup removes only that resource's contribution;
- source actor EndPlay removes its contributions even when PCG does not call
  `UPCGManagedResource::Release()`;
- a low-frequency game-thread watchdog removes contributions whose PCG component was
  destroyed directly; component identity checks keep obsolete construction-script
  copies from removing contributions already transferred to their replacements;
- duplicated PCG actors receive independent handles, while construction-script
  component reconstruction transfers the existing handle safely.

## Noise-blended areas

For point-sized semantic breakup that CPU queries must also observe, use the node's
`Perlin Noise` or `Radial With Noisy Bounds` mode. This bakes the pattern when the
manager rebuilds; higher octave counts cost more generation time but add no per-frame
rendering cost.

For purely visual, high-frequency breakup across large areas, keep the stored mask
simple and combine that channel with deterministic world-space noise in the
Landscape material, for example:

```text
NoisyArea = AreaMask * SmoothStep(LowThreshold, HighThreshold, WorldSpaceNoise)
LayerResult = Lerp(BaseLayer, TargetLayer, NoisyArea)
```

This supports irregular gravel, concrete breakup, scorched edges, and similar blends
without baking high-frequency noise into the 2048 mask. Keep the manager texture for
semantic placement and generate visual detail in the material.

## Publication and lifetime guarantees

The manager owns the authoritative `TArray<FColor>` and never edits a texture already
visible to rendering. A rebuild creates a new transient `UTexture2D`, fills mip 0 and
all average-filtered lower mips, calls `UpdateResource()` once, and then swaps the
Landscape MID parameters. Render-command ordering makes the swap safe without a
render-thread flush. The active texture is a transient `TObjectPtr`; Landscape MIDs
also retain it while Unreal retires the previous render resource behind its normal
destruction fence.

All PCG sampling, UObject access, texture creation, and publication run on the game
thread. The manager retains only value data and weak external UObject references. The
Blueprint CPU query `Get Landscape Data At World Position` reads the authoritative
CPU image and performs no GPU readback.
