"""The five info takeover style options (D is live; see build_takeover_style.py): art files and where the shared M_UI_GlassPane shader goes.

plate_glass / button_glass: one glass rect inside M_TKO_GlassOverArt (art texels from the edges; height > 0 pins the
rect to the top), with M_UI_GlassPane's own parameters. section_glass / well_glass: an MI of the shared
M_UI_GlassPane (parent unchanged) in the VariantSectionGlass brush slot / over the illustration.
"""

PLATE_ART = {'ArtWidth': 1240.0, 'ArtHeight': 920.0, 'BottomBand': 124.0, 'BlendTexels': 40.0}
BUTTON_ART = {'ArtWidth': 560.0, 'ArtHeight': 96.0, 'BottomBand': 0.0, 'BlendTexels': 1.0}

BUTTON_GLASS = {'rect': (9, 9, 9, 9, 0), 'CornerRadiusPx': 5.0, 'InsetFrac': 0.0, 'EdgeBand': 0.22,
                'RimIntensity': 0.36, 'EdgeShadow': 0.16, 'StreakIntensity': 0.09, 'StreakOffset': 0.12,
                'ScratchIntensity': 0.42, 'ScratchCellSize': 0.55, 'ScratchCoverage': 0.62, 'ScratchLength': 0.9,
                'ScratchWidthPx': 0.75, 'ScratchEdgeBias': 0.35, 'ScratchSeed': 23.0, 'DustIntensity': 0.10,
                'EdgeTint': (0.7, 0.9, 1.0, 0.10)}

OPTIONS = {
    'A': {
        'name': 'Frontstahl',
        'title': 'Frontstahl plate',
        'line': 'Battle-worn gunmetal plate with a raised title strip, bolts and cyan brackets; the step / card panes '
                'and UNDERSTOOD are scratched glass.',
        'section_glass': {'CornerRadiusPx': 4.0, 'InsetFrac': 0.0, 'InsetPx': 0.5, 'EdgeBand': 0.035,
                          'RimIntensity': 0.34, 'EdgeShadow': 0.22, 'StreakIntensity': 0.045, 'StreakOffset': 0.1,
                          'ScratchIntensity': 0.34, 'ScratchCellSize': 0.10, 'ScratchCoverage': 0.5,
                          'ScratchLength': 0.9, 'ScratchWidthPx': 0.8, 'ScratchEdgeBias': 0.55, 'ScratchSeed': 7.0,
                          'DustIntensity': 0.07, 'EdgeTint': (0.55, 0.8, 0.95, 0.12)},
        'button_glass': BUTTON_GLASS,
    },
    'B': {
        'name': 'Rauchglas',
        'blur': 0.0,
        'title': 'Smoked glass sheet',
        'line': 'The whole panel is one smoked glass sheet over a faint backlit grid, held by machined corner clamps '
                'in a thin steel frame; the glass shader covers the full panel.',
        'plate_glass': {'rect': (13, 13, 13, 13, 0), 'CornerRadiusPx': 3.0, 'InsetFrac': 0.0, 'EdgeBand': 0.05,
                        'RimIntensity': 0.45, 'EdgeShadow': 0.25, 'StreakIntensity': 0.045, 'StreakOffset': -0.30,
                        'ScratchIntensity': 0.24, 'ScratchCellSize': 0.05, 'ScratchCoverage': 0.5,
                        'ScratchLength': 1.0, 'ScratchWidthPx': 0.8, 'ScratchEdgeBias': 0.35, 'ScratchSeed': 11.0,
                        'DustIntensity': 0.08, 'EdgeTint': (0.6, 0.8, 0.95, 0.06)},
        'button_glass': dict(BUTTON_GLASS, EdgeTint=(0.6, 0.85, 1.0, 0.14)),
    },
    'C': {
        'name': 'Kartentisch',
        'blur': 0.0,
        'title': 'Map table',
        'line': 'A plotting table: grid letters and ruler ticks in the frame, faint topographic contours and a compass '
                'rose on dark slate; the pictures sit under scratched glass, and UNDERSTOOD is glass.',
        'well_glass': {'CornerRadiusPx': 2.0, 'InsetFrac': 0.0, 'InsetPx': 0.0, 'EdgeBand': 0.08,
                       'RimIntensity': 0.42, 'EdgeShadow': 0.28, 'StreakIntensity': 0.10, 'StreakOffset': 0.15,
                       'ScratchIntensity': 0.40, 'ScratchCellSize': 0.16, 'ScratchCoverage': 0.6,
                       'ScratchLength': 0.9, 'ScratchWidthPx': 0.8, 'ScratchEdgeBias': 0.45, 'ScratchSeed': 13.0,
                       'DustIntensity': 0.10, 'EdgeTint': (0.7, 0.88, 1.0, 0.10)},
        'button_glass': BUTTON_GLASS,
    },
    'D': {
        'name': 'Geraeteplatte',
        # the skin's original cyan labels and pips (live until G replaced D on 2026-11-12)
        'label_color': '#38c8ff', 'accent_color': '#3ff0ff',
        'title': 'Instrument panel',
        'line': 'Field-grey crinkle-painted instrument panel: black enamel title placard with brass rim and lamps, '
                'riveted border, enamel footer rail; the pictures get gauge glass and the panes are frosted glass.',
        'section_glass': {'CornerRadiusPx': 4.0, 'InsetFrac': 0.0, 'InsetPx': 0.5, 'EdgeBand': 0.06,
                          'RimIntensity': 0.30, 'EdgeShadow': 0.22, 'StreakIntensity': 0.03, 'StreakOffset': 0.2,
                          'ScratchIntensity': 0.26, 'ScratchCellSize': 0.07, 'ScratchCoverage': 0.45,
                          'ScratchLength': 0.9, 'ScratchWidthPx': 0.8, 'ScratchEdgeBias': 0.6, 'ScratchSeed': 3.0,
                          'DustIntensity': 0.16, 'EdgeTint': (0.78, 0.86, 0.84, 0.14)},
        'well_glass': {'CornerRadiusPx': 3.0, 'InsetFrac': 0.0, 'InsetPx': 0.0, 'EdgeBand': 0.10,
                       'RimIntensity': 0.5, 'EdgeShadow': 0.35, 'StreakIntensity': 0.14, 'StreakOffset': 0.18,
                       'ScratchIntensity': 0.30, 'ScratchCellSize': 0.14, 'ScratchCoverage': 0.55,
                       'ScratchLength': 0.9, 'ScratchWidthPx': 0.8, 'ScratchEdgeBias': 0.55, 'ScratchSeed': 19.0,
                       'DustIntensity': 0.16, 'EdgeTint': (0.85, 0.9, 0.8, 0.14)},
        # light glass over the enamel key, mainly there so the main UI's hover (Hover / HoverSweep / Pressed) shows
        # 2026-11-12 user: stronger hover on the buttons only (the panes keep the quiet Pane* settings).
        'button_glass': dict(BUTTON_GLASS, ScratchIntensity=0.22, ScratchCoverage=0.5, StreakIntensity=0.05,
                             DustIntensity=0.06, RimIntensity=0.28, EdgeTint=(0.7, 0.9, 1.0, 0.06),
                             HoverRimBoost=0.7, HoverEdgeTint=0.18, HoverSweepIntensity=0.3, HoverScratchBoost=0.8),
    },
    'E': {
        'name': 'Ruine',
        'title': 'Battle-scarred',
        'line': 'Wrecked gunmetal: rust at the edges, soot, dents, a shrapnel tear and a lost bolt; heavily scratched '
                'glass on the title band, the panes and UNDERSTOOD.',
        'plate_glass': {'rect': (8, 8, 8, 0, 88), 'CornerRadiusPx': 3.0, 'InsetFrac': 0.0, 'EdgeBand': 0.15,
                        'RimIntensity': 0.45, 'EdgeShadow': 0.22, 'StreakIntensity': 0.05, 'StreakOffset': 0.0,
                        'ScratchIntensity': 0.5, 'ScratchCellSize': 0.35, 'ScratchCoverage': 0.72,
                        'ScratchLength': 1.0, 'ScratchWidthPx': 0.85, 'ScratchEdgeBias': 0.2, 'ScratchSeed': 29.0,
                        'DustIntensity': 0.12, 'EdgeTint': (0.6, 0.8, 0.95, 0.08)},
        'section_glass': {'CornerRadiusPx': 4.0, 'InsetFrac': 0.0, 'InsetPx': 0.5, 'EdgeBand': 0.04,
                          'RimIntensity': 0.40, 'EdgeShadow': 0.24, 'StreakIntensity': 0.06, 'StreakOffset': 0.0,
                          'ScratchIntensity': 0.55, 'ScratchCellSize': 0.12, 'ScratchCoverage': 0.75,
                          'ScratchLength': 1.1, 'ScratchWidthPx': 0.9, 'ScratchEdgeBias': 0.25, 'ScratchSeed': 31.0,
                          'DustIntensity': 0.14, 'EdgeTint': (0.6, 0.8, 0.95, 0.12)},
        'button_glass': dict(BUTTON_GLASS, ScratchIntensity=0.55, ScratchCoverage=0.8, DustIntensity=0.16),
    },
}


# Round 2 (2026-11-12): black / dark gunmetal in the Frontstahl family, no neon cyan, no brass (gen_style_art_dark.py).
# Glass tints are neutral steel; the hover tint follows each option's one accent.
def _dark_glass(hover_tint):
    pane = {'CornerRadiusPx': 4.0, 'InsetFrac': 0.0, 'InsetPx': 0.5, 'EdgeBand': 0.04, 'RimIntensity': 0.30,
            'EdgeShadow': 0.22, 'StreakIntensity': 0.04, 'StreakOffset': 0.1, 'ScratchIntensity': 0.30,
            'ScratchCellSize': 0.10, 'ScratchCoverage': 0.5, 'ScratchLength': 0.9, 'ScratchWidthPx': 0.8,
            'ScratchEdgeBias': 0.55, 'ScratchSeed': 7.0, 'DustIntensity': 0.08,
            'EdgeTint': (0.82, 0.84, 0.86, 0.10), 'HoverTint': hover_tint}
    well = dict(OPTIONS['D']['well_glass'], EdgeTint=(0.85, 0.86, 0.86, 0.12), HoverTint=hover_tint)
    button = dict(OPTIONS['D']['button_glass'], EdgeTint=(0.82, 0.84, 0.86, 0.06), HoverTint=hover_tint)
    return pane, well, button


for _key, _name, _title, _line, _hover, _label, _accent in (
        ('F', 'Schwarzstahl', 'Blackened steel',
         'Blackened gunmetal with recessed screwed slot plates for the title and footer, chrome rims and dome bolts; '
         'steel-grey labels, bone pilot lamps.', (0.92, 0.90, 0.84, 0.5), '#aab3b9', '#d8d4c6'),
        ('G', 'Rotsignal', 'Black oxide, signal red',
         'Black-oxide plate with raised gunmetal title and footer plates and knurled end caps; one dull red index '
         'tab, red labels and red pilot lamps.', (0.85, 0.30, 0.25, 0.4), '#c24a3a', '#d8d4c6'),
        ('H', 'Feldgrau', 'Field-radio black',
         'Black-grey wrinkle paint with worn edges, an engraved keyline, bakelite title and footer plates in chrome '
         'bezels; desaturated olive labels and lamps.', (0.66, 0.72, 0.45, 0.45), '#a3ab7c', '#a3ab7c')):
    _pane, _well, _button = _dark_glass(_hover)
    OPTIONS[_key] = {'name': _name, 'title': _title, 'line': _line, 'section_glass': _pane, 'well_glass': _well,
                     'button_glass': _button, 'label_color': _label, 'accent_color': _accent}
