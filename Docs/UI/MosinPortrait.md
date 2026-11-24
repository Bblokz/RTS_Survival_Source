# Mosin portrait: head nod and blinks ("Kopf & Augen")

`ERTSPortraitTypes::SovietMosin` (speaker name "Rifleman") is the speaker portrait of the Soviet Mosin rifle squad. It
shows the squad's rifleman (`Rus_Inf_Mosin`, `SK_RussianSoldier` with its own atlas) in an SSh-40 helmet with a knotted
oxblood scarf over his nose and mouth, bandit style, so no mouth ever has to move. His Mosin (`SM_InfWeapon_Mosin`)
is slung behind his shoulder; behind him a burnt village at dusk (standing brick chimneys, a knocked-out tank, smoke).

While a line plays, his head moves a little: a slow sway plus small nods and tilts on the stressed syllables, a few
pixels at most. Head, helmet and the upper scarf move together about the neck; the scarf's lower point and the body stay
still. He blinks at a random moment every ~4 s, sometimes twice. Picked 2026-11-12 over a scarf that puffs over the
mouth and firelight from the burning village.

The rhythm is time-driven, not synced to the voice audio, using the same talking envelope as the other speaker
portraits (`HazmatPortrait.md`, `VulturePortrait.md`, `ScavengerPortrait.md`). Nothing changes speed with the voice
(that makes the phase jump once the material clock is large).

## Code

- `RTSPortraitTypes.h`: `SovietMosin` appended at the end (existing values keep their numbers).
- `W_Portrait.cpp`: `SpeakerNames[SovietMosin] = "Rifleman"`.

## Assets

All in `/Game/RTS_Survival/Textures/GameUI/MosinPortrait/`:

| Asset | What it is |
| --- | --- |
| `T_Portrait_MosinRifleman` | Blender render, 1024 x 1350 (the portrait box aspect 176 x 232). UI group, no mips, never stream. |
| `T_Portrait_MosinRifleman_Mask` | Mask pass from the same camera: R = face scarf, G = how much a pixel follows the head (head bone weight, helmet, scarf). sRGB off, BC1 masks. |
| `M_UI_MosinHeadNod` | UI material: the portrait re-sampled through the nod/tilt + blinks (Custom node `MosinHeadNod`). Time ignores pause (period 600 s). Opacity is `VertexColor.a` only, for the widget fade-in. |
| `SB_Portrait_MosinRifleman` | Slate brush drawing the material; `W_BP_Portrait.PortraitBrushes[SovietMosin]` points here. |

## Material parameters

| Parameter | Default | Effect |
| --- | --- | --- |
| `Nod` | 1.0 | Scale of all head motion (0 = still). |
| `Blink` | 1.0 | 0 turns blinks off. |
| `Voice` | 1.0 | Weight of the talking nods (0 = slow sway only). |
| `Neck` | fitted | Pivot of the nod (neck_01 projected), height units `p = (u * Aspect, v)`. |
| `Eye0`, `Eye1` / `Eye0Axes`, `Eye1Axes` | fitted | Eye centres; axes = half-width (xy) and upward half-height (zw) vectors. |
| `Aspect` | 0.7585 | Texture width / height. |
| `TimeOffset` | 0 | Only for captures. |

## Rebuilding

- Render: `CommanderPortraits/Tools/mosin_portrait.py` in Blender 5 (`Blender5.0.exe -b --factory-startup -P
  Tools/mosin_portrait.py`, ~30 s). Reuses the commander pipeline (`build_portraits.build()`) with its own row; the kit
  only has a diffuse atlas, so a flat normal and a plain cloth mask stand in (`Assets/RusSoldier`). The scarf is built
  by `face_scarf()` from rays cast at the posed head in the head-bone frame, hanging like a curtain from the nose so it
  never tucks into the face. Outputs `Renders/Mosin/Mosin.png`, `Mosin_Mask.png`, `Mosin_anchors.json` (eyes, mouth,
  neck), plus `Mosin_FireOn.png` for the unpicked firelight option.
- Preview page with all three options: `Saved/MosinPortrait/build_mosin_page.py` + `mosin_talk_template.html`; it also
  writes the PNGs the build script imports (`Saved/MosinPortrait/T_Portrait_MosinRifleman*.png`).
- HLSL: `Source/Tools/MosinPortrait/mosin_head_nod.hlsl`. Eyelid colour = 60 % lit skin below the eye + 40 % skin
  above (the skin above sits in the helmet brim's shadow, darker than the open eye: sampling only there read as holes).
- Build: `Source/Tools/MosinPortrait/build_mosin_head_nod.py` through UECP `execute_python`
  (`exec(open(...).read(), ns)`, then `run('texture')`, `run('material')`, `run('point')`; `point` needs an editor built
  with `SovietMosin`). `capture()` writes PNG frames to `Saved/MosinPortrait`; the editor clock is added to
  `TimeOffset`, so sweep offsets.

## Verified (2026-11-12)

- Material validates. Canvas sweep (80 frames, 0.1 s apart): the body region below the head matches the texture (mean
  difference 0.57 / 255, brightness 63.19 vs 63.19); head pixels move up to ~8 / 255 on average between frames; blinks
  close both eyes with lid-coloured skin.
- `W_BP_Portrait`: `PortraitBrushes[SovietMosin]` and `SpeakerNames[SovietMosin]` saved 12:47 after the Development
  rebuild with the enum (the first save hit error 32: a headless `-game` run of another session had the package open).
- Live widget on `RTS_ToCopySmall` through `UPlayerPortraitManager::PlayPortrait(SovietMosin, ...)` in a second
  Development editor (UECP disabled): fade-in, 176:232 framing, speaker name "RIFLEMAN"; between shots 0.5 s apart the
  head region changes (up to 1.4 / 255 mean) while the body stays at noise level (~0.2). Shots:
  `Saved/MosinPortrait/live_sequence.png`, `live_name.png`.
- Not run: a mission line that uses the portrait (the test borrowed a hazmat line). The DebugGame DLL from 12:11 may
  predate the enum; rebuild DebugGame before using `SovietMosin` in DebugGame runs.
