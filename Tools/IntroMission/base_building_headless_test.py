"""Throwaway-editor PIE test of Mission_Intro_BaseBuilding on Ger_IntroMission (never run it in the shared editor).

    UnrealEditor.exe RTS_Survival.uproject
      -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/GerMissions/Ger_IntroMission
      -ExecCmds="py <this file>" -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
    env BASE_TEST_FACTION = none | strike | breakthrough   backup faction (none: the game instance decides)
    env BASE_TEST_OUT     = result json path
    env BASE_TEST_LOG     = the run's -abslog (read back for the training restriction log lines)

The mission chain is cut in the editor world before PIE (not saved): Missions[0] = the Mission_Intro_BaseBuilding
instance of the saved chain, so the run starts at the HQ drop. The test plays the player's part:
  HQ glider lands + deploys -> resources + Stage 1 -> a player harvester is spawned -> hints + Stage 2 -> 10 s ->
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
VOICE_LINES = ['IntroMission_HQOperational', 'IntroMission_TrainGamma', 'IntroMission_ExpandGamma',
               'IntroMission_TrainRefinery', 'IntroMission_BuildConverter', 'IntroMission_ConversionToggle',
               'IntroMission_BarracksForgeDrop']


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
            node = manager.get_editor_property('missions')[0]
            while node is not None and node.get_class().get_name() != MISSION:
                node = node.get_editor_property('next_mission')
            check('base_mission_in_saved_chain', node is not None)
            if node is None:
                finish()
                return
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
            if S['t'] - S.get('hq_sample_t', -99.0) >= 2.0:
                S['hq_sample_t'] = S['t']
                trucks = [a for a in actors_of(world, 'Ger_HQ_Truck_C') + actors_of(world, 'Ger_HQ_Truck_Breakthrough_C')]
                gliders = [a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
                           if 'Glider' in a.get_class().get_name()]
                for truck in trucks:
                    meshes = truck.get_components_by_class(unreal.SkeletalMeshComponent)
                    sim = [(m.get_name(), m.is_simulating_physics(), str(m.get_collision_enabled())[-14:]) for m in meshes]
                    parent = truck.get_attach_parent_actor()
                    log(f'hq truck physics {sim} attached_to={parent.get_name() if parent else None} '
                        f'root={truck.root_component.get_name()}')
                for truck in trucks:
                    m = mission(world)
                    spot = m.get_editor_property('HQBuildLocation')
                    near = (truck.get_actor_location() - spot).size2d() < 400 if hasattr(unreal.Vector, 'size2d') else                         ((truck.get_actor_location().x - spot.x) ** 2 + (truck.get_actor_location().y - spot.y) ** 2) ** 0.5 < 400
                    if near and truck.get_velocity().length() < 5:
                        S['hq_still'] = S.get('hq_still', 0) + 1
                    else:
                        S['hq_still'] = 0
                    chassis = [c for c in truck.get_components_by_class(unreal.SkeletalMeshComponent)
                               if c.get_name() == 'Chasis']
                    anim = chassis[0].get_anim_instance() if chassis else None
                    montage = anim.get_current_active_montage() if anim else None
                    building = truck.call_method('GetBuildingMeshComponent')
                    log(f'hq state yaw={round(truck.get_actor_rotation().yaw)} building_visible='
                        f'{building.is_visible() if building else None} montage={montage.get_name() if montage else None} '
                        f'pos={round(anim.montage_get_position(montage), 2) if montage else None} '
                        f'rendered={truck.was_recently_rendered(0.2)} mission_hq={m.get_editor_property("HQ") is not None} '
                        f'deployed={m.call_method("GetIsNomadicDeployedAsBuilding", (truck,))} still={S["hq_still"]}')
                    if os.environ.get('BASE_TEST_CAMERA') and S['hq_still'] == 10 and not S.get('camera_moved'):
                        S['camera_moved'] = True
                        pawn = unreal.GameplayStatics.get_player_pawn(world, 0)
                        pawn.set_actor_location(unreal.Vector(spot.x - 1500, spot.y, spot.z + 2500), False, True)
                        log(f'CAMERA: moved the player camera pawn over the HQ spot ({pawn.get_name()})')
                    if os.environ.get('BASE_TEST_KICK') and S['hq_still'] == 3 and not S.get('hq_kicked'):
                        S['hq_kicked'] = True
                        result = truck.call_method('CreateBuildingAtLocation', (spot, unreal.Rotator(0, 0, 0), True))
                        log(f'KICK: re-ordered the idle HQ truck to build: {result}')
                if trucks or gliders:
                    S.setdefault('hq_samples', []).append(
                        [round(elapsed), [(a.get_class().get_name(), [round(v) for v in (a.get_actor_location().x,
                          a.get_actor_location().y, a.get_actor_location().z)], round(a.get_velocity().length()))
                                          for a in trucks + gliders]])
                    log(f'hq sample {S["hq_samples"][-1]}')
            if elapsed > 3.0 and 'res_before' not in S:
                S['res_before'] = resources(world)
                log(f'resources before HQ {S["res_before"]}')
            if wait_stage(world, 1, 240.0, 'hq_ready', 'hq_deployed'):
                m = mission(world)
                hq = m.get_editor_property('HQ')
                loc = hq.get_actor_location() if hq else None
                check('hq_deployed', hq is not None and m.call_method('GetIsNomadicDeployedAsBuilding', (hq,)),
                      f'HQ at {loc} (wanted near 1450, 300) after {elapsed:.0f} s')
                check('hq_on_concrete', loc is not None and abs(loc.x - 1450) < 800 and abs(loc.y - 300) < 800,
                      f'{loc}')
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
