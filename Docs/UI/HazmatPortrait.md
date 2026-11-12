# Soviet hazmat portrait: red eyes ("Ember Breath")

The SovietHazmat speaker portrait (`ERTSPortraitTypes::SovietHazmat`, used for the hazmat cinematic and mission lines)
moves while a line plays: a red glow fills both gas-mask lenses, slowly swells like breathing through the respirator and
brightens with a speech-like rhythm. Light spills onto the brass lens rims, the mask and the hood. Nothing on the mask
itself animates.

The rhythm is time-driven, not synced to the voice audio. It uses the same talking envelope as the German advisor's
oscillograph portrait (`CommanderPortraits/Tools/announcer_trace.py`), so both portraits talk alike.

## Assets

All in `/Game/RTS_Survival/Textures/GameUI/HazmatPortrait/`:

| Asset | What it is |
| --- | --- |
| `T_Portrait_HazmatEmber` | The old hazmat thumbnail's 2048 source, cropped to the portrait box aspect 176 x 232 (source x 198..1752), 971 x 1280. The old brush showed a 256 px copy squeezed into the box. |
| `M_UI_HazmatEmberEyes` | UI material: the portrait plus one Custom node (`HazmatEmberEyes`). Time ignores pause (period 600 s). Opacity is `VertexColor.a` only, for the widget fade-in. |
| `SB_Portrait_HazmatEmber` | Slate brush drawing the material. `W_BP_Portrait.PortraitBrushes[SovietHazmat]` points here. |

To revert, point `PortraitBrushes[SovietHazmat]` back at `/Game/Thumbnails/Portrait_RusHazmat` (left untouched).

## Material parameters

| Parameter | Default | Effect |
| --- | --- | --- |
| `Intensity` | 1.0 | Overall glow strength. |
| `BreathHz` | 0.22 | Breaths per second (about one every 4.5 s). |
| `Spill` | 0.32 | Red light on the mask and hood. |
| `SpillRadius` | 0.07 | Spill reach, in portrait-height units. |
| `Voice` | 1.0 | Weight of the talking rhythm (0 = breath only). |
| `LensL*` / `LensR*` | fitted | Lens ellipses: centre, major and minor semi-axis vectors in height units (`p = (u * Aspect, v)`). |
| `TimeOffset` | 0 | Only for captures. |

## Rebuilding

- HLSL: `Source/Tools/HazmatPortrait/hazmat_ember_eyes.hlsl`. The glow is added in display space to the sRGB-encoded
  portrait and converted back to linear, so the portrait keeps its exact brightness.
- Build: `Source/Tools/HazmatPortrait/build_hazmat_ember.py` through UECP `execute_python`
  (`exec(open(...).read(), ns)`, then `run('texture')`, `run('material')`, `run('point')`; `capture()` writes PNG
  frames to `Saved/HazmatPortrait`). Lens ellipses are in `LENSES` in that script.
- Live check: `Source/Tools/HazmatPortrait/hazmat_portrait_test.py` in a throwaway DebugGame editor (see its docstring)
  plays `S_Hazmat_DepotReinforcements` with the hazmat portrait and takes UI screenshots spaced by game time (each
  `shot showui` stalls the game for seconds, and wall-clock spacing captured identical frames).

## Verified (2026-11-10)

- Canvas capture: portrait pixels away from the eyes equal the source exactly; glow sits inside both lenses.
- Live widget on `RTS_ToCopySmall` through `UPlayerPortraitManager::PlayPortrait(SovietHazmat, ...)`: fade-in works,
  correct 176:232 framing, lens glow varies between frames 0.35 s apart.
- Not run: the takeover cinematic itself. It uses the same `W_BP_Portrait` brush map, so it should show the same
  portrait, but it was not watched end to end.
