# Reusable short editing: typewriter callouts

`edit_short.py` applies a JSON recipe to an existing MP4. It works independently
of Unreal, so changing words or timings does not require rendering the battle
again. Pillow, ffmpeg and ffprobe must be installed. This workstation uses
`E:/Python11/python.exe`.

```powershell
E:/Python11/python.exe Source/Tools/ShortsEditing/edit_short.py `
  --input path/to/finished-short.mp4 `
  --output path/to/edited-short.mp4 `
  --recipe Source/Tools/ShortsEditing/paratrooper_callouts.json
```

Run from the project folder, or use absolute paths. The output must be a new
file. Keep the original as the editing master so reruns do not add duplicate
captions. An `.edit.json` report records the resolved timings, source checksum
and audio verification.

## Editing approach

1. Start from the finished short, including its faction header and audio.
2. Store editorial text and timings separately in a reusable recipe.
3. Render small transparent animation clips only while each callout is active.
   Letters appear quickly, a cursor tracks the text, and the completed line
   holds before a short slide to the left and fade out, or a left-to-right wipe.
4. Composite the clips above the video. Scale the font, margins and stroke
   with the output resolution; wrap long text once before typing so words do
   not jump between lines. The default placement leaves 11.5% below the text
   for player controls.
5. Encode the picture once at high quality, while copying the original audio
   packets. Check the duration, frame count, dimensions, frame rate and audio
   checksum before accepting the output. Inspect actual frames during entry,
   hold, exit and the empty interval.

## Recipe timing

Use exactly one timing anchor per callout:

- `start_seconds`: an absolute start.
- `after_previous_seconds`: a gap after the preceding callout has **fully disappeared**.
- `start_fraction`: a fraction of the delivered video's duration; `0.5` starts at halfway.
- `end_before_seconds`: places the entire animation so it is fully gone this many seconds before the end.

Relative anchors use the final, trimmed MP4 length, so shorter battle outcomes
keep captions at the midpoint and near the end. The King Tiger vs IS-2 recipe
(`king_tiger_is2_callouts.json`) uses these anchors with four- and three-second
holds. It finishes the closing animation 0.8 seconds before the video ends.

`type_seconds`, `hold_seconds` and `exit_seconds` are separate phases; the hold
starts when the entire sentence is readable. Timings round to the video's
frame grid. Overlapping captions and captions beyond the video are rejected.
Set a callout's `exit_effect` to `wipe` for a stationary left-to-right erasure,
or `slide_fade` for the default exit. `exit_seconds` controls either effect.
An `after_previous_seconds` value of zero starts the next entry as soon as the
preceding exit finishes.

The supplied recipe resolves at both 30 and 60 fps to:

| Text | Types in | Fully readable | Exits | Gone |
| --- | --- | --- | --- | --- |
| paratroopers test... | 0.20–0.60 s | 0.60–2.10 s | 2.10–2.30 s | 2.30 s |
| infantry automatically takes cover | 4.30–4.80 s | 4.80–6.80 s | 6.80–7.00 s | 7.00 s |
| IS-1 / director of forge / HEAT rounds | 9.00–10.00 s | 10.00–12.00 s | 12.00–12.20 s | 12.20 s |
| nothing is scripted here; autonomous targets and cover | 13.20–15.00 s | 15.00–18.00 s | 18.00–18.20 s | 18.20 s |
| Animals / VAT / minimal CPU budget | 26.00–27.20 s | 27.20–30.20 s | wipe 30.20–30.50 s | 30.50 s |
| ... But do look a bit funky | 30.50–31.00 s | 31.00–32.50 s | wipe 32.50–32.80 s | 32.80 s |

Long captions wrap into multiple lines at the same bottom-left anchor.
Their full wording is stored in the recipe. Typing takes a little longer for
these longer sentences; their holds begin only once all
the text has appeared.

Copy the recipe for another short and change its text and timing. Optional
style settings include `font`, `font_size_fraction`, `left_fraction`,
`bottom_fraction`, `max_width_fraction`, `stroke_fraction`,
`exit_slide_fraction` and `cursor_blink_seconds`. Fractions are resolution
relative; the font path is local to the machine doing the editing.

## Automatic use by the Shorts tool

Jobs can opt into the same step with this top-level field:

```json
"EditorialOverlayRecipe": "Source/Tools/ShortsEditing/paratrooper_callouts.json"
```

The path is relative to the project folder; an absolute path also works.
`post_shorts.py` applies this after picture repair and before the contact
sheet. Jobs without this field keep their existing editing workflow.
