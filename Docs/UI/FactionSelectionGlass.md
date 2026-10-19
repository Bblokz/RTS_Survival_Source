# Faction selection glass

The three faction buttons in `/Game/RTS_Survival/FactionSelection/UI/BP_W_FactionSelectionMenu`
reuse the Frontstahl `M_UISkinSlice` button instances and the shared `M_UI_GlassPane` shader.
The shared master materials are unchanged.

`/Game/RTS_Survival/FactionSelection/UI/Materials/MI_FactionButton_Glass` tunes the pane for
the faction button proportions. Its 13 px inset follows the skin's inner face. Scratches
remain strongest near the edges, leaving the labels readable.

Each button contains an overlay with its glass image first and its original label content
second. The images are hit-test invisible:

- `M_GlassGerBreakthrough`
- `M_GlassGerThermoKorps`
- `M_GlassGerItalianGame`

`UW_FactionSelectionMenu::SelectFaction` refreshes the selection visuals after each
faction assignment. This covers the initial Breakthrough selection, faction buttons,
and the audio buttons. Each button retains its own dynamic normal/hover materials;
only the selection component of `Fx` changes, preserving the existing hover sweep.
The selected pane receives a stronger warm rim and reflection. The previous faction
returns to neutral glass. No tick or timer is added.

Selection values live in `UpdateFactionButtonSelection` and `UpdateFactionSelectionBrush`.
The glass instance controls scratch density, width, strength, and edge bias.

## Recovery and verification

Original widget and source backups are in `Saved/CodexBackups/FactionGlassSelection_20261018`.
Only the faction widget and its new material instance were saved by the asset commandlet.
Saved structure, material values, and glass hit-testing were checked by reloading them.

The C++ compilation completed without errors. Live Coding stalled before confirming
patch application; the shared editor was left open to preserve unrelated unsaved work.
Runtime selection and rendered layout verification still require recovery/restart and
a successful link/load of the updated code.
