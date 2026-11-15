# IS-2 driver portrait: the instruments ("Pribory")

`ERTSPortraitTypes::SovietIS2Driver` (speaker name "IS-2 Driver") is a speaker portrait of the driver (mekhvod) of an
IS-2 heavy tank. He sits at the driver's station with both fists on the steering levers. A band of daylight from the
vision slit falls across his eyes. The instrument panel glows amber at his left. The compressed-air start bottles and a
red extinguisher are on the white wall at his right, and a dome lamp burns behind him. The crewman is the Mosin squad's
`SK_RussianSoldier` with the steel helmet removed and a padded tanker's helmet (shlemofon) built around his head.

While a line plays, the engine revs with his voice:
- The amber panel light pulses on his face. This is lighting only: his face changes colour, not position.
- The gauge dials wobble about their centres, which reads as the needles shaking.
- A fine engine tremor runs through the station around him and through his fists on the levers.
- A canteen in its khaki cloth cover hangs on its strap from a hook under the roof, left of his head (his right). It
  swings with the hull at about 1 Hz, wider and with an engine jiggle as the revs rise. The user asked for it on
  2026-11-15 so the motion reads better at the in-game size. It hangs in the band of daylight, so a bright stripe
  crosses it.

**His head never moves.** The user picked option B on 2026-11-15 on one condition: "the face remains stationary and
everything around the driver can move". So there is no nod, and the band of light holds still across his eyes. The
preview's version of option B had a nod, a whole-frame tremor and a band that rocked with the engine; all three are
gone. Preview page: `Saved/IS2Portrait/driver_talk.html`, also published at
https://claude.ai/artifact/PoggxA2qik8U1VNPHmkAG9. The setup itself was picked from four rough IS-2 setups
(`CommanderPortraits/Tools/is2_crew_portrait.py`, concept D).

The rhythm is time-driven, not synced to the voice audio, using the same talking envelope as the other speaker
portraits.

## Code

- `RTSPortraitTypes.h`: `SovietIS2Driver` appended at the end (existing values keep their numbers).
- `W_Portrait.cpp`: `SpeakerNames[SovietIS2Driver] = "IS-2 Driver"`.

## Assets

All in `/Game/RTS_Survival/Textures/GameUI/IS2DriverPortrait/`:

| Asset | What it is |
| --- | --- |
| `T_Portrait_IS2Driver` | Blender render, 1024 x 1350 (the portrait box aspect 176 x 232). UI group, no mips, never stream. |
| `T_Portrait_IS2Driver_PanelOff` | The same render with the instrument panel unlit. Render minus this = the panel's own light. |
| `T_Portrait_IS2Driver_Swing` | The canteen alone (hook, strap, flask), RGBA, premultiplied in linear light by `pack_is2_driver_mask.py`. The other textures leave it out: they are the plate it swings over. |
| `T_Portrait_IS2Driver_Mask` | R = gauge faces and panel lamps. G = tremor weight: 1 on the station and on his fists and the lever grips, 0 on the rest of him, and forced to 0 on his head (feathered). sRGB off, BC1 masks. |
| `M_UI_IS2DriverPribory` | UI material, Custom node `IS2DriverPribory`. Time ignores pause (period 600 s). Opacity is `VertexColor.a` only (widget fade-in). |
| `SB_Portrait_IS2Driver` | Slate brush drawing the material; `W_BP_Portrait.PortraitBrushes[SovietIS2Driver]` points here. |

How the motion is built:
1. The render is re-sampled through the engine tremor, weighted by mask G. On his head the weight is 0, so the head
   pixels are never moved.
2. Where mask R is set, the nearest of the five gauge centres is found and the sample is rotated about it.
3. The panel light (render minus panel-off render) is scaled with the revs. When he is silent the engine idles at 35 %.
4. The canteen layer is sampled rotated about its hook (which rides the tremor), with a small bounce along the strap,
   and drawn over the result.

## Material parameters

| Parameter | Default | Effect |
| --- | --- | --- |
| `Tremor` | 1.0 | Scale of the station and fist tremor (0 = still). |
| `Needles` | 1.0 | Scale of the dial wobble. |
| `Glow` | 1.0 | Scale of the panel light pulse (0 = the render's own light). |
| `Sway` | 1.0 | Scale of the canteen swing (0 = hangs still). |
| `Voice` | 1.0 | Weight of the speech-driven revs (0 = idle engine). |
| `G01`, `G23`, `G4` | fitted | Gauge centres (gauge 0 xy, gauge 1 zw, ...), in height units `p = (u * Aspect, v)`, from `IS2Driver_anchors.json`. Two of them are outside the frame (u > 1); only the visible parts of those dials move. |
| `SwingPivot` | fitted | Canteen hook xy, canteen centre zw (`swing_pivot`, `swing_c` in the anchors). |
| `Aspect` | 0.7585 | Texture width / height. |
| `TimeOffset` | 0 | Only for captures. |

## Rebuilding

- Render: `CommanderPortraits/Tools/is2_driver_portrait.py -- --passes` in Blender 5. It writes `IS2Driver.png`, the
  band and panel passes, `IS2Driver_Mask.png` (R gauges, G head, B hands and grips), `IS2Driver_Driver.png` (his
  silhouette; also `-- --silhouette` on its own), `IS2Driver_Swing.png` (the canteen alone, every other mesh a holdout;
  also `-- --swing` on its own, which merges its anchors into the file) and `IS2Driver_anchors.json` to
  `Renders/IS2Driver`. The canteen is placed with `--swing-at=right,forward,up` in his frame (default 0.22,-0.06,0.03);
  `--with-swing` shows it in the normal renders, for placement checks only.
- Mask: `Source/Tools/IS2DriverPortrait/pack_is2_driver_mask.py` packs `IS2Driver_TalkMask.png` from the mask and the
  silhouette, and premultiplies the canteen layer into `IS2Driver_SwingPremul.png`.
- HLSL: `Source/Tools/IS2DriverPortrait/is2_driver_pribory.hlsl`.
- Build: `Source/Tools/IS2DriverPortrait/build_is2_driver.py`. Its `run()` steps:
  - `assets`: textures, material and brush;
  - `texture` or `material`: only that part;
  - `point`: sets `W_BP_Portrait`, and needs an editor built with `SovietIS2Driver`;
  - `all`: everything.
  `capture()` writes PNG frames to `Saved/IS2Portrait`.
- Headless build + capture: `Source/Tools/IS2DriverPortrait/is2_driver_build_headless.py` in a throwaway editor
  (launch line in the file; the step comes from the `IS2_DRIVER_STEP` environment variable, default `all`).
  - Gotcha: imports and saves tick Slate from their progress dialogs, which re-enters a
    `register_slate_post_tick_callback` tick. The first run built twice and crashed in a nested save. The script now
    leaves its phase before the long call.

## Verified (2026-11-15)

Run in a throwaway DebugGame editor on `/Engine/Maps/Entry`, with the step set to `assets` (second build, with the
canteen):
- The material compiles.
- With Tremor, Needles, Glow and Sway at 0 it matches the render with the canteen at rest: mean difference
  0.41 / 255, brightness 68.2 vs 68.2.

40 frames, 0.1 s apart, change between frames (mean / max, out of 255):

| Run | Head | Canteen area |
| --- | --- | --- |
| Glow 0 (geometry only) | 0.002 / 4.0 | 8.2 |
| Talking, everything on | 1.88 | 8.3 |
| Silent (Voice 0) | 0.52 | 6.9 |

- With Glow 0 the head stays put: the max is bilinear at the edge of the mask.
- When talking, the head's change is the panel light only.
- Frames: `Saved/IS2Portrait/ue_canteen_strip.png`, `ue_talk.gif` and `ue_talk_1to1.gif` (in-game size).

Hook-up (2026-11-15, after the user rebuilt the Development editor at 13:27):
- `build_is2_driver.run('point')` ran in the shared editor through UECP: `W_BP_Portrait.PortraitBrushes[SovietIS2Driver]`
  = `SB_Portrait_IS2Driver`, `SpeakerNames[SovietIS2Driver]` = "IS-2 Driver", saved.
- Live test `Source/Tools/IS2DriverPortrait/is2_driver_portrait_test.py` in a throwaway Development editor on
  `RTS_ToCopySmall`:
  - It reads the brush back from `W_BP_Portrait`, and `UPlayerPortraitManager::PlayPortrait(SovietIS2Driver, ...)` ran
    without an error.
  - The map was broken that day ("M_PlayerHQ is not set on the player controller", an uninitialised
    EnemyStrategicAIComponent) and no HUD came up, so the screenshots show no UI.
  - **The portrait has not been seen live in the widget yet.** Re-run the test once the map works again (it only reads
    `W_BP_Portrait`; `IS2_DRIVER_POINT=1` also points it from there).
