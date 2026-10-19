# Fake emissive god rays

## Where to find them

Open `Ger_MarderStugMission` and expand `Lighting/FakeGodRays` in the Outliner. Three independently controlled rays are placed near the factory:

- `FakeGodRays_Factory_01`
- `FakeGodRays_Factory_02`
- `FakeGodRays_Factory_03`

Duplicate one of these actors, or place `/Game/RTS_Survival/SpecialEffects/Atmosphere/BP_FakeGodRayZone` from the Content Browser. Each actor represents one shaft, made from three crossed, two-sided planes. Each actor owns its dynamic material instance; changing one ray does not change the others.

The actor origin is the lower end of the shaft. Its local Z axis points toward the upper end. The placed rays have an art-directed gentle lean, about 63 degrees above the ground, reduced from the earlier 45-degree version. Their directions differ slightly. They do not automatically follow the mission sun; rotate individual actors to aim them. `BeamLength` and `BeamWidth` are centimetres. Actor scaling also scales the geometry. The placed widths are 2400, 3100 and 1900 cm; feathering makes the visible glow narrower than those full bounds.

## Useful controls

| Control | Effect |
| --- | --- |
| EmissiveStrength | Direct HDR brightness, independent of fog density. Default 1.5; try 3–6 for a stronger effect. Values above 10 can become very bright at the mission exposure. |
| RayOpacity | Transparency, independent of brightness. Default 0.06. |
| RayColor | Local core tint; defaults to warm cream. Tint fades toward a less saturated colour in the halo and varies along the shaft. |
| RayFade | Overall visibility from 0 to 1. |
| BeamLength / BeamWidth | Length and full width of this shaft. |
| EdgeSoftness / EndSoftness | Feathering across the beam and at its ends. |
| CoreWidth / CoreStrength | Shape and brightness of the central glow. |
| StreakDensity / StreakStrength | Number and contrast of the fine streaks. |
| MotionSpeed | Shader-driven streak motion. Default 0.014; zero freezes it and negative values reverse it. |
| NoiseStrength / Seed | Controls soft gaps, uneven width, and brightness variation along each ribbon; default strength 0.75. Seed gives each duplicate a different pattern. Streak paths stay nearly straight. |
| IntersectionFadeDistance | Softens intersections with opaque surfaces. |
| CameraFadeDistance | Hides close-up planar intersections near the camera. |

Editing the actor reruns its construction setup. BeginPlay also recreates and configures its material. For gameplay control, call `SetEmissiveStrength(NewValue)` or `SetRayFade(NewValue)` for immediate updates. If changing several other settings at runtime, call `RefreshFakeGodRays` afterward.

## Rendering behavior

`M_FakeGodRay_Emissive` is an Unlit, Additive, two-sided material. Asymmetric feathering, interrupted ribbons, view-angle fading, depth fading and slow procedural motion provide the appearance of illuminated air. The wide shafts contain soft dim areas and brighter cores instead of a uniform fill. A spatially varying desaturation mask blends the core tint into paler patches and edges. Brightness can be increased without thickening volumetric fog. Bloom depends on the scene's existing exposure, bloom strength and threshold; these global settings were not increased for the fake rays.

The rays are visual glow. They do not illuminate terrain or units, and do not receive volumetric shadowing. Opaque geometry still depth-occludes their visible surfaces. Their components have collision, shadows and indirect-light influence disabled. The existing real volumetric setup and sunlight materials are retained.

Three placed actors use nine planes total. They have no Blueprint Tick or Niagara simulation; material time drives motion. Transparency coverage and overlapping planes still cost GPU time. GPU timing is intentionally deferred until the appearance is approved.

The new assets are `BP_FakeGodRayZone` and `M_FakeGodRay_Emissive`, both under `/Game/RTS_Survival/SpecialEffects/Atmosphere`. A recovery copy of the mission before placement is at `Saved/CodexBackups/FakeGodRays_20261017/BeforeFakeGodRays.umap`.

The mission and both fake-ray assets from before the wider, angled refinement are preserved in `Saved/CodexBackups/FakeGodRaysRefinement_20261017/`. Refined previews are in `Saved/Screenshots/CodexLighting/FakeGodRays_Refined.png` and `FakeGodRays_Refined_Closer.png`.

## German first mission experiment

`Ger_FirstMission` now has twelve widely separated landscape rays, `FakeGodRay_Landscape_01` through `_12`, under `Lighting/FakeGodRays_Landscape`. They replace the earlier three experimental actors. The nearest pair is 112.4 metres apart; placement excluded an 80-metre radius around outpost building classes and required a landscape collision hit. Ground anchors are lowered 80 cm for soft intersections.

These actors use the dedicated `BP_FakeGodRayFirstMission` and `M_FakeGodRay_FirstMission`. Their cool blue-white tints, gentle lean, uneven masks, widths and brightness vary between instances. Their material samples the same moving cloud field as the mission sun at each actor's ground anchor, fading out as transmission drops from 0.70 to 0.40. Their positions remain fixed; only visibility and internal wisps change. No atmospheric fog, real lights or exposure adjustments were added.

The sun now uses `M_LF_FirstMissionSunbreaks`, combining the original two panning texture layers with the Marder/Stug broad sunbreak treatment and fine moving openings. Both the sunlight and ray materials call `MF_FirstMissionSunbreakField` and share `MPC_FirstMissionSunbreaks` controls. The original shared materials and the Marder/Stug setup were not modified. See `FirstMissionSunbreaks.md` for controls and verification, and `FirstMissionSunbreakRayPlacements.json` for the placement-time brightness samples and coordinates.

The before/after screenshots use the same camera at `(-16500, 16600, 7000)`, pitch `-57`, yaw `-88`, and resolution `1600 x 1000`. They are `Saved/Screenshots/CodexLighting/FirstMission_FakeRays_Before.png` and `FirstMission_FakeRays_After.png`. The original mission is backed up at `Saved/CodexBackups/FirstMissionFakeRays_20261017/BeforeFakeRays.umap`.

The earlier three-ray landscape preview is `Saved/Screenshots/CodexLighting/FirstMission_LandscapeRays.png`. Current sparse-layout previews are `FirstMission_SparseSunbreakRays_North.png` and `FirstMission_SparseSunbreakRays_Final.png` in the same directory. The earlier outpost placement is preserved in `Saved/CodexBackups/FirstMissionFakeRays_20261017/BeforeLandscapeRelocation.umap`; the three-ray landscape version is preserved in `Saved/CodexBackups/FirstMissionSunbreaks_20261017/BeforeSparseSunbreakRays.umap`.
