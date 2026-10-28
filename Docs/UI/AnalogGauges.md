# Analog tank instruments (experimental)

WWII tank dashboard instruments in the Frontstahl style: gunmetal bezels, black dials with aged cream paint and
radium pips, domed scratched glass, and needles that are never quite still. Nothing in the HUD uses them yet; they
are a kit to place.

## Where things are

| What | Where |
|---|---|
| Face generator (numerals, ticks, legends, zone arcs, bezel) | `UISkins/_generator/analog_gauges.py` → `UISkins/13_Frontstahl_Gunmetal/AnalogGauges/` (`T_Gauge_*.png`, `gauges.json`, `preview_faces.png`) |
| Editor script (import, materials, instances, capture, showcase) | `UISkins/_generator/ui_analog_gauges.py` |
| Assets | `/Game/RTS_Survival/Textures/GameUI/Skins/_Shared/AnalogGauges/` |
| Showcase widget | `W_AnalogGaugeShowcase` in the same folder; open it in the UMG designer to watch the motion |

Rebuild everything: `python analog_gauges.py`, then in the editor `ui_analog_gauges.build()` and
`ui_analog_gauges.build_showcase()`. `ui_analog_gauges.capture(out_dir, scale, frames, step)` draws the sheet to a
render target and writes PNG frames, stepping `TimeOffset` per frame.

## The instruments

| Instance | Size (units) | Moves by itself |
|---|---|---|
| `MI_Gauge_Tachometer` (U/min ×100, hour counter) | 160 | engine tremor and slow drift; the hour drum rolls |
| `MI_Gauge_Speedometer` (km/h, odometer) | 160 | drift; the km drum rolls, red tenths wheel |
| `MI_Gauge_Compass` (Kurskreisel) | 160 | the card sways around `CardHeading` |
| `MI_Gauge_TurretIndicator` (clock positions, fine dial) | 160 | red turret hand drifts; the fine hand is geared 12:1 |
| `MI_Gauge_Clock` (Borduhr, 24 h ring) | 96 | real hours and minutes, ticking seconds with overshoot |
| `MI_Gauge_OilPressure`, `_Coolant`, `_Fuel`, `_Ammeter` | 96 | tremor / drift / damped kicks (ammeter kicks hardest, fuel sloshes) |
| `MI_Gauge_Voltmeter`, `_OilTemp` | 56 | small tremor and drift |
| `MI_Gauge_Inclinometer` | 112 × 56 | the bubble rocks with the tank and sloshes on bumps |
| `MI_Lamp_Red` / `_Amber` / `_Green` / `_Cyan` / `_RedSmall` | 40 / 28 | blink / pulse / faulty flicker / double flash / steady |

Images must be drawn at the instance's aspect (square, or 2:1 for the inclinometer).

## Materials

- **`M_UI_AnalogGauge`** (one master for every round gauge):
  - face texture under a domed glass (`GlassRadius`, `Refraction`);
  - up to three needles. Each needle has vector parameters `Needle<A|B|C><Geo|Shape|Sweep|Motion|Color>`:
    - Geo = pivot xy, length, tail;
    - Shape = base width, tip width, hub radius, style (0 sword, 1 clock hand);
    - Sweep = start and end angle (0 = up, clockwise), mode, kick;
    - Motion = tremor amount and Hz, drift amount and Hz;
    - Color alpha = enabled.

    Modes: 0 value, 1/2/3 clock hour/minute/second, 4 geared to needle A (ratio in sweep end), 5 fixed.
  - `DrumEnabled`, `DrumRect`, `DrumDigits`, `DrumValue`, `DrumRate`, `DrumRedLast`: rolling counter;
  - `CardEnabled`, `CardRadius`, `CardHeading`, `CardSway(Hz)`: turning compass card; `OverlayEnabled`: fixed marks above it;
  - glass: `CapIntensity`, `StreakIntensity`, `StreakOffset`, `ReflectionDrift`, `RimIntensity`, `GlassTint`,
    `ScratchIntensity/CellSize/Coverage/Seed`, `DustIntensity`; `DialLight` is a faint flickering instrument lamp.
- **`M_UI_IndicatorLamp`**: knurled bezel and cut-glass jewel. `BlinkMode` 0 steady, 1 blink (`BlinkHz`, `Duty`),
  2 pulse, 3 double flash, 4 faulty flicker; `Power` 0..1 switches it; `MinOn` keeps a glow when off.
- **`M_UI_AnalogLevel`**: inclinometer bubble, `Value` −1..1 (0 = level), `Sway`, `SwayHz`, `Kick`.

All three take `TimeOffset` (de-sync copies of the same instrument) and use a pause-ignoring Time, so the
instruments keep moving on a paused game.

## On the HUD: indicator lamps

- **Idle lamps:** each idle unit button (`UW_IdleUnitButton`) builds a lamp into its free top-right corner, against
  the bezel and reaching `IdleLampOverhang` units past the edge. It blinks while something of its type is idle
  (`MI_Lamp_IdleHarvester` red, `MI_Lamp_IdleNomadTruck` green, 0.8 Hz, out of phase) and is a dark jewel otherwise.
- **Power deficit lamp:** `UMainGameUI::SetupPowerDeficitLamp` puts a red lamp (`MI_Lamp_PowerDeficit`, 1.6 Hz) in
  the minimap's top-left corner, next to the top of the energy bar, mirroring the full map button. The energy bar
  lights it while demand exceeds supply. `bShowPowerDeficitLampWhenOk` false hides it while the power is fine.
- **Settings:** Project Settings > Game > Indicator Lamps (`UIndicatorLampSettings`): lamp materials, sizes,
  overhang, inset and the fade time. No HUD Blueprint was changed; everything is built at runtime.
- **Code:** `FIndicatorLamp` (`GameUI/IndicatorLamps/`) is the shared helper: an Image with its own dynamic
  instance, `SetIsLit` and a `Tick` that fades `Power` in and out.
- **Capture:** `-UISnapshotLampsOn` (non-shipping) forces every lamp lit and steady (no blink) for offscreen UI
  captures.

## Driving them at runtime

Make a dynamic instance and set `Value` (0..1 along the sweep), `ValueB`, `ValueC`, `CardHeading` (degrees),
`DrumValue`, or a lamp's `Power`/`BlinkMode`. The built-in motion is added around the value, so a fixed value still
looks alive. `ClockOffsetSeconds` sets the clock (seconds since midnight at game start).
