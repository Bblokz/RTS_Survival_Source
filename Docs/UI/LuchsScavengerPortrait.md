# Luchs Scavenger portrait: the ride ("Rütteln")

`ERTSPortraitTypes::GermanLuchsScavenger` (speaker name "Luchs Commander") is a speaker portrait for the German light
panzer. A Scavenger stands in the open commander hatch of a Panzer II Luchs going flat out, seen head-on from above
the turret front. He wears the squad's hood and filter mask, with a radio headset over the hood and goggles pushed up.

The Luchs is the game's own hull, turret and 2 cm weapon system, with scrap welded on and a jerry can and a crate
lashed to the deck. Grit streaks past him (never across his face), torn rags stream off his hood, the scrub on the
steppe smears by, and dust trails behind.

While a line plays:
- Tank and crewman jolt over random bumps.
- He nods on stressed syllables.
- The rags flutter.
- The steppe and the dust behind him stream away into the distance.
- Each syllable puffs vapour from the two cheek filters of his mask, and the head wind smears it toward the rags. Slow
  breaths keep a faint puff going between words. This is how the Scavenger squad portrait "talks" too.

Picked 2026-11-14: "Rütteln" (the ride) over "Fahrtwind" (rags whip, grit streaks), then "Filteratem" (vapour from
the mask filters) was added on top the same day. Preview page: `Saved/LightTankPortrait/fahrtwind_talk.html`. The setup itself was picked from four rough light-panzer setups
(`CommanderPortraits/Tools/lighttank_crew_portrait.py`).

The rhythm is time-driven, not synced to the voice audio, using the same talking envelope as the other speaker
portraits. Nothing changes speed with the voice (that makes the phase jump once the material clock is large).

## Code

- `RTSPortraitTypes.h`: `GermanLuchsScavenger` appended at the end (existing values keep their numbers).
- `W_Portrait.cpp`: `SpeakerNames[GermanLuchsScavenger] = "Luchs Commander"`.

## Assets

All in `/Game/RTS_Survival/Textures/GameUI/LuchsScavengerPortrait/`:

| Asset | What it is |
| --- | --- |
| `T_Portrait_LuchsScavenger` | Blender render, 1024 x 1350 (the portrait box aspect 176 x 232). UI group, no mips, never stream. |
| `T_Portrait_LuchsScavenger_Mask` | Mask: R = rags, G = follows the head (head bone weight, headset, goggles), B = background (everything that is not the crewman or the tank). sRGB off, BC1 masks. |
| `M_UI_LuchsScavengerRide` | UI material, Custom node `LuchsScavengerRide`. Time ignores pause (period 600 s). Opacity is `VertexColor.a` only (widget fade-in). |
| `SB_Portrait_LuchsScavenger` | Slate brush drawing the material; `W_BP_Portrait.PortraitBrushes[GermanLuchsScavenger]` points here. |

How the motion is built:
- **Foreground** (crewman and tank, not B): re-sampled through the jolt, then the head nod about the neck (G), then
  the rag flutter, a wave along the rag axis (R).
- **Filter breath** (drawn over both): four puffs per filter. Each is born on the talking envelope (or a slow 0.25 Hz
  breath), drifts along the rag axis while it grows and fades, and is broken up by noise. The filter points follow the
  jolt and the nod.
- **Background** (B): the render flows toward a vanishing point behind him, along the projected travel direction. It is
  blurred along the flow, and two phases are cross-faded so the scroll never jumps. Samples that would land on the
  crewman or the tank are rejected.

## Material parameters

| Parameter | Default | Effect |
| --- | --- | --- |
| `Jolt` | 1.0 | Scale of the bumps (0 = smooth ride). |
| `Nod` | 1.0 | Scale of the head nod. |
| `Flow` | 1.0 | Scale of the background flow and blur (0 = still background). |
| `Flutter` | 1.0 | Scale of the rag flutter. |
| `Breath` | 1.0 | Scale of the filter vapour (0 = no vapour). |
| `Voice` | 1.0 | Weight of the speech-driven nods, jolts and puffs. |
| `NeckVP` | fitted | Neck (nod pivot) xy, vanishing point zw, in height units `p = (u * Aspect, v)`. |
| `Rag` | fitted | Rag root xy, rag tip zw. |
| `Filters` | fitted | Left filter xy, right filter zw (from `Fahrtwind_anchors.json`). |
| `Aspect` | 0.7585 | Texture width / height. |
| `TimeOffset` | 0 | Only for captures. |

## Rebuilding

- Render: `CommanderPortraits/Tools/lighttank_fahrtwind_portrait.py -- --passes` in Blender 5 (~2 min on a free GPU).
  It execs `lighttank_crew_portrait.py` (`LT_IMPORT_ONLY`), which in turn execs `tiger_crew_portrait.py` for the
  interior kit. The Luchs meshes are in `CommanderPortraits/Assets/Luchs`:
  - turret at hull (0.1, 0, 1.2);
  - weapon system 0.216 above the turret casing;
  - commander hatch at turret-local (-0.4, 0.43), cut open in a copy of the turret mesh.
  Outputs: `Renders/LightTankA/Fahrtwind.png`, `Fahrtwind_Mask.png` (B is written as 1 - foreground alpha) and
  `Fahrtwind_anchors.json`.
- HLSL: `Source/Tools/LuchsScavengerPortrait/luchs_ride.hlsl`.
- Build: `Source/Tools/LuchsScavengerPortrait/build_luchs_ride.py` through UECP `execute_python`
  (`run('texture')`, `run('material')`, `run('point')`; `point` needs an editor built with `GermanLuchsScavenger`).
  `capture()` writes PNG frames to `Saved/LightTankPortrait`.
- Headless rebuild + on/off capture (no shared editor needed):
  `Source/Tools/LuchsScavengerPortrait/luchs_breath_build_headless.py` in a throwaway editor (launch line in the file).
- Live test: `Source/Tools/LuchsScavengerPortrait/luchs_ride_portrait_test.py` in a second Development editor, as in
  `TigerLoaderPortrait.md`.

## Verified (2026-11-14)

- Material validates. Canvas capture with Jolt, Nod, Flow and Flutter at 0 matches the render (mean difference 0.51 /
  255, brightness 83.4 vs 83.3). In a 40-frame sweep (0.1 s apart) the head region changes ~6 / 255 per frame, the
  body and tank ~2.6, the background ~0.9.
- Development and DebugGame editors rebuilt with the enum (20:03 / 20:04). `W_BP_Portrait` saved with the brush and
  the speaker name.
- Live widget on `RTS_ToCopySmall` through `UPlayerPortraitManager::PlayPortrait(GermanLuchsScavenger, ...)` in a second
  Development editor: fade-in, full-resolution portrait, speaker name "LUCHS COMMANDER" with the subtitle. After the
  fade-in, shots 0.5 s apart change the head 1 to 7 / 255, the tank 0.3 to 2.8, the background 0.3 to 1.2. Shots:
  `Saved/LightTankPortrait/live_sequence.png`, `live_name.png`.
- Not run: a mission line that uses the portrait (the test borrowed a long hazmat line).
- Filter breath (2026-11-14, `luchs_breath_build_headless.py` on `/Engine/Maps/Entry`): material compiles; all motion
  and Breath at 0 still matches the render (0.49 / 255). Breath on vs off at the same clock over 4 s: vapour only in a
  box from the filters up and right toward the rags, up to ~22 000 px of 528 x 696 at full talk, fading to ~6 700 px
  in a word gap. Frames: `Saved/LightTankPortrait/breath_sheet.png`. Not re-run live (the brush and widget entry are
  unchanged).
- Gotcha: with another editor loading at the same time (or a game running), a throwaway editor on `RTS_ToCopySmall`
  died out of video memory in its runtime virtual texture. The empty `/Engine/Maps/Entry` map is enough for captures.
