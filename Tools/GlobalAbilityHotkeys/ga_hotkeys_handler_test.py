"""Throwaway-editor test: does each Ctrl chord reach its sidebar slot's click path, and does the disabled-button gate hold?
Run like ga_hotkeys_headless_test.py (throwaway DebugGame editor on RTS_ToCopySmall, never the shared editor).

Observable: UGlobalAbilitiesManager::OnClickedAbilityButton hides the ability description (W_GA_Description collapses)
even when the ability itself stays blocked. Per loaded slot:
  1. hover the slot (description shown), hold Ctrl+<slot key> with the button disabled  -> description must stay shown;
  2. hover again, force the button enabled every tick, hold Ctrl+<slot key>             -> description must collapse.
Keys are forced through Enhanced Input (Input.+key), so chords and chord blockers run as with a keyboard.
Log: Saved/GlobalAbilityHotkeys/handler_result.json
"""
import json
import os

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityHotkeys/'
SLOT_KEYS = ['A', 'S', 'D', 'F', 'Z', 'X', 'C', 'V']
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'cases': [], 'case': 0, 'force_enable': None}


def log(message):
    S['log'].append(f"{S['t']:.2f} {message}")
    unreal.log('GAHANDLERTEST ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump({'log': S['log'], 'cases': S['cases']}, open(OUT_DIR + 'handler_result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def find_all(cls, world):
    return [o for o in unreal.ObjectIterator(cls) if o.get_world() == world]


def items(world):
    return sorted(find_all(unreal.W_GA_Item, world), key=lambda entry: entry.get_name())


def button_of(item):
    return item.get_editor_property('AbilityButton').get_editor_property('M_Button')


def description_shown(world):
    return any(desc.is_visible() for desc in find_all(unreal.W_GA_Description, world))


def console(world, command):
    unreal.SystemLibrary.execute_console_command(world, command)


def build_cases(world):
    for index, item in enumerate(items(world)):
        if item.is_visible() and index < len(SLOT_KEYS):
            S['cases'].append({'slot': index, 'key': SLOT_KEYS[index], 'mode': 'disabled'})
            S['cases'].append({'slot': index, 'key': SLOT_KEYS[index], 'mode': 'forced_enabled'})
    log(f"cases {len(S['cases'])}")


def tick(delta):
    if S['phase'] == 'done':
        return
    S['t'] += delta
    phase = S['phase']
    since = S['t'] - S['mark']
    try:
        world = game_world()
        if S['force_enable'] is not None and world is not None:
            button_of(items(world)[S['force_enable']]).set_is_enabled(True)
        if phase == 'wait_load' and S['t'] > 15.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie' and world is not None and since > 8.0:
            unreal.GameplayStatics.get_player_controller(world, 0).call_method('ClickedStartGameButton')
            S['phase'], S['mark'] = 'wait_hud', S['t']
        elif phase == 'wait_hud' and since > 6.0:
            build_cases(world)
            S['phase'], S['mark'] = 'case_hover', S['t']
        elif phase == 'case_hover' and since > 0.5:
            if S['case'] >= len(S['cases']):
                S['phase'], S['mark'] = 'end', S['t']
                return
            case = S['cases'][S['case']]
            item = items(world)[case['slot']]
            S['force_enable'] = case['slot'] if case['mode'] == 'forced_enabled' else None
            if S['force_enable'] is None:
                button_of(item).set_is_enabled(False)
            item.call_method('OnHoveredAbilityButton', (True,))
            case['shown_before'] = description_shown(world)
            S['phase'], S['mark'] = 'case_ctrl', S['t']
        elif phase == 'case_ctrl' and since > 0.2:
            case = S['cases'][S['case']]
            case['enabled_at_press'] = button_of(items(world)[case['slot']]).get_is_enabled()
            console(world, 'Input.+key LeftControl')
            S['phase'], S['mark'] = 'case_key', S['t']
        elif phase == 'case_key' and since > 0.2:
            console(world, f"Input.+key {S['cases'][S['case']]['key']}")
            S['phase'], S['mark'] = 'case_check', S['t']
        elif phase == 'case_check' and since > 0.3:
            case = S['cases'][S['case']]
            case['shown_after'] = description_shown(world)
            expected_hidden = case['mode'] == 'forced_enabled'
            case['pass'] = case['shown_before'] and (case['shown_after'] != expected_hidden)
            console(world, f"Input.-key {case['key']}")
            console(world, 'Input.-key LeftControl')
            S['force_enable'] = None
            items(world)[case['slot']].call_method('OnHoveredAbilityButton', (False,))
            log(f'case {case}')
            S['case'] += 1
            S['phase'], S['mark'] = 'case_hover', S['t']
        elif phase == 'end' and since > 1.0:
            passed = sum(1 for case in S['cases'] if case.get('pass'))
            log(f"passed {passed}/{len(S['cases'])}")
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            finish()
        if S['t'] > 240.0:
            log('timeout')
            finish()
    except Exception as error:  # noqa
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
