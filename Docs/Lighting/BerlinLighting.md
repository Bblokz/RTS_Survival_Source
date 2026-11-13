# Ger_Berlin lighting

## Sun light function (2026-11-13)

The sun of `Ger_Berlin` uses `/Game/RTS_Survival/Materials/Sky/M_LF_BerlinSmokePall`: soft cloud and smoke shadows that
drift slowly east along the lanes, plus a fixed term per district. The map has no `ExponentialHeightFog` actor (the
owner's rule: no engine default fog on this map); `step_lighting` and `step_sun_light_function` both remove one if a
rebuild from the template brings it back.

Assets (same construction as the First Mission and Trains sunbreaks):

- `M_LF_BerlinSmokePall`: light function, absolute world position -> field -> transmission. Rebuilt from scratch by
  the script on every run.
- `MF_BerlinSmokePallField`: one Custom node. Output R = direct sunlight multiplier, G = opening, B = fine variation,
  so a later effect (shafts, ground haze) can sample the same field at a world position.
- `MPC_BerlinSmokePall`: the controls. The parameter names are the First Mission ones (the collection is a copy); their
  Berlin meaning:

| Parameter | Value | Berlin meaning |
| --- | --- | --- |
| SunbreakWorldSize | 9000 cm | Size of the largest cloud shapes; two finer octaves at about 1/2 and 1/4. |
| CloudCoverPower | 0.54 | Cover threshold on the 0..1 noise: lower puts more of the city in shade. |
| SunbreakContrast | 1.0 | Edge hardness. 1 is soft clouds; raise it for harder smoke shadows. |
| ShadeTransmission / SunTransmission | 0.5 / 1.0 | Sunlight in full shade and in the open. Keep the floor: units must stay readable. |
| RayDetailDrift x CloudDriftScale | 60 x 1.0 | Wind speed in cm/s. Zero stops the drift. |
| RayDetailWorldSize / Strength / Floor | 3500 / 0.3 / 0.85 | Faint variation inside the open stretches. |
| LegacyWeight | 1.0 | Strength of the district term; 0 lights the whole city alike. |

The noise is procedural (three octaves of smooth value noise in the Custom node), not the project's
`T_RTS_PackedNoise`: that texture is veined, so thresholded it gave marbling and read small it gave hard flecks. The
owner asked for the noise to look like softer clouds (2026-11-13).

District term (fixed in the world, in the Custom code; +X north, +Y east): a standing pall over the crater (full
within 45 m, gone at 150 m) and over the east (y from 140 m to 340 m), which lowers the cover threshold and dims even
the openings by up to 22 %; the dead park in the west and the north along the Spree are clearer. The wind blows from
the west, a little to the north (0.22, 1.0).

Sun settings set by `step_sun_light_function`: light function fade distance 150000 cm (the pattern must hold at RTS
camera heights), disabled brightness 1.0. Colour and intensity are unchanged (`step_lighting`).

### Rebuild

```
UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=Source/Tools/BerlinMap/lighting/berlin_light_function_build.py -unattended -nosplash -nullrhi -DisablePlugins=Fab,BpGeneratorUltimate
```

then `berlin_map_build.step_sun_light_function()` on the map. `BERLIN_LF_DEBUG=1` in the environment builds an
exaggerated version (black shade, a gale) for looking at the pattern; rebuild without it afterwards. An editor that
has the three assets loaded locks their files: run the script inside that editor (`import
berlin_light_function_build as b; b.run()`), not from a second process.

### Checked

Throwaway editor on Ger_Berlin, shots in `Saved/Berlin/lighting/` (`real_h14000`, `real_h30000`, `real_mitte`,
`real_east`; `it_*` are the exaggerated runs; `fm_*` the First Mission function on the same sun for comparison).
The pattern is soft and cloud-shaped from 140 m and 300 m up, and two frames 88 s apart differ (mean 25 of 255 grey
levels at 320 x 180), so it drifts. Not checked: a game run (fog of war, game camera), GPU cost, and whether the
light function is applied beyond the shadow distance (from 920 m up no pattern showed with any light function).

### Traps met

- Repointing the copied First Mission light function at the Berlin field dropped its pin links: the sun went black.
  The script now rebuilds the material and wires it explicitly.
- `post_edit_change` does not exist on these Python objects; use the setters.

## Still to do (atmosphere per district)

Low smoke and dust through Mitte, a faint green glow and heat shimmer at the crater, fires with flickering light in
the east. None of it may use the engine height fog or volumetric fog.
