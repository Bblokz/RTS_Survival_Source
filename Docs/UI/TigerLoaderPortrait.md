# Tiger loader portrait: head nod, engine tremor and lamp jitter ("Kopf & Fahrt")

`ERTSPortraitTypes::GermanTigerLoader` (speaker name "Loader") is a speaker portrait of a Tiger I crewman: the loader,
standing in the turret in red battle light with an 8.8 cm Pzgr round held upright, its black nose beside his face. He
wears the black Panzer uniform, a black side cap, a radio headset and a throat microphone. Behind him is the ready rack of
brass case bases; a caged red battle lamp hangs on a short cable from the roof, with cool daylight from the hatch behind
him and gun smoke in the air. The crewman is the German infantry kit (`SK_WWII_GermanSoldier_varA`) with its helmet
removed.

While a line plays:
- His head nods a little on stressed syllables and sways slowly, and he blinks every ~4 s (head, cap and headset move
  about the neck).
- A fine engine tremor runs through the whole frame, with a jolt on stressed words.
- The red lamp jitters on its cable, so its light trembles across his face, the round and the rack.

Picked 2026-11-12 over a slowly swinging lamp and a flickering lamp (preview page:
`Saved/TigerLoaderPortrait/loader_talk.html`). The rhythm is time-driven, not synced to the voice audio, using the same
talking envelope as the other speaker portraits (`MosinPortrait.md`, `VulturePortrait.md`, `ScavengerPortrait.md`).
Nothing changes speed with the voice (that makes the phase jump once the material clock is large).

## Code

- `RTSPortraitTypes.h`: `GermanTigerLoader` appended at the end (existing values keep their numbers).
- `W_Portrait.cpp`: `SpeakerNames[GermanTigerLoader] = "Loader"`.

## Assets

All in `/Game/RTS_Survival/Textures/GameUI/TigerLoaderPortrait/`:

| Asset | What it is |
| --- | --- |
| `T_Portrait_TigerLoader` | Blender render, 1024 x 1350 (the portrait box aspect 176 x 232). UI group, no mips, never stream. |
| `T_Portrait_TigerLoader_SwingL` / `_SwingR` | The same render with the lamp swung -9 / +9 degrees on its cable, the lamp itself hidden from the camera (only its light and its glow in the haze). |
| `T_Portrait_TigerLoader_Mask` | Mask pass: R = the round, G = follows the head (head bone weight, cap, headset), B = the lamp at rest. sRGB off, BC1 masks. |
| `M_UI_TigerLoaderHeadDrive` | UI material, Custom node `TigerLoaderHeadDrive`. Time ignores pause (period 600 s). Opacity is `VertexColor.a` only, for the widget fade-in. |
| `SB_Portrait_TigerLoader` | Slate brush drawing the material; `W_BP_Portrait.PortraitBrushes[GermanTigerLoader]` points here. |

How the lamp works: the base and the matching swung pass are cross-faded by the jitter amount (textures are sRGB, so the
mix happens in linear light). The lamp at rest (mask B, grown by a few pixels) is replaced by the lamp-free background of
that pass. The one lamp sprite is then drawn back from the base, rotated about the cable's roof pivot, so a second
"ghost" lamp never shows.

## Material parameters

| Parameter | Default | Effect |
| --- | --- | --- |
| `Nod` | 1.0 | Scale of the head motion (0 = still). |
| `Tremor` | 1.0 | Scale of the whole-frame engine shake. |
| `Jitter` | 1.0 | Scale of the lamp jitter (0 = the lamp hangs still). |
| `Blink` | 1.0 | 0 turns blinks off. |
| `Voice` | 1.0 | Weight of the speech-driven nods and jolts (0 = idle sway and tremor only). |
| `LampPivot` / `LampSwing` | fitted | Lamp centre + cable pivot; lamp centre swung left + right. Height units `p = (u * Aspect, v)`. |
| `Neck`, `Eye0`, `Eye1`, `Eye0Axes`, `Eye1Axes` | fitted | Nod pivot and eye centres/axes for the blinks. |
| `Aspect` | 0.7585 | Texture width / height. |
| `TimeOffset` | 0 | Only for captures. |

## Rebuilding

- Render: `CommanderPortraits/Tools/tiger_loader_portrait.py` in Blender 5 (`Blender5.0.exe -b --factory-startup -P
  Tools/tiger_loader_portrait.py -- --passes`; about 2 min on a free GPU, much longer when an Unreal game run holds the
  GPU). It reuses the crewman and interior kit of `tiger_crew_portrait.py`, the rough five-station concept script.
  Outputs go to `Renders/TigerLoader/`: `TigerLoader.png`, `_SwingL`, `_SwingR`, `_LampOff` (unused), `_Mask` and
  `TigerLoader_anchors.json`.
- Lighting gotcha: the lamp has to light the face directly. Above the cap peak, or behind the round, it barely changed
  the face, so the jitter would not show. Check it by comparing the face mean of the base and LampOff renders.
- HLSL: `Source/Tools/TigerLoaderPortrait/tiger_loader_head_drive.hlsl`.
- Build: `Source/Tools/TigerLoaderPortrait/build_tiger_loader.py` through UECP `execute_python`
  (`exec(open(...).read(), ns)`, then `run('texture')`, `run('material')`, `run('point')`). The vectors are read from
  the anchors JSON. `point` needs an editor built with `GermanTigerLoader`. `capture()` writes PNG frames to
  `Saved/TigerLoaderPortrait`; the editor clock is added to `TimeOffset`, so sweep offsets.
- Live test: `Source/Tools/TigerLoaderPortrait/tiger_loader_portrait_test.py` in a second Development editor
  (`-ini:...EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall -ExecCmds="py <file>" -unattended
  -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding`).

## Verified (2026-11-12)

- Material validates. Canvas capture with Nod, Tremor, Jitter and Blink at 0 matches the render (mean difference 0.43 /
  255, brightness 41.65 vs 41.47). In a 40-frame sweep (0.1 s apart) the head region changes ~2.5 / 255 per frame, the
  lamp ~11, the body ~0.6 (the tremor). The lamp moves as one lamp, without a ghost or outline.
- Development and DebugGame editors rebuilt with the enum (17:49 / 17:40). `W_BP_Portrait` saved with
  `PortraitBrushes[GermanTigerLoader]` and `SpeakerNames[GermanTigerLoader] = "Loader"`.
- Live widget on `RTS_ToCopySmall` through `UPlayerPortraitManager::PlayPortrait(GermanTigerLoader, ...)` in a second
  Development editor: fade-in, 176:232 framing, speaker name "LOADER". Over 12 shots 0.5 s apart the head changes 1.2 to
  5.5 / 255, the lamp 4 to 16, the body ~0.6. Shots: `Saved/TigerLoaderPortrait/live_sequence.png`, `live_name.png`.
- Not run: a mission line that uses the portrait (the test borrowed a long hazmat line; the Tiger crew lines are ~1.5 s,
  shorter than the fade-in).
