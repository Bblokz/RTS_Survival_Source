# Ger_Trains lighting

Ger_Trains uses the dedicated `BP_FakeGodRayTrains` actor and `M_LF_TrainsSunbreaks` sun light function. Fourteen sparse lavender shafts sit under `Lighting/FakeGodRays_Trains` in the Outliner. Their cooler tint contrasts with the map's yellow and green terrain. Duplicate an actor to add another independently controlled shaft.

The reference image informed the soft, broken ground illumination. Following the requested correction, the map is predominantly sunlit with smaller, fewer, lighter shaded areas. The final sampled mask has median transmission 0.944; 13.5% of sampled terrain is below 0.65 transmission. These are direct sunlight multipliers, not final pixel brightness or total ambient illumination.

## Assets and controls

- `/Game/RTS_Survival/SpecialEffects/Atmosphere/BP_FakeGodRayTrains`
- `/Game/RTS_Survival/SpecialEffects/Atmosphere/BP_TrainsSunbreakDrift` — automatically binds the controls and shafts during construction.
- `/Game/RTS_Survival/SpecialEffects/Atmosphere/M_FakeGodRay_Trains`
- `/Game/RTS_Survival/Materials/Sky/M_LF_TrainsSunbreaks`
- `/Game/RTS_Survival/Materials/Sky/MF_TrainsSunbreakField`
- `/Game/RTS_Survival/Materials/Sky/MPC_TrainsSunbreaks`
- `/Game/RTS_Survival/Materials/Sky/M_TrainsSunbreakProbe` — editor authoring aid, never placed in the map.

The sun and each shaft's current ground anchor sample `MF_TrainsSunbreakField`. World-space smooth noise creates the cloud openings; a filtered texture adds subtle broad variation. `Trains_SunbreakDrift` advances a shared `CloudDriftOffset` vector and physically carries the fourteen shafts with those openings. All cloud layers use that same displacement. Shaft visibility fades between transmission 0.60 and 0.88. The fake shafts supply visual glow; the existing sun supplies terrain illumination.

Changing speed changes the rate of movement without jumping the current cloud position. The controller runs in both gameplay and the realtime editor viewport. At map boundaries, shafts fade out, recycle to the opposite side and fade back in where the moving mask permits. Landscape collision samples keep their bases grounded. The per-actor `RayFade` control remains independent of this boundary fade.

Edit `MPC_TrainsSunbreaks` to tune the shared lighting:

| Control | Final value | Use |
| --- | --- | --- |
| SunbreakWorldSize | 11000 cm | Lower values make individual cloud shapes smaller. |
| CloudCoverPower | 0.62 | Lower values reduce shaded coverage. |
| SunbreakContrast | 1.35 | Lower values soften light/shade transitions. |
| ShadeTransmission | 0.46 | Raise to lighten the darkest cloud shade. |
| SunTransmission | 1.0 | Fully open direct sunlight. |
| RayDetailWorldSize | 4800 cm | Smaller variation inside the openings. |
| RayDetailStrength / RayDetailFloor | 0.40 / 0.78 | Gentle variation within sunlit pools. |
| RayDetailDrift / CloudDriftScale | 4.0 / 0.32 | Shared velocity controls. At RayDetailDrift 4, CloudDriftScale 0.32 is 32 cm/s; 32 is 3200 cm/s. Zero stops movement; negative values reverse it. |
| LegacyWeight | 0.12 | Subtle contribution of the filtered noise texture. |

`RayColor`, `EmissiveStrength`, `RayOpacity`, `BeamLength`, `BeamWidth`, and the existing wisp/fade controls remain editable per actor. The reusable default tint is linear RGB (0.58, 0.40, 1.0). Placed shafts vary slightly in color and dimensions, with length 5000–6140 cm and width 1500–2040 cm.

`Trains_SunbreakDrift` uses `BP_TrainsSunbreakDrift`, derived from native `ATrainsSunbreakDrift`. Its construction script binds the collection and finds the dedicated ray class once; there is no actor search on Tick. Its Details panel exposes the travel bounds, wind direction, speed multiplier, terrain sample interval and boundary fade width. Use `Reset Sunbreak Drift` to return to the authored anchors and zero cloud displacement. Reconstruct the controller after adding shafts to refresh its ray list. Realtime viewport playback must be enabled to preview continuous motion.

## Verification and recovery

All seven lighting assets passed Unreal data validation. Both Blueprints compiled without errors or warnings. Both rendering materials passed shader validation; topology-only checks still flag the world-position nodes because that checker does not traverse material-function inputs. Native engine connections and the rendered probe confirm those inputs are connected.

Verified fourteen separate dynamic material instances, disabled actor ticking, three collision-free non-shadow-casting planes per shaft, and successful fade/brightness setter calls with original values restored. The closest pair of ground anchors is 11388.9 cm apart. Setting both shared sunlight transmission controls to zero extinguished the rendered probe, and restoring the controls recovered the field. Checks ran in the editor; PIE and GPU profiling were not run.

The original first-mission function, Blueprint and ray material hashes are unchanged. No sun color/intensity, atmospheric fog or exposure edits were made. The sun's light-function fade distance is 150000 cm so the pattern remains visible at RTS camera heights.

The continuous-drift checks verified fourteen grounded rays, a maximum ray/offset alignment error below 0.002 cm, 100× velocity between drift scales 0.32 and 32, no position jump when changing speed, stopping and reverse movement. The rendered mask changed at fixed world positions but matched exactly when the probe followed the shared displacement. See `TrainsDriftVerification.json`. These checks ran in the editor. Play teardown encountered a crash in `ParachuteDescentComponent::EndPlay`, so a complete Play-mode test remains unverified.

The missing `TrainsSunbreakDrift.h` and `.cpp` build errors were fixed by restoring both source files. A complete DebugGame editor build compiled and linked successfully.

The saved recovery map is `D:/UE5Projects/RTS_Survival/Saved/CodexBackups/TrainsLighting_20261025/Ger_Trains_BeforeLighting.umap`.

`Trains_Final.png` shows the corrected lighting. `Trains_Before.png` records the original lighting from a different viewport position. `TrainsRayPlacements.json` records placement-time coordinates and brightness samples; cloud motion and later artistic tuning naturally change those samples. `TrainsLightingVerification.json` records the final numerical checks.
