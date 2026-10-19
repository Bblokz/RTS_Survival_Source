# Local volumetric god rays

## Placement and local controls

The first tuned zone is `GodRays_Factory`, in the `Lighting/GodRays` Outliner folder of `Ger_MarderStugMission`.

Place or duplicate `/Game/RTS_Survival/SpecialEffects/Atmosphere/BP_GodRayZone`. It adds a single persistent CPU Niagara particle with a true Volume-domain material. Its density receives the scene's volumetric sunlight and shadows. No Blueprint Tick drives the effect.

Keep the actor upright. Move it to the area that should catch sunlight; rotate around Z to orient its elliptical footprint. `ZoneExtents` gives half dimensions in centimetres. Uniform actor scale also changes the density footprint; use `ZoneExtents` for independent width, length and height. The editor-only box guides placement; the actual density fades inside an ellipsoid. The guide has no collision and is absent from packaged play.

| Control | Use |
| --- | --- |
| Density | Start at 0.0045. Raising it reveals more light but also obscures surfaces. |
| ZoneExtents | Default half dimensions: 2600 × 3200 × 1800 cm. |
| EdgeSoftness | Default 0.65; higher values produce a wider soft boundary. |
| NoiseScale | Default 1500 cm; controls the size of internal wisps. |
| NoiseStrength | Default 0.25; zero gives a smooth pocket. |
| DriftVelocity | Default (4, 1, 0.3) cm/s in world space, roughly 2.5 m/min total. Set all axes to zero to freeze the density noise. |
| HeightBias | Zero gives even vertical distribution; higher values reduce density toward the top. |
| FogAlbedo | Subtle atmospheric tint; the sun supplies the illumination. |
| ZoneFade | Zero removes this zone's density; one gives full strength. |

Construction Script updates local settings when editing the actor. BeginPlay also applies the settings. After changing several controls from gameplay logic, call `RefreshGodRays`. For animated fades, call `SetGodRayFade(NewFade)`; it updates density without restarting the particle. Both functions are Blueprint-callable.

In an editor viewport that is not ticking Niagara, preview a fade by editing `ZoneFade` in Details, which reruns construction. The runtime fade function propagates its renderer parameters on the next Niagara simulation tick.

These zones control where atmospheric density exists. Their visible shafts depend on the current sun, moving cloud openings, occluders, and camera angle. Duplicate zones share the Niagara/material assets but have independent parameter overrides.

## Shared mission lighting

The mission enables Volumetric Fog on its existing Exponential Height Fog actor. The atmospheric base is deliberately thin:

- Volumetric extinction scale: 0.015.
- Scattering distribution: 0.15.
- Volumetric view distance: 24000 cm. Very distant cameras fade the effect; adjust this if gameplay zoom changes substantially.
- Sun volumetric scattering intensity: 3.5.
- SkyLight volumetric scattering intensity: 0.05.

The existing ordinary fog density, sun intensity/rotation/colour, skylight surface intensity and post-process treatment are retained. The volumetric system changes how nearby fog is lit, so it can alter the appearance even outside a placed zone slightly.

The sun uses `/Game/RTS_Survival/Materials/Sky/MI_LF_CNC_MarderGodRays`, based on the dedicated `M_LF_CNC_MarderGodRays`. This preserves the previous slow broad-cloud parameters and adds finer moving openings. These controls affect the shared sun across the map:

- `RayDetailWorldSize`: 3500 cm.
- `RayDetailStrength`: 0.9. Set to zero to remove only the finer opening pattern.
- `RayDetailDrift`: 3 cm/s (the secondary axis moves at 22% of this speed).

The earlier `M_Clouds_Default`, `M_LF_CNC_MarderSunbreaks` and `MI_LF_CNC_MarderSunbreaks` assets remain available. Reassign the earlier instance to restore the previous cloud mask.

## Assets and verification

- `BP_GodRayZone`: local controls, placement guide, startup and fade logic.
- `NS_CNC_GodRayZone`: one persistent particle with size bound to the local footprint.
- `M_CNC_LocalGodRayDensity`: soft bounded density with two scales of slowly drifting 3D procedural noise; no emissive glow.
- `M_LF_CNC_MarderGodRays` and `MI_LF_CNC_MarderGodRays`: dedicated sun-opening detail.

The volume renderer's four dynamic material bindings intentionally use LinearColor parameter types. These store four unrestricted floats per binding (dimensions/density, edge/noise/fade, drift/height and orientation); they are not displayed colours. Keep the User parameter and binding types consistent when editing the system.

The emitter runs its own infinite lifecycle, bursts one particle at time zero in the first loop only, and does not kill it by age. System warm-up is 15 ticks at 1/30 second. Keep `WarmupTickCount` consistent with `WarmupTime` and `WarmupTickDelta`: writing only the time through an authoring API can leave the actual tick count at zero.

Blueprint compilation, Play-mode startup/fade calls, visual comparisons, asset validation and saved settings were checked during implementation. GPU timing has deliberately not been assessed yet. Local placement does not make the global volumetric grid free; profile the complete effect at the intended gameplay resolution and camera distance before expanding its use.
