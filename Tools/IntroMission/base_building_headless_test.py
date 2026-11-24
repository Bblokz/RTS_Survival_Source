"""Throwaway-editor PIE test of Mission_Intro_BaseBuilding on Ger_IntroMission (never run it in the shared editor).

    UnrealEditor.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env BASE_TEST_FACTION = none | strike | breakthrough   backup faction (none: the game instance decides)
    env BASE_TEST_OUT     = result json path
    env BASE_TEST_LOG     = the run's -abslog (read back for the training restriction log lines)

The mission chain is cut in the editor world before PIE (not saved): Missions[0] = the Mission_Intro_BaseBuilding
instance of the saved chain, so the run starts at the HQ drop. The test plays the player's part:
  HQ glider lands (glider line) -> the truck drives out -> nomadic conversion takeover (closed by the test) -> convert
  line + "Deploy HQ" marker -> the test converts the HQ as the player (select, convert button, placement click; the
  preview must not ask for a build radius, which proves the glider HQ is the registered player HQ) -> resources + Stage 1 -> a player harvester is spawned -> hints + Stage 2 -> 10 s ->
  Stage 3 (gamma unlocked) -> a gamma truck is delivered by the mission's own cargo plane call (counts as trained,
  builds on landing) -> Stages 4, 5 -> same for the refinery -> Stages 6, 7 -> the converter step is driven by calling
  Step_ConverterBuilt (placing a BXP is not reachable from python; GetPlayerBuiltBuildingExpansionCountOfTypes stays
  untested here) -> barracks + forge trucks dropped without building -> the test orders both to deploy -> complete.
2026-11-12 real-path version: the harvester, Gamma and refinery are trained by clicking the HQ's training menu items
(UMissionMapTestLibrary.MapTest_ClickTrainingOption, training time cheat RTSCheatTraining), the trucks are ordered to
deploy, the converter is built by the option click + placement click (MapTest_StartBxpPlacement / MapTest_PlaceLoadedBxp),
its command card is checked for the conversion toggle, and every mission voice line must be heard playing in order.
"""
import json
import os

import unreal

FACTION = os.environ.get('BASE_TEST_FACTION', 'none')
OUT = os.environ.get('BASE_TEST_OUT', 'D:/UE5Projects/RTS_Survival/Saved/BaseMission/result.json')
RUN_LOG = os.environ.get('BASE_TEST_LOG', '')
MISSION = 'Mission_Intro_BaseBuilding_C'
HARVESTER = '/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Light/Panzer1/Ger_Harvester_Panzer1'
GAMMA_SPOT = unreal.Vector(-1300, -1400, 600)
REFINERY_SPOT = unreal.Vector(4300, 600, 540)
S = {'phase': 'wait_load', 't': 0.0, 'mark': 0.0, 'log': [], 'checks': {}, 'stages': []}


def log(message):
    S['log'].append(f"{S['t']:7.1f} {message}")
    unreal.log_warning('BASETEST ' + message)


def check(name, ok, detail=''):
    S['checks'][name] = {'ok': bool(ok), 'detail': detail}
    log(f"CHECK {name}: {'PASS' if ok else 'FAIL'} {detail}")


def finish():
    if RUN_LOG and os.path.exists(RUN_LOG):
        lines = [l.strip()[-200:] for l in open(RUN_LOG, encoding='utf-8', errors='ignore')
                 if 'Mission training restriction' in l]
        S['restriction_log'] = lines
        check('restriction_log', any('restricted=1' in l for l in lines) and lines and 'cleared' in lines[-1],
              ' | '.join(l.split('Mission training restriction')[-1] for l in lines))
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump({'faction': FACTION, 'checks': S['checks'], 'stages': S['stages'], 'log': S['log'],
               'restriction_log': S.get('restriction_log', [])}, open(OUT, 'w'), indent=1)
    S['phase'] = 'done'
    unreal.SystemLibrary.quit_editor()


def game_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()


def actors_of(world, class_name):
    return [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
            if a.get_class().get_name() == class_name]


def mission(world):
    for obj in unreal.ObjectIterator(unreal.MissionBase):
        if obj.get_class().get_name() == MISSION and obj.get_world() == world:
            return obj
    return None


def resources(world):
    pc = unreal.GameplayStatics.get_player_controller(world, 0)
    manager = pc.call_method('GetPlayerResourceManager')
    return (manager.call_method('GetResourceAmount', (unreal.RTSResourceType.RESOURCE_RADIXITE,)),
            manager.call_method('GetResourceAmount', (unreal.RTSResourceType.RESOURCE_METAL,)))


def spawn_player_harvester(world):
    # Through the mission's async spawner, the path training uses: it sets the unit's subtype (a raw spawn has none).
    option = unreal.TrainingOption()
    option.import_text('(UnitType=UNType_Tank,SubtypeValue=%d)' % int(unreal.TankSubtype.TANK_PZ_I_HARVESTER.value))
    mission(world).call_method('AsyncSpawnActorAtLocation',
                               (option, 77, unreal.Vector(1450, -1700, 600), unreal.Rotator(0, 0, 0)))
    log('asked the mission spawner for a harvester')


def describe_harvesters(world):
    out = []
    for harvester in actors_of(world, 'Ger_Harvester_Panzer1_C'):
        rts = harvester.get_component_by_class(unreal.RTSComponent)
        out.append((harvester.get_name(), str(rts.call_method('GetSubtypeAsTankSubtype')) if rts else None,
                    rts.get_editor_property('owning_player') if rts else None))
    return out


def note_stage(world):
    m = mission(world)
    if m is None:
        return None
    stage = m.get_editor_property('Stage')
    if not S['stages'] or S['stages'][-1][1] != stage:
        S['stages'].append([round(S['t'], 1), stage])
        log(f'stage {stage}')
    return stage


def wait_stage(world, wanted, timeout, next_phase, fail_name):
    stage = note_stage(world)
    elapsed = S['t'] - S['mark']
    if stage is not None and stage >= wanted:
        S['phase'], S['mark'] = next_phase, S['t']
        return True
    if elapsed > timeout:
        check(fail_name, False, f'stage stayed {stage} for {timeout} s')
        finish()
    return False


def deliver(world, subtype, spot, delivery_id):
    m = mission(world)
    m.call_method('DeliverPlayerNomadicByCargoPlane', (subtype, spot, unreal.Rotator(0, 0, 0), delivery_id, True))
    log(f'test delivered {subtype} to {spot} (builds on landing)')


TRAIN_SECONDS = 5
VOICE_LINES = ['IntroMission_HQGliderInbound', 'IntroMission_ConvertHQ', 'IntroMission_HQOperational', 'IntroMission_TrainGamma', 'IntroMission_ExpandGamma',
               'IntroMission_TrainRefinery', 'IntroMission_BuildConverter', 'IntroMission_ConversionToggle',
               'IntroMission_BarracksForgeDrop']


HQ_TRUCKS = ('Ger_HQ_Truck_C', 'Ger_HQ_Truck_Breakthrough_C')
# Placement clicks tried in turn when the HQ spot itself is blocked (a player would click next to it).
CONVERT_OFFSETS = [(0, 0), (600, 0), (0, 600), (0, -600), (1000, 0)]


def open_takeovers(world):
    # WidgetBlueprintLibrary is not exposed to this editor's Python; walk the live user widgets instead.
    return [w for w in unreal.ObjectIterator(unreal.UserWidget)
            if w.get_world() == world and w.is_in_viewport() and w.get_class().get_name().startswith('WBP_')
            and w.get_class().get_name().endswith('Takeover_C')]


def hint_texts(hint):
    widget_comp = hint.get_component_by_class(unreal.WidgetComponent)
    widget = widget_comp.get_widget() if widget_comp else None
    return [str(w.get_text()) for w in unreal.ObjectIterator(unreal.RichTextBlock)
            if widget and w.get_outer() and w.get_typed_outer(unreal.UserWidget) == widget]


def dist2d(a, b):
    return ((a.x - b.x) ** 2 + (a.y - b.y) ** 2) ** 0.5


def opt(unit_type, subtype):
    option = unreal.TrainingOption()
    option.import_text('(UnitType=%s,SubtypeValue=%d)' % (unit_type, int(subtype.value)))
    return option


def click_training(trainer, option):
    """The player's left click on the option in the trainer's menu (UMissionMapTestLibrary).
    @return (queued, locked by mission)"""
    code = unreal.MissionMapTestLibrary.map_test_click_training_option(trainer, option)
    log(f'clicked {option.export_text()} on {trainer.get_name() if trainer else None}: result {code} '
        f'(1 queued, 2 locked by mission, 3 not queued, 0 not on the menu)')
    return code == 1, code == 2


def deploy(nomadic, spot):
    error = nomadic.call_method('CreateBuildingAtLocation', (spot, unreal.Rotator(0, 0, 0), True))
    log(f'ordered {nomadic.get_name()} to deploy at {spot}: {error}')


def note_voice_lines(world):
    """Records each mission voice line the first time an audio component plays it (and the stage then)."""
    heard = S.setdefault('voice', {})
    for comp in unreal.ObjectIterator(unreal.AudioComponent):
        if comp.get_world() != world or not comp.is_playing():
            continue
        sound = comp.get_editor_property('sound')
        name = sound.get_name() if sound else ''
        if name.startswith('IntroMission_') and name not in heard:
            m = mission(world)
            heard[name] = [round(S['t'], 1), m.get_editor_property('Stage') if m else None]
            log(f'voice line playing: {name} (stage {heard[name][1]})')


def voice_checks():
    heard = S.get('voice', {})
    missing = [n for n in VOICE_LINES if n not in heard]
    order = [heard[n][0] for n in VOICE_LINES if n in heard]
    check('voice_lines_played', not missing and order == sorted(order),
          f'heard {heard} missing {missing}')


def tick(delta):
    S['t'] += delta
    phase = S['phase']
    elapsed = S['t'] - S['mark']
    try:
        world = game_world()
        if world is not None and phase not in ('wait_load', 'wait_pie', 'done'):
            note_stage(world)
            note_voice_lines(world)
            pending = S.get('pending_deploy')
            if pending and S['t'] >= pending[2] and phase in ('gamma_deploy', 'refinery_deploy'):
                truck = mission(world).get_editor_property(pending[0])
                if pending[3] < 2 and truck is not None and not mission(world).call_method(
                        'GetIsNomadicDeployedAsBuilding', (truck,)):
                    deploy(truck, pending[1])
                    pending[3] += 1
                    pending[2] = S['t'] + 25.0
                else:
                    S['pending_deploy'] = None
        if phase == 'wait_load' and S['t'] > 15.0:
            editor_world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            manager = actors_of(editor_world, 'BP_MissionManager_C')[0]
            # Since the ridge missions (2026-11-12) the base mission is Missions[2], started by a "Mission Class
            # Completed" trigger on Mission_Intro_ClearSovietRig; search every manager entry and its NextMission chain.
            node = None
            for top in manager.get_editor_property('missions'):
                while top is not None and node is None:
                    if top.get_class().get_name() == MISSION:
                        node = top
                    top = top.get_editor_property('next_mission')
            check('base_mission_in_saved_chain', node is not None)
            if node is None:
                finish()
                return
            # Run it first and at once: drop the trigger that waits for ClearSovietRig (editor world only, not saved).
            node.set_editor_property('mission_trigger', None)
            manager.set_editor_property('missions', [node])
            if os.environ.get('BASE_TEST_HQ'):
                build, touch = [unreal.Vector(*[float(v) for v in part.split(',')])
                                for part in os.environ['BASE_TEST_HQ'].split(';')]
                node.set_editor_property('HQBuildLocation', build)
                node.set_editor_property('HQGliderTouchdown', touch)
                log(f'HQ override build {build} touchdown {touch}')
            if FACTION in ('strike', 'breakthrough'):
                backup = manager.get_editor_property('player_faction_backup_data')
                backup.set_editor_property('set_faction_manually', True)
                backup.set_editor_property('player_faction', unreal.RTSFaction.GER_STRIKE_DIVISION if FACTION ==
                                           'strike' else unreal.RTSFaction.GER_BREAKTHROUGH_DOCTRINE)
                manager.set_editor_property('player_faction_backup_data', backup)
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
            S['phase'], S['mark'] = 'wait_pie', S['t']
        elif phase == 'wait_pie':
            if world is not None and elapsed > 15.0:
                unreal.GameplayStatics.get_player_controller(world, 0).call_method('ClickedStartGameButton')
                log('pressed start game')
                S['phase'], S['mark'] = 'wait_hq', S['t']
            elif elapsed > 180.0:
                log('pie never started')
                finish()
        elif phase == 'wait_hq':
            if elapsed > 3.0 and 'res_before' not in S:
                S['res_before'] = resources(world)
                log(f'resources before HQ {S["res_before"]}')
            panels = open_takeovers(world)
            if panels:
                m = mission(world)
                trucks = [t for c in HQ_TRUCKS for t in actors_of(world, c)]
                hq = m.get_editor_property('HQ')
                heard = S.get('voice', {})
                check('glider_line_before_takeover', 'IntroMission_HQGliderInbound' in heard,
                      f'heard {sorted(heard)}')
                check('conversion_takeover_shown', unreal.GameplayStatics.is_game_paused(world),
                      f'{panels[0].get_class().get_name()} {elapsed:.0f} s after start, '
                      f'paused={unreal.GameplayStatics.is_game_paused(world)}')
                # GetIsInTruckMode is not Blueprint-exposed; the building mesh stays hidden while it is a truck.
                building = hq.call_method('GetBuildingMeshComponent') if hq is not None else None
                check('hq_out_of_glider_as_truck', hq is not None and hq in trucks and not m.call_method(
                    'GetIsNomadicDeployedAsBuilding', (hq,)) and not (building and building.is_visible()),
                      f'HQ {hq.get_name() if hq else None} at {hq.get_actor_location() if hq else None}')
                S['takeover_presses'], S['takeover_press_t'] = 0, -99.0
                S['phase'], S['mark'] = 'close_takeover', S['t']
            elif elapsed > 240.0:
                check('conversion_takeover_shown', False, 'no takeover 240 s after start')
                finish()
        elif phase == 'close_takeover':
            panels = open_takeovers(world)
            if panels and S['t'] - S['takeover_press_t'] > 1.5 and S['takeover_presses'] < 4:
                panels[0].call_method('HandleUnderstoodClicked')
                S['takeover_presses'] += 1
                S['takeover_press_t'] = S['t']
                log(f'pressed the takeover button ({S["takeover_presses"]})')
            elif not panels and S['t'] - S['takeover_press_t'] > 0.5:
                S['takeover_closed_t'] = S['t']
                check('conversion_takeover_closed', not unreal.GameplayStatics.is_game_paused(world),
                      f'{S["takeover_presses"]} press(es)')
                S['phase'], S['mark'] = 'after_takeover', S['t']
            elif elapsed > 20.0:
                check('conversion_takeover_closed', False, f'still open after {S["takeover_presses"]} presses')
                finish()
        elif phase == 'after_takeover' and elapsed > 4.0:
            m = mission(world)
            heard = S.get('voice', {})
            convert = heard.get('IntroMission_ConvertHQ')
            # The line starts from On Closed, i.e. on the closing press (the panel is seen gone a frame later).
            check('convert_line_after_takeover', convert is not None and convert[0] >= S['takeover_press_t'],
                  f'convert line at {convert}, closing press at {S["takeover_press_t"]:.1f}')
            spot = m.get_editor_property('HQBuildLocation')
            hints = actors_of(world, 'BP_Hint_ExpandHQ_C')
            near = [h for h in hints if dist2d(h.get_actor_location(), spot) < 300]
            texts = [hint_texts(h) for h in near]
            check('deploy_hq_marker', len(near) == 1 and any('Deploy HQ' in t for t in texts[0]),
                  f'{len(hints)} marker(s), {len(near)} at the HQ spot, texts {texts}')
            hq = m.get_editor_property('HQ')
            for dx, dy in CONVERT_OFFSETS:
                click = unreal.Vector(spot.x + dx, spot.y + dy, spot.z)
                # 1 ordered, 2 not a truck, 3 no preview, 4 spot rejected, 5 click not taken; +10 = build radius asked.
                code = unreal.MissionMapTestLibrary.map_test_convert_nomadic_at(hq, click)
                ok, needed_radius = code % 10 == 1, code >= 10
                log(f'convert click at {click}: code {code} ordered={ok} needed_build_radius={needed_radius}')
                S.setdefault('convert_tries', []).append([dx, dy, ok, needed_radius, code])
                if ok:
                    break
            tries = S['convert_tries']
            check('hq_registered_converts_without_radius', all(t[3] is False for t in tries),
                  f'needed_build_radius per click {tries}')
            check('hq_conversion_ordered', any(t[2] for t in tries), f'{tries}')
            S['phase'], S['mark'] = 'wait_hq_deployed', S['t']
        elif phase == 'wait_hq_deployed':
            if wait_stage(world, 1, 240.0, 'hq_ready', 'hq_deployed'):
                m = mission(world)
                hq = m.get_editor_property('HQ')
                loc = hq.get_actor_location() if hq else None
                check('hq_deployed', hq is not None and m.call_method('GetIsNomadicDeployedAsBuilding', (hq,)),
                      f'HQ at {loc} (wanted near 1450, 300) after {elapsed:.0f} s')
                check('hq_on_concrete', loc is not None and abs(loc.x - 1450) < 1200 and abs(loc.y - 300) < 1200,
                      f'{loc}')
                check('deploy_hq_marker_removed', not actors_of(world, 'BP_Hint_ExpandHQ_C'),
                      f'{len(actors_of(world, "BP_Hint_ExpandHQ_C"))} left')
        elif phase == 'hq_ready' and elapsed > 3.0:
            radixite, metal = resources(world)
            before = S.get('res_before', (0, 0))
            check('hq_resources_given', radixite - before[0] >= 1000 and metal - before[1] >= 400,
                  f'radixite {before[0]} -> {radixite}, metal {before[1]} -> {metal}')
            unreal.SystemLibrary.execute_console_command(world, f'RTSCheatTraining {TRAIN_SECONDS}')
            hq = mission(world).get_editor_property('HQ')
            queued, locked = click_training(hq, opt('UNType_Nomadic', unreal.NomadicSubtype.NOMADIC_GER_GAMMA_FACILITY))
            check('hq_gamma_locked_before_unlock', locked and not queued, f'locked={locked} queued={queued}')
            queued, locked = click_training(hq, opt('UNType_Tank', unreal.TankSubtype.TANK_PZ_I_HARVESTER))
            check('hq_harvester_trainable', queued and not locked, f'locked={locked} queued={queued}')
            S['phase'], S['mark'] = 'wait_harvester', S['t']
        elif phase == 'wait_harvester':
            if wait_stage(world, 2, 180.0, 'hints', 'harvester_detected'):
                check('harvester_detected', True, f'{describe_harvesters(world)} after {elapsed:.0f} s of training')
        elif phase == 'hints' and elapsed > 2.0:
            radixite = actors_of(world, 'BP_Hint_HarvestRadixite_C')
            metal = actors_of(world, 'BP_Hint_HarvestMetal_C')
            spans = [round(h.get_life_span()) for h in radixite + metal]
            texts = []
            for hint in radixite + metal:
                widget_comp = hint.get_component_by_class(unreal.WidgetComponent)
                widget = widget_comp.get_widget() if widget_comp else None
                rich = [w for w in unreal.ObjectIterator(unreal.RichTextBlock) if widget and w.get_outer() and
                        w.get_typed_outer(unreal.UserWidget) == widget]
                texts.append(str(rich[0].get_text()) if rich else None)
            check('harvest_hints', len(radixite) == 2 and len(metal) == 1 and all(40 <= s <= 46 for s in spans)
                  and all(x and 'Right click to harvest' in x for x in texts),
                  f'radixite {len(radixite)} metal {len(metal)} life spans {spans} texts {texts}')
            S['phase'], S['mark'] = 'wait_gamma_unlock', S['t']
        elif phase == 'wait_gamma_unlock':
            if wait_stage(world, 3, 20.0, 'gamma_train', 'gamma_unlocked'):
                check('gamma_unlocked', True, f'{elapsed:.1f} s after the hints')
                hq = mission(world).get_editor_property('HQ')
                queued, locked = click_training(hq, opt('UNType_Nomadic',
                                                        unreal.NomadicSubtype.NOMADIC_GER_GAMMA_FACILITY))
                check('hq_gamma_trainable_after_unlock', queued and not locked, f'locked={locked} queued={queued}')
        elif phase == 'gamma_train':
            if wait_stage(world, 4, 180.0, 'gamma_deploy', 'gamma_trained'):
                check('gamma_trained', True)
                # A trained unit exists ~2 s before its training ends; order the deploy once it rolled out, as a player.
                S['pending_deploy'] = ['GammaNomadic', GAMMA_SPOT, S['t'] + 5.0, 0]
        elif phase == 'gamma_deploy':
            if wait_stage(world, 5, 240.0, 'refinery_train', 'gamma_deployed'):
                check('gamma_deployed', True)
                hq = mission(world).get_editor_property('HQ')
                queued, locked = click_training(hq, opt('UNType_Nomadic', unreal.NomadicSubtype.NOMADIC_GER_REFINERY))
                check('hq_refinery_trainable_after_unlock', queued and not locked, f'locked={locked} queued={queued}')
        elif phase == 'refinery_train':
            if wait_stage(world, 6, 180.0, 'refinery_deploy', 'refinery_trained'):
                check('refinery_trained', True)
                S['pending_deploy'] = ['RefineryNomadic', REFINERY_SPOT, S['t'] + 5.0, 0]
        elif phase == 'refinery_deploy':
            if wait_stage(world, 7, 240.0, 'converter', 'refinery_deployed'):
                check('refinery_deployed', True)
        elif phase == 'converter' and elapsed > 2.0:
            # The refinery deploy opens the building expansion pack-up takeover (paused); the converter line plays
            # from its On Closed. Close it like a player before building the converter.
            panels = open_takeovers(world)
            if panels:
                if 'bxp_takeover' not in S:
                    S['bxp_takeover'] = [panels[0].get_class().get_name(), unreal.GameplayStatics.is_game_paused(world)]
                    check('bxp_packup_takeover_shown', S['bxp_takeover'][0] == 'WBP_BxpPackUpTakeover_C'
                          and S['bxp_takeover'][1], f'{S["bxp_takeover"]} {elapsed:.1f} s after the refinery deployed')
                if S['t'] - S.get('bxp_press_t', -99.0) > 1.5:
                    S['bxp_press_t'] = S['t']
                    panels[0].call_method('HandleUnderstoodClicked')
                    log('closed the building expansion pack-up takeover')
                return
            if 'bxp_takeover' not in S:
                check('bxp_packup_takeover_shown', False, 'no takeover after the refinery deployed')
                S['bxp_takeover'] = None
            if 'bxp_line_checked' not in S and S.get('bxp_press_t') is not None:
                line = S.get('voice', {}).get('IntroMission_BuildConverter')
                if line is None and S['t'] - S['bxp_press_t'] < 4.0:
                    return
                S['bxp_line_checked'] = True
                # Voice times are rounded to 0.1 s; the line starts in the same frame as the closing press.
                check('converter_line_after_bxp_takeover', line is not None and line[0] >= S['bxp_press_t'] - 0.1,
                      f'converter line at {line}, takeover closed at {S["bxp_press_t"]:.1f}')
            refinery = mission(world).get_editor_property('RefineryNomadic')
            if 'bxp_clicked' not in S:
                S['bxp_clicked'] = unreal.MissionMapTestLibrary.map_test_start_bxp_placement(
                    refinery, unreal.BuildingExpansionType.BTX_REF_CONVERTER)
                check('converter_option_clicked', S['bxp_clicked'], 'refinery offers BTX_RefConverter with a free slot')
                if not S['bxp_clicked']:
                    finish()
                return
            if unreal.MissionMapTestLibrary.map_test_is_bxp_waiting_for_placement(world):
                placed = unreal.MissionMapTestLibrary.map_test_place_loaded_bxp(world, refinery.get_actor_location())
                check('converter_placed_and_paid', placed, f'clicked at the refinery {refinery.get_actor_location()}')
                S['phase'], S['mark'] = 'converter_build', S['t']
            elif elapsed > 60.0:
                check('converter_placed_and_paid', False, 'the converter never loaded for placement')
                finish()
        elif phase == 'converter_build':
            if wait_stage(world, 8, 300.0, 'converter_card', 'converter_built'):
                converters = actors_of(world, 'Ger_Ref_Converter_C')
                check('converter_built', bool(converters), f'{len(converters)} converter(s), {elapsed:.0f} s to build')
        elif phase == 'converter_card' and elapsed > 2.0:
            converters = actors_of(world, 'Ger_Ref_Converter_C')
            lib = unreal.MissionMapTestLibrary
            card = [(lib.map_test_has_ability(c, unreal.AbilityID.ID_ENABLE_RESOURCE_CONVERSION),
                     lib.map_test_has_ability(c, unreal.AbilityID.ID_DISABLE_RESOURCE_CONVERSION)) for c in converters]
            check('converter_command_card_toggle', any(a or b for a, b in card),
                  f'(enable, disable) on each converter: {card}')
            S['phase'], S['mark'] = 'wait_drops', S['t']
        elif phase == 'wait_drops':
            m = mission(world)
            barracks, forge = m.get_editor_property('BarracksNomadic'), m.get_editor_property('ForgeNomadic')
            if barracks is not None and forge is not None and 'drops_seen' not in S:
                S['drops_seen'] = S['t']
                log(f'barracks truck at {barracks.get_actor_location()}, forge truck at {forge.get_actor_location()}')
            if 'drops_seen' in S and S['t'] - S['drops_seen'] > 8.0:
                built = [m.call_method('GetIsNomadicDeployedAsBuilding', (n,)) for n in (barracks, forge)]
                check('factories_dropped_as_trucks', not any(built), f'deployed {built} 8 s after landing')
                for nomadic, spot in ((barracks, m.get_editor_property('BarracksDropLocation')),
                                      (forge, m.get_editor_property('ForgeDropLocation'))):
                    error = nomadic.call_method('CreateBuildingAtLocation', (spot, unreal.Rotator(0, 0, 0), True))
                    log(f'ordered {nomadic.get_name()} to deploy at {spot}: {error}')
                S['phase'], S['mark'] = 'wait_complete', S['t']
            elif elapsed > 240.0:
                check('factories_dropped_as_trucks', False, 'no barracks / forge delivery in 240 s')
                finish()
        elif phase == 'wait_complete':
            m = mission(world)
            if m.call_method('GetIsMissionComplete'):
                check('mission_completed', m.get_editor_property('Stage') == 9, f'{elapsed:.0f} s after deploy order')
                S['phase'], S['mark'] = 'after_complete', S['t']
            elif elapsed > 240.0:
                check('mission_completed', False, f'stage {m.get_editor_property("Stage")}')
                finish()
        elif phase == 'after_complete' and elapsed > 12.0:
            # The last line (barracks + forge drop) plays well before this; give queued portraits time anyway.
            voice_checks()
            finish()
    except Exception as error:  # noqa: BLE001 - report and quit, never hang the throwaway editor
        log(f'ERROR {phase}: {error!r}')
        finish()


unreal.register_slate_post_tick_callback(tick)
log('script loaded')
