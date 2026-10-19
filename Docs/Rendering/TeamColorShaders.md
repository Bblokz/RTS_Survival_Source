# Team colour shaders for vehicles (test, 2026-10-19)

Goal: give every player's vehicles a team colour without losing the hand-painted camo. Three shader types were built
and compared on the Panzer III M (desert 3-tone camo) and the King Tiger (ambush 3-tone camo). **Nothing shared was
changed**: `MM_Ger_PostApoTank_NoBlend`, `MI_PZIII_Hull_DesertAPO` and `M_TigerII_Hull` are untouched; everything lives in
`/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/MasterMaterials/TeamColorTest/`.

## Assets

| Asset | What it is |
|---|---|
| `MF_TeamColor_CamoSwap` | repaints one camo band (picked with `TC_KeyColor`, default red-brown) in the team colour |
| `MF_TeamColor_PaintTint` | glazes all painted plates toward the team colour; bare metal untouched |
| `MF_TeamColor_TopologyAccent` | paints the steep/side plates (turret walls, superstructure sides); flat decks and the turret roof keep their camo |
| `MM_Ger_PostApoTank_TC_<Type>` | duplicate of the tank master with the function spliced in |
| `MI_PZIII_Hull_DesertAPO_TC_<Type>` | copy of the Panzer III hull instance on that test master |
| `M_TigerII_Hull_TC_<Type>` | copy of the King Tiger hull/turret material (a standalone material, not on the master) with the function spliced in |
| `MI_TigerII_Hull_TC_CamoSwap_GreenKey` | King Tiger CamoSwap with `TC_KeyColor` set to the camo's dark green |

## Where the function sits in the master

`TankDiffuse` (hull camo, UV x2) -> **MF_TeamColor_*** -> named reroute `DiffuseTankHull` -> `MF_Apply4xSquareStamp`
(icons / crosses) -> lerp with the shared tank atlas (`TankUVs`) -> BaseColor.

Inserting before the stamp keeps unit markings crisp, and the atlas parts (fuel cans, antenna, tools) never get the
team colour. Integrating into the real master later is: drop the function in this same spot, nothing else.
`M_TigerII_Hull` has the same structure (`Hull_low_BaseColor` -> `MF_Apply4xSquareStamp.InDiffuse`); vehicles with their
own materials like it need the function added per material.

## How the colour is applied (all three types)

- Blending happens in gamma space (`pow(x, 1/2.2)`): the camo is dark (median ~0.1-0.2 sRGB), linear maths only darkens.
- The camo's value is **overlaid** with the team colour: wear, grime, panel shading and the stamp all survive, only the
  hue/chroma changes. `TC_Lift` pulls the value toward mid grey so dark camo still reads as the colour.
- `PaintMask` = chroma of the camo (`max - min` in sRGB, smoothstep `TC_PaintMin..TC_PaintMax`): painted plates are
  coloured, bare steel, grilles, tools and soot are grey and never get the team colour. Checked on the Panzer III
  texture: grilles and tool clips fall out cleanly.
- CamoSwap keys one band by **chromaticity** (colour with brightness divided out, `rgb / (r + g + b)` in sRGB): pixels
  whose chromaticity lies within `TC_KeyMin..TC_KeyMax` (0.08..0.15) of `TC_KeyColor` are repainted. Wear and shading
  inside the band stay keyed because brightness is ignored. `TC_KeyColor` is set per camo in a material instance by
  eyedropping the band to swap: default red-brown (sRGB 0.19, 0.085, 0.002); dark green for the King Tiger's ambush camo
  (sRGB 0.07, 0.10, 0.005). The first version keyed only red-brown with `(R - G) / (R + G + B)`; the chromaticity key
  gives the same Panzer III mask (8.4% vs 9.0% of the texture) and works for any band colour.
- TopologyAccent uses the per-pixel world normal: `1 - smoothstep(TC_UpMin, TC_UpMax, PixelNormalWS.z)` colours the
  steep plates, and the red-brown band is left out so the pattern reads through. Normal-map rivets, welds and bevels
  break the colour like chipped paint. It reads geometry, not UVs, so it needs no per-vehicle setup.
  First tried the other way round (colour the up-facing plates): from the RTS camera (about -60 degrees) nearly every
  visible pixel faces up, so it recoloured the whole tank and looked like a stronger PaintTint. Painting the sides
  instead outlines the hull in team colour while the large camo areas on top stay whole.

## How each type behaves on other hull shapes

- **Boxy hulls with vertical sides (Panzer III/IV, StuG):** TopologyAccent gives a clean coloured band around the
  silhouette; CamoSwap and PaintTint behave the same everywhere.
- **Sloped armour (T-34, Panther glacis at roughly 55-60 degrees, N.z about 0.5):** inside the `TC_UpMin..TC_UpMax`
  falloff, so these plates get a partial coat; the more slope, the more camo survives. Tune the window per faction
  master if a sloped tank looks under-coloured.
- **Cast/rounded turrets (T-34/85, IS):** the falloff gives a soft gradient from coloured walls to a camo roof rather
  than a hard edge.
- **Other camos:** CamoSwap needs `TC_KeyColor` set to a band that the camo has and that is visible from above. On the
  King Tiger the default red-brown key only finds a few patches (the visible plates are dunkelgelb and dark green), the
  dark-green key repaints the green stripes and reads clearly. Single-colour camo (Panzergrau, Soviet green) has no
  second band to key; there PaintTint (or CamoSwap keyed on the base colour) is the fallback. PaintTint and
  TopologyAccent need no per-camo setup because they key on paint chroma and geometry.
- **Team colours close to the camo palette (yellow/sand on desert camo):** CamoSwap reads weakly; that is a limit of
  swapping only one band.

## Feeding the team colour per player

`TeamColor` is a vector parameter bound to **custom primitive data 4-7** (RGBA, linear; A = weight, 0 = off). Index 0
is already used by the faction flag (`FRTS_FactionFlags`). Per-primitive data keeps one shared material instance for
every tank (no dynamic instances, batching intact) and different teams on screen at once; a Material Parameter
Collection would only allow one colour per scene. Game code would set it like `FRTS_FactionFlags::SetFlagOnActorAndAllChildren`
does, on every primitive of the vehicle and its turret child actors:
`Primitive->SetCustomPrimitiveDataVector4(4, FVector4(TeamColor.R, TeamColor.G, TeamColor.B, 1.f))`.
Without primitive data the material instance's `TeamColor` value is used (default alpha 0, so tanks look unchanged).

## Testing

`Source/Tools/TeamColor/run_team_color_capture.sh` spawns the Panzer III M offscreen (unit portrait capture,
`Docs/UI/UnitPortraits.md`) once per list entry in `pz3m_team_colors.txt`, swapping the shared hull instance for a
test instance and setting the team colour. New capture-tool switches/fields:

- list entry fields `|Swap=<material path>><material path>|TeamColor=r,g,b[,a]`
- `-UnitPortraitShots=0` takes only the idle picture and spawns no AI controller (the tank otherwise engaged map
  enemies mid-picture: smoke, slewed turret)
- `-UnitPortraitRtsView` adds a steep `RtsCamera` view like the in-game camera
- `-UnitPortraitFixedExposure` uses manual exposure (bias from `EXPOSURE_BIAS`, 2.0 for these runs) so cells are
  exposed alike. The map's light still changes over the ~2 minutes of game time a run takes (the road gets darker
  in some cells), so compare colours within a row more than across rows.

`python make_team_color_sheet.py <captures> <out> [--unit KingTiger] [--variants A,B]` lays the pictures out as shader x
colour sheets per view. `kt_team_colors.txt` does the same for the King Tiger; it starts with `Warmup_` entries because a
test material shown for the first time still compiles its shaders and pictures as the grey default material.
Material edits made while no editor bridge is free (editor closed or in PIE) can run headless:
`UnrealEditor-Cmd <uproject> -run=pythonscript -script=<file> -unattended -nullrhi -DisablePlugins=Fab`.

## Results on the Panzer III M (desert camo, RTS camera)

- **CamoSwap** keeps the most of the artwork while reading clearly as a team: the sand and dark outlines stay, the red-brown
  band becomes the team colour. Yellow barely reads against sand (a limit of swapping one band).
- **PaintTint** gives the strongest team identity; the pattern survives as light/dark, but the whole tank takes the hue.
- **TopologyAccent** keeps almost all camo but gives the weakest identity on this hull: the Pz III's steep plates are
  small and lie in shadow at this sun angle, so from the RTS camera it reads close to the original. It would do more
  on tall-sided hulls, or combined with a low-strength PaintTint glaze.

## Results on the King Tiger (ambush camo)

- **CamoSwap, dark-green key** is the best fit here: the green stripes (also on the gun barrel) become the team
  colour, dunkelgelb and red-brown stay, and the tank still reads as ambush camo. Yellow is weak against dunkelgelb.
- **CamoSwap, default red-brown key** shows the team colour only on a few brown patches: it must be keyed per camo.
- **PaintTint** works without setup but changes the most; on the bright dunkelgelb it turns pastel (blue reads as
  silver-white), so dark team colours lose saturation on bright camo.
- **TopologyAccent** colours the steep turret and hull sides; better than on the Panzer III because the King Tiger's
  turret walls are larger, but still mostly camo from the RTS camera.

## Not done yet

- Game code does not set `TeamColor` yet; only the capture tool writes the primitive data.
- The shared master and every vehicle instance are untouched; adopting a type means adding its function to
  `MM_Ger_PostApoTank_NoBlend` at the spot above (and to any other vehicle masters).
- Checked on two camos (Panzer III desert, King Tiger ambush); grey or single-colour camo not tried yet.
