# German first mission: moving sunbreaks and sparse fake rays

`Ger_FirstMission` has twelve landscape shafts, named `FakeGodRay_Landscape_01` through `_12`, in `Lighting/FakeGodRays_Landscape`. Their nearest separation is 11239.5 cm. They were selected from bright samples of the rendered cloud mask, with a minimum 11000 cm spacing, an 8000 cm exclusion around building/outpost classes, and a landscape collision hit at each anchor. Placement-time values and coordinates are in `FirstMissionSunbreakRayPlacements.json`.

## One field drives sunlight and ray visibility

The sun's dedicated light function is `/Game/RTS_Survival/Materials/Sky/M_LF_FirstMissionSunbreaks`. It and the mission's `M_FakeGodRay_FirstMission` call `MF_FirstMissionSunbreakField`:

- The original first-mission texture effect remains: the red channel of `T_RTS_PackedNoise` pans at world sizes 45000 and 135000 cm with the original two velocities, power 0.9 and contrast 0.1.
- A second pair of noise samples provides the Marder/Stug broad sunbreak treatment: 36000/108000 cm, contrast 1.8, power 1.2, drift scale 0.55.
- The Marder/Stug procedural openings add finer detail at 3500 cm and drift 3 cm/s, with a secondary axis at 22% of that speed.
- The sunlight multiplies these layers. Each ray samples the identical field at its actor origin, then multiplies its existing feathered opacity by `smoothstep(0.40, 0.70, Transmission)`.

The actor anchors remain fixed. Moving shade fades a shaft out; a passing opening brings it back. The actors do not slide across the terrain. This keeps the effect sparse while tying its visibility to the moving lighting. Material time drives all motion; no actor Tick, timers, Niagara system or extra light is needed.

## Shared controls

Edit `/Game/RTS_Survival/Materials/Sky/MPC_FirstMissionSunbreaks` to affect both sunlight and ray visibility together. Gameplay can set these collection parameters as well.

| Parameter | Default | Purpose |
| --- | --- | --- |
| LegacyWeight | 0.35 | Contribution of the original panning cloud mask. |
| SunbreakWorldSize | 36000 cm | Width of the broad moving noise field; the second layer is three times larger. |
| SunbreakContrast | 1.8 | Separation between broad open and shaded regions. |
| CloudCoverPower | 1.2 | Balance of cloud coverage. |
| CloudDriftScale | 0.55 | Broad-layer motion speed. |
| ShadeTransmission / SunTransmission | 0.30 / 1.0 | Direct sunlight in shaded and fully open areas. |
| RayDetailWorldSize | 3500 cm | Scale of the smaller moving openings. |
| RayDetailStrength | 0.9 | Strength of fine opening detail. |
| RayDetailDrift | 3 cm/s | Fine opening motion. |
| RayDetailFloor | 0.65 | Minimum contribution of fine detail to sunlight. |

For the original texture treatment alone, set LegacyWeight to 1, ShadeTransmission and SunTransmission to 1, and RayDetailFloor to 1. Reassigning `M_Clouds_Default` on the sun restores the original light function completely, but also breaks its synchronization with the dedicated ray material.

Use `BP_FakeGodRayFirstMission` for further cloud-linked placements. The original `BP_FakeGodRayZone` still creates the independent, ungated material used in Marder/Stug. Local colour, brightness, width, fade and wisp controls remain on each actor. The placed shafts vary in length from 4400 to 5840 cm, width from 1400 to 2060 cm, and emissive strength from 2.8 to 3.7.

## Placement probe and validation

`M_FirstMissionSunbreakProbe` renders the same field over XY bounds (-34000, -35000) to (28000, 33000). A 256 x 256 render target was read once per placement pass to choose bright, separated terrain locations. The probe is an authoring aid and is not placed in the level or rendered during gameplay.

The mission-specific Blueprint compiled with no errors or warnings, and all six new assets validated. The shared transmission was tested at zero through the collection and then restored; its rendered maximum reached zero and recovered above 0.9. Visual checks covered multiple map areas. The topology validator reports function-input sources as island warnings because it does not traverse Material Function call inputs; the materials compile and the rendered probe varies spatially as expected.

No atmospheric fog, extra lights, exposure changes or edits to the original cloud/fake-ray materials were made. The original `M_Clouds_Default`, `M_LF_CNC_MarderGodRays` and `M_FakeGodRay_Emissive` file hashes remained unchanged. GPU cost has not been profiled; twelve shafts use 36 crossed planes, and their shared field adds texture samples to the ray material.

The recovery map is `Saved/CodexBackups/FirstMissionSunbreaks_20261017/BeforeSparseSunbreakRays.umap`. Screenshots are in `Saved/Screenshots/CodexLighting/FirstMission_SparseSunbreakRays_*.png`.
