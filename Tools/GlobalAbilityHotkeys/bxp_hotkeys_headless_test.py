"""Throwaway-editor test for the building expansion hotkeys (IA_BuildingExpansion1..10, Alt+Q/W/E/R/T/A/S/D/F/G).
Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    -log=BxpHotkeyTest.log

Keys are forced through Enhanced Input (Input.+key), so chords and chord blockers run as with a keyboard.
  1. labels: every bxp item badge and the nomadic badge text (logged) + a screenshot with the bxp panel forced visible;
  2. overlay shots (`showdebug enhancedinput`) holding Alt+Q and Ctrl+A, read by eye: Alt+Q fires IA_BuildingExpansion1
     only (not ActionButton1 / NomadicExpansion), Ctrl+A fires IA_GlobalAbility1 only (not IA_BuildingExpansion6);
  3. gate + handler per item slot, with the panel forced visible: an untouched item still has status BXS_NotUnlocked, so
     a click logs "This building expansion is not unlocked". Disabled button -> no new line; enabled -> one new line.
  4. the key bindings menu builds its chorded entries (1 nomadic + 10 bxp + 8 global ability = 19).
Log: Saved/GlobalAbilityHotkeys/bxp_result.json
"""
import json
import os

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityHotkeys/'
LOG_FILE = 'D:/UE5Projects/RTS_Survival/Saved/Logs/BxpHotkeyTest.log'
CLICK_MARK = 'This building expansion is not unlocked'
SLOT_KEYS = ['Q', 'W', 'E', 'R', 'T', 'A', 'S', 'D', 'F', 'G']
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'cases': [], 'case': 0, 'result': {}}


def log(message):
    S['log'].append(f"{S['t']:.2f} {message}")
    unreal.log('BXPHOTKEYTEST ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump({'log': S['log'], 'cases': S['cases'], 'result': S['result']},
              open(OUT_DIR + 'bxp_result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def find_all(cls, world):
    return [o for o in unreal.ObjectIterator(cls) if o.get_world() == world]


def console(world, command):
    unreal.SystemLibrary.execute_console_command(world, command)


def click_marks():
    try:
        with open(LOG_FILE, 'r', encoding='utf-8', errors='ignore') as handle:
            return handle.read().count(CLICK_MARK)
    except OSError:
        return -1


def item_panel(world):
    panels = find_all(unreal.W_BuildingUI_ItemPanel, world)
    return panels[0] if panels else None


def bxp_items(world):
    return sorted(find_all(unreal.W_ItemBuildingExpansion, world), key=lambda item: item.get_name())


def badge_text(widget):
    if widget is None:
        return None
    return {'text': str(widget.get_editor_property('M_KeyText').get_text()), 'vis': str(widget.get_visibility())}


def parent_of(widget):
    # Inside one user widget follow the panel parents; at its root hop to the user widget that owns the tree.
    parent = widget.get_parent()
    if parent is not None:
        return parent
    outer = widget.get_outer()
    if outer is not None and outer.get_class().get_name() == 'WidgetTree':
        owner = outer.get_outer()
        return owner if isinstance(owner, unreal.Widget) else None
    return None


def force_panel_visible(world):
    panel = item_panel(world)
    child, widget = None, panel
    # Show every container up the chain (and pick our branch in switchers) so the panel actually draws.
    while widget is not None:
        if isinstance(widget, unreal.WidgetSwitcher) and child is not None:
            widget.set_active_widget(child)
        if not widget.is_visible():
            widget.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)
        child, widget = widget, parent_of(widget)
    panel.set_visibility(unreal.SlateVisibility.VISIBLE)
    for border in find_all(unreal.Border, world):
        if border.get_name() == 'BuildingUIBorder':
            border.set_visibility(unreal.SlateVisibility.SELF_HIT_TEST_INVISIBLE)


def read_labels(world):
    items = bxp_items(world)
    labels = [{'item': item.get_name(), **(badge_text(item.get_editor_property('M_BxpHotkey')) or {})} for item in items]
    panel = item_panel(world)
    nomadic = badge_text(panel.get_editor_property('M_Hotkey_NomadicExpansion')) if panel else None
    log(f'bxp labels {labels}')
    log(f'nomadic label {nomadic}')
    S['result']['labels'] = labels
    S['result']['nomadic'] = nomadic


def build_cases(world):
    for index, _item in enumerate(bxp_items(world)):
        for mode in ('disabled', 'enabled'):
            S['cases'].append({'slot': index, 'key': SLOT_KEYS[index], 'mode': mode})
    log(f"cases {len(S['cases'])}")


def tick(delta):
    if S['phase'] == 'done':
        return
    S['t'] += delta
    phase = S['phase']
    since = S['t'] - S['mark']
    try:
        world = game_world()
        pc = unreal.GameplayStatics.get_player_controller(world, 0) if world else None
        if phase == 'wait_load' and S['t'] > 15.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie' and world is not None and since > 8.0:
            pc.call_method('ClickedStartGameButton')
            S['phase'], S['mark'] = 'wait_hud', S['t']
        elif phase == 'wait_hud' and since > 6.0:
            read_labels(world)
            force_panel_visible(world)
            for item in bxp_items(world):
                item.set_visibility(unreal.SlateVisibility.VISIBLE)
            S['phase'], S['mark'] = 'label_shot', S['t']
        elif phase == 'label_shot' and since > 0.5:
            console(world, 'shot showui')
            console(world, 'showdebug enhancedinput')
            S['phase'], S['mark'] = 'alt_q', S['t']
        elif phase == 'alt_q' and since > 1.0:
            console(world, 'Input.+key LeftAlt')
            S['phase'], S['mark'] = 'alt_q_key', S['t']
        elif phase == 'alt_q_key' and since > 0.2:
            console(world, 'Input.+key Q')
            S['phase'], S['mark'] = 'alt_q_shot', S['t']
        elif phase == 'alt_q_shot' and since > 0.3:
            log('shot: holding Alt+Q')
            console(world, 'shot showui')
            S['phase'], S['mark'] = 'ctrl_a', S['t']
        elif phase == 'ctrl_a' and since > 0.5:
            console(world, 'Input.-key Q')
            console(world, 'Input.-key LeftAlt')
            console(world, 'Input.+key LeftControl')
            S['phase'], S['mark'] = 'ctrl_a_key', S['t']
        elif phase == 'ctrl_a_key' and since > 0.3:
            console(world, 'Input.+key A')
            S['phase'], S['mark'] = 'ctrl_a_shot', S['t']
        elif phase == 'ctrl_a_shot' and since > 0.3:
            log('shot: holding Ctrl+A')
            console(world, 'shot showui')
            S['phase'], S['mark'] = 'ctrl_a_release', S['t']
        elif phase == 'ctrl_a_release' and since > 0.5:
            console(world, 'Input.-key A')
            console(world, 'Input.-key LeftControl')
            console(world, 'showdebug')
            build_cases(world)
            S['phase'], S['mark'] = 'case_setup', S['t']
        elif phase == 'case_setup' and since > 0.5:
            if S['case'] >= len(S['cases']):
                S['phase'], S['mark'] = 'keybindings', S['t']
                return
            case = S['cases'][S['case']]
            force_panel_visible(world)
            item = bxp_items(world)[case['slot']]
            item.set_visibility(unreal.SlateVisibility.VISIBLE)
            item.get_editor_property('M_BxpItemButton').set_is_enabled(case['mode'] == 'enabled')
            case['marks_before'] = click_marks()
            console(world, 'Input.+key LeftAlt')
            S['phase'], S['mark'] = 'case_key', S['t']
        elif phase == 'case_key' and since > 0.2:
            console(world, f"Input.+key {S['cases'][S['case']]['key']}")
            S['phase'], S['mark'] = 'case_release', S['t']
        elif phase == 'case_release' and since > 0.3:
            case = S['cases'][S['case']]
            console(world, f"Input.-key {case['key']}")
            console(world, 'Input.-key LeftAlt')
            S['phase'], S['mark'] = 'case_check', S['t']
        elif phase == 'case_check' and since > 1.0:
            case = S['cases'][S['case']]
            case['marks_after'] = click_marks()
            clicked = case['marks_after'] > case['marks_before']
            case['clicked'] = clicked
            case['pass'] = clicked == (case['mode'] == 'enabled')
            log(f'case {case}')
            S['case'] += 1
            S['phase'], S['mark'] = 'case_setup', S['t']
        elif phase == 'keybindings' and since > 0.5:
            pc.call_method('OpenEscapeMenu')
            menus = find_all(unreal.W_EscapeMenu, world)
            for button in find_all(unreal.Button, world):
                if button.get_name() == 'M_ButtonKeyBindings' and button.get_outer().get_outer() in menus:
                    button.on_clicked.broadcast()
            S['phase'], S['mark'] = 'keybindings_count', S['t']
        elif phase == 'keybindings_count' and since > 2.0:
            entries = find_all(unreal.W_EscapeMenuChordedKeyBindingEntry, world)
            S['result']['chorded_entries'] = len(entries)
            passed = sum(1 for case in S['cases'] if case.get('pass'))
            log(f"chorded entries {len(entries)}; cases passed {passed}/{len(S['cases'])}")
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            finish()
        if S['t'] > 300.0:
            log('timeout')
            finish()
    except Exception as error:  # noqa
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
