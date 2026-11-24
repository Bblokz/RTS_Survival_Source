# Vulture portrait: breath fog ("Atemhauch")

The GermanVultures speaker portrait (`ERTSPortraitTypes::GermanVultures`, speaker name "Vulture") shows the Vulture squad's
yellow hazmat soldier. While a line plays, the inside of his visor fogs up with his breath: each word adds mist that
spreads out from the mouth, the reflections go soft under it and fine droplets catch the light. In pauses the mist
clears from the edges. A faint puff on every breath (about one every 4 s) keeps the portrait from looking frozen. Nothing
glows; it was picked (2026-11-12) over an inner chin lamp and a chest radio "magic eye" as the option without eye lights,
unlike the Soviet hazmat (`HazmatPortrait.md`).

The rhythm is time-driven, not synced to the voice audio. It uses the same talking envelope as the advisor's oscillograph
portrait and the Soviet hazmat's eyes (`CommanderPortraits/Tools/announcer_trace.py`), so all speaker portraits talk alike.

Before this, `W_BP_Portrait.PortraitBrushes` had no GermanVultures entry.

## Assets

All in `/Game/RTS_Survival/Textures/GameUI/VulturePortrait/`:

| Asset | What it is |
| --- | --- |
| `T_Portrait_VultureFog` | Blender render, 1024 x 1350 (the portrait box aspect 176 x 232). UI group, no mips, never stream. |
| `T_Portrait_VultureFog_VisorMask` | Visor mask from the same camera (white = visor glass). Grayscale, sRGB off. |
| `M_UI_VultureFogVisor` | UI material: portrait + one Custom node (`VultureFogVisor`). Time ignores pause (period 600 s). Opacity is `VertexColor.a` only, for the widget fade-in. |
| `SB_Portrait_VultureFog` | Slate brush drawing the material. `W_BP_Portrait.PortraitBrushes[GermanVultures]` points here. |

To remove it, delete the GermanVultures entry from `PortraitBrushes`.

## Material parameters

| Parameter | Default | Effect |
| --- | --- | --- |
| `Strength` | 1.0 | Fog opacity scale. |
| `Spread` | 1.0 | How far the fog reaches across the visor. |
| `Voice` | 1.0 | Weight of the talking rhythm (0 = breath puffs only). |
| `Mouth` | (0.536, 0.526) | Where the fog starts, in height units `p = (u * Aspect, v)` (20 % up the visor). |
| `VisH` | 0.356 | Visor height in height units (scales the spread). |
| `Aspect` | 0.7585 | Texture width / height. |
| `Portrait`, `VisorMask` | textures | The two textures above. |
| `TimeOffset` | 0 | Only for captures. |

## Rebuilding

- Render: `CommanderPortraits/Tools/vulture_portrait.py` in Blender 5
  (`Blender5.0.exe -b --factory-startup -P Tools/vulture_portrait.py`, ~30 s). It reuses the commander portrait pipeline
  (`build_portraits.build()`) with its own row and the `deadzone` backdrop (dead trees, ruined works, circling vultures).
  Writes `Renders/Vulture/Vulture.png`, `Vulture_VisorMask.png` and `Vulture_anchors.json` (visor and mouth positions);
  the other passes (`_LampOn`, `_Radio*`) belong to the options that were not picked.
  Copy the two PNGs into `Saved/VulturePortrait/` as `T_Portrait_VultureFog.png` / `T_Portrait_VultureFog_VisorMask.png`.
- HLSL: `Source/Tools/VulturePortrait/vulture_fog_visor.hlsl`. The fog is mixed in display space (every texel is encoded
  with pow 1/2.2, the result decoded with pow 2.2), so pixels outside the visor come out exactly as the texture.
- Build: `Source/Tools/VulturePortrait/build_vulture_fog.py` through UECP `execute_python`
  (`exec(open(...).read(), ns)`, then `run('texture')`, `run('material')`, `run('point')`). `capture()` writes PNG
  frames to `Saved/VulturePortrait`. If the visor anchors change, update `VISOR_LOW_UV` / `VISOR_V` there.
- Preview page (all three options, live WebGL): `CommanderPortraits/Tools/build_vulture_page.py` with
  `vulture_talk_template.html`.
- Live check: `Source/Tools/VulturePortrait/vulture_portrait_test.py` in a throwaway DebugGame editor (see its docstring).

## Verified (2026-11-12)

- Material validates (no compile errors). Canvas capture: away from the visor the output matches the source texture
  (mean difference 0.35 / 255, overall brightness 76.85 vs 76.99); an 8 s sweep shows the visor fogging, clearing in a
  pause and fogging again.
- Live widget on `RTS_ToCopySmall` through `UPlayerPortraitManager::PlayPortrait(GermanVultures, ...)` in a throwaway
  DebugGame editor: fade-in works, correct 176:232 framing, speaker name "Vulture", visor fog changes between shots
  (`Saved/VulturePortrait/live_sequence.png`). During steady talking the fog stays near full, so later shots 0.35 s apart
  differ only slightly.
- Not run: a mission line that actually uses the GermanVultures portrait (the test borrowed a hazmat line).
