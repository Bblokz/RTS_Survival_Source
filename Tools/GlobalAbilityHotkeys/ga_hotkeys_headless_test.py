"""Throwaway-editor test for the global ability sidebar hotkeys (IA_GlobalAbility1..8, Ctrl+A/S/D/F/Z/X/C/V).
Run in a throwaway DebugGame editor (no UECP endpoint), never the shared one:
  UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
    -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
    -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding

Checks, with real keys forced through Enhanced Input (Input.+key, so chords and chord blockers are evaluated):
  - both mapping contexts are applied and their priorities (chorded must be higher for chord blockers);
  - every sidebar item shows its own hotkey label (logged text + screenshot);
  - plain A and Ctrl+A: screenshots with `showdebug enhancedinput`, read by eye - plain A must fire IA_ActionButton6
    only, Ctrl+A IA_GlobalAbility1 only (bound-value reads are 'n/a': the getter is not exposed to Python);
  - the key bindings menu builds and shows the chorded entries (6 existing + 8 global ability ones).
Activation itself is covered by ga_hotkeys_handler_test.py.
Log: Saved/GlobalAbilityHotkeys/result.json, screenshots via `shot showui` into Saved/Screenshots.
"""
import json
import os
import time

import unreal

OUT_DIR = 'D:/UE5Projects/RTS_Survival/Saved/GlobalAbilityHotkeys/'
INPUT = '/Game/RTS_Survival/Blueprints/Player/Input/'
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'result': {}}


def log(message):
    S['log'].append(f"{S['t']:.2f} {message}")
    unreal.log('GAHOTKEYTEST ' + message)


def finish():
    os.makedirs(OUT_DIR, exist_ok=True)
    json.dump({'log': S['log'], 'result': S['result']}, open(OUT_DIR + 'result.json', 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def in_world(obj, world):
    return obj.get_world() == world


def find_all(cls, world):
    return [o for o in unreal.ObjectIterator(cls) if in_world(o, world)]


def console(world, command):
    unreal.SystemLibrary.execute_console_command(world, command)


def action(name):
    folder = 'ChordedActions/' if name.startswith('IA_GlobalAbility') else 'Actions/'
    return unreal.load_asset(INPUT + folder + name)


def input_component(world, pc):
    for comp in find_all(unreal.EnhancedInputComponent, world):
        if comp.get_outer() == pc:
            return comp
    return None


def bound_value(pc, action_name):
    getter = getattr(unreal.EnhancedInputLibrary, 'get_bound_action_value', None)
    if getter is None:
        return 'n/a'
    value = getter(pc, action(action_name))
    return unreal.EnhancedInputLibrary.conv_input_action_value_to_bool(value)


def aim_shown(world):
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerAimAbility)
    return [not a.is_hidden() if hasattr(a, 'is_hidden') else not a.get_editor_property('hidden') for a in actors]


def check_contexts(world, pc):
    subsystems = [s for s in unreal.ObjectIterator(unreal.EnhancedInputLocalPlayerSubsystem)
                  if not s.get_name().startswith('Default__')]
    out = {'subsystems': len(subsystems)}
    for subsystem in subsystems:
        for path in ['IMC_RTSDefault', 'ChordedActions/RTS_IMC_ChordedActions']:
            imc = unreal.load_asset(INPUT + path)
            out[subsystem.get_name() + ':' + path] = str(subsystem.has_mapping_context(imc))
    log(f'contexts {out}')
    S['result']['contexts'] = out


def read_labels(world):
    labels = []
    for item in find_all(unreal.W_GA_Item, world):
        badge = item.get_editor_property('HotkeyWidget')
        text = badge.get_editor_property('M_KeyText').get_text() if badge else None
        labels.append({'item': item.get_name(), 'item_vis': str(item.get_visibility()),
                       'badge_vis': str(badge.get_visibility()) if badge else None, 'text': str(text)})
    labels.sort(key=lambda entry: entry['item'])
    log(f'labels {labels}')
    S['result']['labels'] = labels


def press(world, keys):
    for key in keys:
        console(world, f'Input.+key {key}')


def release(world, keys):
    for key in keys:
        console(world, f'Input.-key {key}')


def sample(world, pc, tag):
    values = {name: bound_value(pc, name) for name in ['IA_GlobalAbility1', 'IA_ActionButton6']}
    entry = {'values': values, 'aim_shown': aim_shown(world)}
    log(f'{tag} {entry}')
    S['result'][tag] = entry
    # The Enhanced Input debug overlay lists the actions that triggered this frame: visual proof of no double fire.
    console(world, 'shot showui')


SLOT_KEYS = ['A', 'S', 'D', 'F', 'Z', 'X', 'C', 'V']


def slot_states(world):
    states = []
    for item in sorted(find_all(unreal.W_GA_Item, world), key=lambda entry: entry.get_name()):
        button = item.get_editor_property('AbilityButton').get_editor_property('M_Button')
        states.append({'item': item.get_name(), 'visible': item.is_visible(), 'enabled': button.get_is_enabled()})
    return states


def first_enabled_slot(world):
    for index, state in enumerate(slot_states(world)):
        if state['visible'] and state['enabled']:
            return index
    return None


def open_keybindings(world, pc):
    pc.call_method('OpenEscapeMenu')
    menus = find_all(unreal.W_EscapeMenu, world)
    log(f'escape menus {len(menus)}')
    for button in find_all(unreal.Button, world):
        if button.get_name() == 'M_ButtonKeyBindings' and button.get_outer().get_outer() in menus:
            log('clicking key bindings button')
            button.on_clicked.broadcast()


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
            log('pressed start')
            S['phase'], S['mark'] = 'wait_hud', S['t']
        elif phase == 'wait_hud' and since > 6.0:
            console(world, 'RTSCheatResources 100000')
            console(world, 'RTSCheatBlueprints 100000')
            for step in (check_contexts, read_labels):
                try:
                    step(world, pc) if step is check_contexts else step(world)
                except Exception as error:  # noqa
                    log(f'ERROR {step.__name__}: {error!r}')
            log(f'aim before {aim_shown(world)}')
            console(world, 'shot showui')
            S['phase'], S['mark'] = 'plain_a', S['t']
        elif phase == 'plain_a' and since > 2.0:
            console(world, 'showdebug enhancedinput')
            press(world, ['A'])
            S['phase'], S['mark'] = 'plain_a_sample', S['t']
        elif phase == 'plain_a_sample' and since > 0.4:
            sample(world, pc, 'plain_A')
            release(world, ['A'])
            S['phase'], S['mark'], S['wait0'] = 'wait_available', S['t'], S['t']
        elif phase == 'wait_available' and since > 5.0:
            S['mark'] = S['t']
            slot = first_enabled_slot(world)
            log(f'poll available slot {slot}')
            if slot is not None or S['t'] - S['wait0'] > 270.0:
                S['phase'] = 'ctrl_a'
        elif phase == 'ctrl_a':
            states = slot_states(world)
            log(f'slot states {states}')
            try:
                items = sorted(find_all(unreal.W_GA_Item, world), key=lambda entry: entry.get_name())
                items[0].call_method('OnHoveredAbilityButton', (True,))
                for desc in find_all(unreal.W_GA_Description, world):
                    log(f"hover cooldown: {desc.get_editor_property('Cooldown').get_text()}")
                items[0].call_method('OnHoveredAbilityButton', (False,))
            except Exception as error:  # noqa
                log(f'ERROR hover: {error!r}')
            S['result']['slot_states'] = states
            S['slot'] = first_enabled_slot(world)
            S['slot_key'] = SLOT_KEYS[S['slot']] if S['slot'] is not None else 'A'
            log(f"testing slot {S['slot']} with Ctrl+{S['slot_key']}")
            press(world, ['LeftControl'])
            S['phase'], S['mark'] = 'ctrl_a_key', S['t']
        elif phase == 'ctrl_a_key' and since > 0.3:
            press(world, [S['slot_key']])
            S['phase'], S['mark'] = 'ctrl_a_sample', S['t']
        elif phase == 'ctrl_a_sample' and since > 0.4:
            sample(world, pc, 'ctrl_A')
            S['phase'], S['mark'] = 'ctrl_a_release', S['t']
        elif phase == 'ctrl_a_release' and since > 0.5:
            release(world, [S['slot_key'], 'LeftControl'])
            S['phase'], S['mark'] = 'after_release', S['t']
        elif phase == 'after_release' and since > 1.5:
            sample(world, pc, 'after_release')
            console(world, 'showdebug')
            open_keybindings(world, pc)
            S['phase'], S['mark'] = 'keybindings', S['t']
        elif phase == 'keybindings' and since > 2.0:
            entries = find_all(unreal.W_EscapeMenuChordedKeyBindingEntry, world)
            log(f'chorded entries {len(entries)}')
            S['result']['chorded_entries'] = len(entries)
            S['phase'], S['mark'] = 'keybindings_shot', S['t']
        elif phase == 'keybindings_shot' and since > 3.0:
            entries = find_all(unreal.W_EscapeMenuChordedKeyBindingEntry, world)
            log(f'chorded entry visibility {[str(e.get_visibility()) for e in entries]}')
            console(world, 'shot showui')
            S['phase'], S['mark'] = 'end', S['t']
        elif phase == 'end' and since > 2.0:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            finish()
        if S['t'] > 420.0:
            log('timeout')
            finish()
    except Exception as error:  # noqa
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
