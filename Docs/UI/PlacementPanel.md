# Placement panel

The panel shown while the player places a building, an expansion, a field defense or a field trench. It replaced the
old `W_BP_StatsPreview` world-space widget (2026-11-25), which floated 3 m above the preview mesh. On the top-down
camera that put it right on top of the building, so the player couldn't see how the building was turned.

## What it shows

- **Title**: what is placed, in the objective plates' cyan small caps (`DEPLOY BUILDING`, `PLACE EXPANSION`,
  `ATTACH EXPANSION`, `BUILD EXPANSION`, `FIELD CONSTRUCTION`, `PLAN FIELD DEFENSE`, `DIG FIELD TRENCH`).
- **Status line with a lamp**: green when a click places it; red with the reason when it doesn't. For the trench,
  cyan tells the player where to click first. The reasons come from `EConstructionPlacementIssue`, in the order the
  player should fix them:
  1. no build radius;
  2. outside the build radius;
  3. something in the way;
  4. ground too steep;
  5. no free spot on the building;
  6. no ground under the cursor.
- **Numbers**:
  - the rotation, when the preview can turn;
  - the incline against the 5° limit, for previews that check it;
  - the build radius: distance / radius, or how far outside it is.

  A trench shows its length out of 60 m, what it costs while dug (summed per piece the way `ARTSFieldTrench` charges
  it), and whether it is joined to a pit's trench port.
- **Key caps**: LMB, RMB, and the rotate keys. The rotate keys are read from the Enhanced Input mappings of
  `IA_RotateLeft` / `IA_RotateRight` when a preview starts. Comma and period (the default) show as `<` and `>`: the
  same physical keys, and a lone comma or full stop is an unreadable dot on a key cap.

## Where it sits

The panel never covers what is placed:

- Every tick the owner hands it the world bounds of the preview (for a trench, of the whole drawn trench).
  `PlacementPanelDocking::ChooseDock` puts the panel beside their screen rectangle. It tries the current side first,
  so the panel doesn't hop around, then right, left, below and above.
- It keeps 16 units from the screen edges. It stays out of the bottom 312 units (the HUD's selection and action
  panel) and the right 320 units (the minimap and training menu column).
- It goes over the HUD only when that is the only way to keep off the building. When even that fails (the building
  fills the screen), it picks the spot that covers the least.
- It slides after the preview, but jumps when it changes side, and also when sliding would cross the building.
- Docking runs in the owners' ticks (`UW_PlacementPanel::UpdateDocking`), not in the widget's own tick. That way the
  panel moves in the same frame as the preview, and map tests can read its position.
- It never takes a click (HitTestInvisible), so a placement click on the screen under it still lands in the world.
- The socket marker of the socket an expansion preview is snapped to is hidden, because the preview stands on it.
  The other markers no longer take clicks either.

## Code

- `Player/ConstructionPreview/PlacementPanel/`:
  - `UW_PlacementPanel`: builds its own widget tree, no Blueprint;
  - `PlacementPanelTypes.h`: content structs and dock request/result;
  - `PlacementPanelDocking.cpp`: the pure layout maths.
- `ACPPConstructionPreview::CreatePlacementPanel` makes the panel in `InitConstructionPreview`.
  `UpdatePlacementPanel` fills it every tick, and `GetPlacementPanel` lends it out. The old stats widget and its
  component are still handed over by `RTS_BP_PreviewBuilding`, but stay hidden.
- `UFieldTrenchPlacementComponent::UpdatePlacementPanel` borrows the same panel while a trench is laid out.

## Style

- Plates: `MI9_Custom_ThinPanel_32x32_u_n` is the hover info box / cost tooltip plate. `MI9_Custom_ResourceCounter_32x32_u_n`
  is the recessed label field, used for the key caps.
- Colours: the `FSurvivalUiStyle` palette (text `ECE8DC`, muted `9FA09A`, accent `3FF0FF`, warning `E07A5F`,
  accept `8FD46B`), with the objective plates' dark groove between sections.
- Text: Roboto (bold/regular) at whole-number sizes with a 1 px dark shadow (Docs/UI/Skins/ShippingReadyUI.md).

## Tests

- Automation `RTS.PlacementPanel.Docking.*` (`PlacementPanel/Tests/`), run headless:

  ```
  UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests RTS.PlacementPanel;Quit" -nullrhi -unattended
  ```

  It sweeps building rectangles of 40 to 1100 units over 1920, 2560 and 1440 x 1080 screens. It checks that the
  panel stays on screen and never covers a building that has room beside it. It also checks that a building near
  the screen centre never pushes the panel into the HUD band, and that the panel keeps its side.
- Map tests:
  - `-FieldDefenseValidate` checks `placement_panel_clear_of_preview` when the Pak 40 preview follows the cursor.
  - `-FieldDefenseValidateTrench` checks `placement_panel_clear_of_trench` before the confirming click.
  - Both need a rendered viewport (`-RenderOffscreen`). Without one nothing projects and the check logs a `SKIP`.
