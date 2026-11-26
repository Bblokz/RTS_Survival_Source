# Tactical unit icons behind glass

Small rounded-square plates that show a unit's tactical class icon (light tank, autocannon, anti-air, ...) behind
lit, scratched glass. They are meant to sit in the bottom corners of a unit image, replacing the tactical icons
that used to be painted into the unit portrait textures. Some units have two, so there is a bottom-left and a
bottom-right plate.

**Status (2026-10-15): built and verified, but not placed anywhere.** It was tried on the selected unit's
portrait in the action UI (`BP_W_SelectedUnitInfo`). The user liked the look but removed it from the main game UI;
it will be used somewhere else later. Nothing in the game references the widgets now. The C++, the material and
the placement script are kept so it can be placed again quickly.

## Files

| What | Where |
|---|---|
| One glass plate (widget) | `RTS_Survival/GameUI/TacticalIcons/W_TacticalUnitIcon.h/.cpp` (`UW_TacticalUnitIcon`) |
| Bottom-left + bottom-right pair (widget) | `RTS_Survival/GameUI/TacticalIcons/W_TacticalUnitIconCorners.h/.cpp` (`UW_TacticalUnitIconCorners`) |
| Settings per plate | `RTS_Survival/GameUI/TacticalIcons/TacticalUnitIconSettings.h` (`FTacticalUnitIconSettings`: `IconTexture`, `LightColor`) |
| Glass material | `/Game/RTS_Survival/Textures/GameUI/UnitIcons/Tactical/Glass/M_UI_TacticalIconGlass` |
| Preview instances (tuning only) | `.../Tactical/Glass/MI_TacticalIconGlass_Preview_Blue` and `_Red` |
| Icon textures | `/Game/RTS_Survival/Textures/GameUI/UnitIcons/Tactical/` (`T_LightTankClass_Icon_V2`, `T_AutoCannon_Class_V2`, `T_MediumTankClass_Icon_V2`, `T_AntiAirClass_Icon_V3_NoStar`, `T_AssaultClass_Icon_V3`, `T_CASAircraftClass_Icon_V3_Clean`) |
| Generator / placement script | `UISkins/_generator/ui_tactical_icons.py` (repository root, next to the other UI skin scripts) |

## How it works

- **`UW_TacticalUnitIcon`** builds its own tree (SizeBox → Image) in an `Initialize()` override, not in
  `NativeOnInitialized`, because `Initialize` also runs in the UMG designer and so the designer preview works.
  `SetupTacticalIcon(FTacticalUnitIconSettings)` creates a dynamic instance of the glass material and sets the
  `Icon` texture and `LightColor` parameters. A plate without a texture collapses itself. `SetIconSize(float)` sets
  the edge length in UI units (default 32).
- **`UW_TacticalUnitIconCorners`** builds an Overlay with two `UW_TacticalUnitIcon`s, aligned bottom-left and
  bottom-right. Its editable properties are `M_LeftIcon`, `M_RightIcon`, `M_IconSize` (default 32) and
  `M_CornerInset` (default 3). At runtime, use `SetTacticalIcons(Left, Right)` or `ClearTacticalIcons()`
  (BlueprintCallable).
- Both widgets are hit-test invisible, so hover and clicks still reach the image underneath.
- **Material path:** `UW_TacticalUnitIcon::M_GlassMaterial` is a soft pointer whose default (set in the
  constructor) is the material path above. If you move the material, update `TacticalUnitIconConstants`.
- **Cooking:** nothing hard-references the material; only that C++ soft path does. Before shipping a placement,
  make sure the material is cooked: set `M_GlassMaterial` explicitly on the placed instance, or add the `Glass`
  folder to a cook rule.

## Placing it

Put a `W_TacticalUnitIconCorners` in the same Overlay as the unit image, after the image, with fill/fill alignment.
Then set Left Icon / Right Icon (texture + light colour) and the size under **Tactical Icon** in the details panel,
or call `SetTacticalIcons` from code with per-unit data. That per-unit data does not exist yet: there is no
mapping from unit type or subtype to tactical icons.

`ui_tactical_icons.py` automates the action UI trial:

- `apply()` wraps `M_UnitInfoImage` in `BP_W_SelectedUnitInfo` in an Overlay (`M_TacticalIconOverlay`) and adds
  the corners widget (`M_TacticalUnitIcons`) with the test icons. State is saved in
  `Saved/UISkinSwitcher/tactical_icons_state.json`.
- `restore()` undoes it. This is the current state.
- `resize_portrait_icons()` re-applies the size and inset.
- `build()` rebuilds the material and the preview instances.

Run them in the editor through Python. Add the folder to `sys.path` first:

```python
import sys; sys.path.append(r'D:/UE5Projects/RTS_Survival/UISkins/_generator')
import ui_tactical_icons; ui_tactical_icons.apply()
```

Settings the user approved on the action UI portrait (128-unit box, 108-unit image):

- plate size **42**, corner inset **0**, so the plates cover the portrait corners completely;
- left: `T_LightTankClass_Icon_V2` with light blue `(0.55, 0.88, 1.0)`, the same light blue as the glass panes'
  `CooldownTint`;
- right: `T_AutoCannon_Class_V2` with red `(1.0, 0.12, 0.08)`.

## Placard badges in the unit description (2026-11-25)

The selected unit description (`BP_W_UnitDescription_Horizontal`, C++ `UW_SelectedUnitDescription`, shown when the
action UI portrait is hovered) shows the portrait's own tactical icons in its left column, each with a one-line
explanation, instead of the armor type icon + name. Portraits without tactical icons (variants, Soviet units, trucks,
buildings without a placard plan) still show the armor type.

- **Source of truth:** the placard plans in `Source/Tools/TacticalIconBackgrounds/plans/*.txt`
  (`<portrait texture>=<clean png>|<left icon>|<right icon>`), the same files that engraved the portraits.
- `tactical_icon_catalog.py` (plain Python) renders each icon spec (`stem` or `stem@colour`) as a standalone placard
  badge (`D:/UE_Assets/Icons/TrainingUI/Placard/Badges/T_TacPlacard_*.png`, 128 px, same look as on the portraits)
  and writes `tactical_icon_catalog.json` next to the script. The one-line texts are `DESCRIPTIONS` in that script;
  text before a `:` is highlighted in the UI. A new icon stem in a plan needs a line there or the script stops.
- `fill_tactical_icon_catalog.py` (in the editor) imports the badges to
  `/Game/RTS_Survival/Textures/GameUI/UnitIcons/Tactical/Placard/` and fills `DA_TacticalIconCatalog`
  (`UTacticalIconCatalog`: icon spec -> badge + text, portrait package path -> left/right spec).
- **Runtime:** `UW_SelectedUnitInfo` passes the portrait brush's resource (`M_UnitInfoImage`) to
  `UW_SelectedUnitDescription::SetupTacticalIcons` after the Blueprint set the portrait, and again while hovered (it
  only redoes the rows when the portrait changed). Lookup is by the portrait texture's package path, so a unit whose
  portrait has no placard plan simply keeps the armor type.
- **Widgets** (all `BindWidgetOptional`): `ArmorTypeOverlay` (collapsed when icons show), `TacticalIconsBox`,
  `TacticalIconRow1/2`, `TacticalIconImage1/2` (44 px SizeBox), `TacticalIconText1/2` (`DT_RichText`, wrap 220).
  The catalog is the `TacticalIconCatalog` default on the WBP (hard reference, so badges are cooked).

After changing a plan or a text: run `tactical_icon_catalog.py`, then `fill_tactical_icon_catalog.py` in the editor.

## Material parameters (`M_UI_TacticalIconGlass`)

A UI-domain, translucent, unlit material with one Custom HLSL node. It outputs linear colour with straight alpha.
Tune the defaults in `SCALARS` / `VECTORS` in the script, then run `build()`.

- **Plate:** `CornerRadiusFrac`, `BezelFrac`, `BezelColor`, `BezelLightSpill` (light on the bezel's inner lip).
- **Light behind the glass:** `LightColor`, `LightIntensity`, `LightFalloff`, `LightOffsetY` (light comes from
  below), and `LightSaturation`. The volume light uses `pow(LightColor, 1 + LightSaturation)`, so pale tints do not
  turn the glass milky.
- **Backing:** `BackingColor` and `BackingOpacity` for the dark smoked glass.
- **Icon:** `IconScale`, `Refraction` (slight lens), `IconTint`, `IconBrightness`, `IconLightBoost`.
- **Halo:** `HaloMip` and `HaloIntensity`; the halo is a blurred icon alpha from a lower mip.
- **Surface:**
  - edge: `EdgeBand`, `EdgeGlow`;
  - lit rim and reflection: `RimIntensity`, `StreakIntensity`;
  - scratches (screen space, the same code as `M_UI_GlassPane`): `ScratchIntensity`, `ScratchCellSize`,
    `ScratchCoverage`, `ScratchLength`, `ScratchWidthPx`, `ScratchEdgeBias`, `ScratchSeed`;
  - `DustIntensity`.

## Lessons from building it

- **Linear output makes small white terms read as haze.** A 0.08 white reflection streak came out as a ~80/255
  grey veil over the whole plate. Keep white surface terms around 0.02–0.03, and tint the edge glow and scratches
  with the deepened light colour.
- **Check the look with the offscreen capture, not in the editor.** Run `-UISnapshot` on
  `Test_TrainingUI` with the Development editor target; it selects units, so the selected unit portrait is
  visible. The capture loads the assets from disk, so save first.
- **Saves can fail silently** while another session's `UnrealEditor-Cmd -game` run has the HUD package loaded. The
  log says "MoveFile was unable to move ... failed to save", and the next capture shows the old asset. Wait until
  no `UnrealEditor-Cmd.exe` is running before you save, and check the log afterwards.

## Removing it completely

These steps remove the glass plates only. `TacticalIconCatalog.h/.cpp` in the same folder belongs to the unit
description badges above; keep it.

1. If it is placed, run `ui_tactical_icons.restore()` or delete the widget from that Blueprint.
2. Delete `W_TacticalUnitIcon*` and `TacticalUnitIconSettings.h` from `RTS_Survival/GameUI/TacticalIcons/`.
3. Delete the `Tactical/Glass` asset folder.
4. Delete `UISkins/_generator/ui_tactical_icons.py` and this doc.
