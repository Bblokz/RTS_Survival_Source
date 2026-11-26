"""PIE check of M05 (Mission_OW_SecondBearing) and the two bonus stages, run through M03/M04 first (see
pie_m03_m04_check.py), in my own UECP editor with Ger_OderWarthe open:
    python ../../BerlinMap/mission/ep.py <port> pie_m05_check.py
After the first wave is destroyed: take DF 2 (M05 done), DF 3-5 (all five posts bonus done), then kill Werk 1's tunnel
mouth with damage (tunnel bonus done, the director counts it collapsed). Logs OWDC lines.
Presses Start, holds Space on the arrival tour, takes DF 1 (ForceCaptureByPlayer). Expected:
- the Direction Finding field manual opens; it is screenshotted and closed with UNDERSTOOD;
- the first wave spawns at a tunnel mouth and both stages show their state;
- the wave's units are destroyed by the script, then M04 completes with its supply drop.
Logs OWDC lines every second. Never `py` it from inside execute_python; all state lives on one object.
"""
import unreal


class StageCheck:
    STAGE_CLASSES = ('Mission_OW_SecondBearing', 'Mission_OW_BonusRadioTruck')
    MANUAL_SCREENSHOT_AFTER = 3.0
    KILL_WAVE_AFTER = 20.0

    def __init__(self):
        self.phase = 'begin'
        self.t = 0.0
        self.mark = 0.0
        self.next_sample = 0.0
        self.handle = None
        self.captured_at = None
        self.manual_seen_at = None
        self.wave_seen_at = None
        self.killed = False
        self.end_at = None
        self.radixite_before = None
        self.manual_shot = False

    def log(self, message):
        import unreal
        unreal.log_warning(f'OWDC {self.t:.1f} {message}')

    def world(self):
        import unreal
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()

    def finish(self):
        import unreal
        self.phase = 'done'
        unreal.unregister_slate_post_tick_callback(self.handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

    def in_viewport(self, class_part):
        import unreal
        return [w for w in unreal.ObjectIterator(unreal.UserWidget)
                if class_part in w.get_class().get_name() and w.is_in_viewport() and w.get_world() == self.world()]

    def stage(self, name):
        import unreal
        for mission in unreal.ObjectIterator(unreal.MissionBase):
            if mission.get_class().get_name().startswith(name) and mission.get_world() == self.world():
                return mission
        return None

    def enemies(self):
        import unreal
        found = []
        for cls in (unreal.TankMaster, unreal.SquadController):
            for actor in unreal.GameplayStatics.get_all_actors_of_class(self.world(), cls):
                component = actor.get_component_by_class(unreal.RTSComponent)
                # The decoy truck and its escort are not part of the wave.
                if any(part in actor.get_class().get_name() for part in ('ZilRadio', 'BA12', 'Ba12', 'BA-12')):
                    continue
                if component is not None and component.call_method('GetOwningPlayer') == 2:
                    found.append(actor)
        return found

    def resources(self):
        import unreal
        pc = unreal.GameplayStatics.get_player_controller(self.world(), 0)
        manager = pc.get_component_by_class(unreal.PlayerResourceManager)
        return manager.call_method('GetResourceAmount', (unreal.RTSResourceType.RESOURCE_RADIXITE,)) if manager else -1

    def sample(self):
        m03, m04 = self.stage('Mission_OW_ListeningPost'), self.stage('Mission_OW_FirstAttack')
        director = None
        import unreal
        for actor in unreal.GameplayStatics.get_all_actors_of_class(self.world(), unreal.WerkWaveDirector):
            director = actor
        enemies = self.enemies()
        self.log(f'm03={m03 is not None} m04={m04 is not None} '
                 f'sent={m04.get_editor_property("bFirstWaveSent") if m04 else None} '
                 f'waves={director.call_method("GetWavesLaunched") if director else None} enemies={len(enemies)} '
                 f'manual={len(self.in_viewport("DirectionFindingTakeover"))} radixite={self.resources()}')
        return enemies

    def completed(self, name):
        import unreal
        live = [m for m in unreal.ObjectIterator(unreal.MissionBase) if m.get_world() == self.world()]
        cls = unreal.load_object(None, '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/OderWarthe/%s.%s_C'
                                 % (name, name))
        return live[0].call_method('GetHasMissionClassCompleted', (cls,)) if live and cls else None

    def stage_state(self):
        return ' '.join('%s:%s/%s' % (n.split('_')[-1], self.stage(n) is not None, self.completed(n))
                        for n in self.STAGE_CLASSES)

    def take(self, index):
        import unreal
        manager = unreal.DirectionFindingManager.find_direction_finding_manager(self.world())
        manager.get_editor_property('stations')[index].call_method('ForceCaptureByPlayer', (1,))
        self.log(f'took DF {index + 1}')

    def truck_state(self):
        import unreal
        decoy = unreal.DecoyRadioTruck.find_decoy_radio_truck(self.world())
        manager = unreal.DirectionFindingManager.find_direction_finding_manager(self.world())
        truck = decoy.call_method('GetRadioTruck') if decoy else None
        where = '(%.0f, %.0f)' % (truck.get_actor_location().x / 100, truck.get_actor_location().y / 100) if truck else 'none'
        return (f'truck={where} transmitting={decoy.call_method("GetIsTransmitting") if decoy else None} '
                f'destroyed={decoy.call_method("GetIsRadioTruckDestroyed") if decoy else None} '
                f'decoy={manager.call_method("GetIsDecoyActive")}'), decoy, truck

    def tick_m05(self, since):
        import unreal
        steps = getattr(self, 'm05_steps', None)
        if steps is None:
            steps = self.m05_steps = {'next_log': 0.0}
        state, decoy, truck = self.truck_state()
        if since >= steps['next_log']:
            steps['next_log'] = since + 5.0
            self.log(state + ' ' + self.stage_state())
        if 'fast' not in steps:
            steps['fast'] = since
            unreal.SystemLibrary.execute_console_command(self.world(), 'slomo 4')
            self.log('slomo 4 until the truck transmits')
        if decoy and decoy.call_method('GetIsTransmitting') and 'parked' not in steps:
            steps['parked'] = since
            unreal.SystemLibrary.execute_console_command(self.world(), 'slomo 1')
            self.take(1)
            self.log('truck parked; took DF 2, slomo 1')
        if 'parked' in steps and since - steps['parked'] > 14.0 and 'map' not in steps:
            steps['map'] = since
            self.log('minimap shot next (OpenFullMapView is not callable from Python)')
        if 'map' in steps and since - steps['map'] > 2.0 and 'shot' not in steps:
            steps['shot'] = since
            unreal.SystemLibrary.execute_console_command(self.world(), 'shot showui')
            self.log('screenshot of the decoy wedges')
        if 'shot' in steps and since - steps['shot'] > 2.0 and 'kill' not in steps and truck:
            steps['kill'] = since
            unreal.GameplayStatics.apply_damage(truck, 1000000.0, None, None, unreal.DamageType)
            self.log('damaged the radio truck')
        if 'kill' in steps and since - steps['kill'] > 5.0:
            self.log('final ' + state + ' ' + self.stage_state())
            self.finish()
        if since > 360.0:
            self.log('truck never transmitted: ' + state)
            self.finish()

    def tick(self, delta):
        import unreal
        if self.phase == 'done':
            return
        self.t += delta
        try:
            if self.phase == 'begin':
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
                self.phase, self.mark = 'wait_pie', self.t
            elif self.phase == 'wait_pie':
                world = self.world()
                if world is not None and self.t - self.mark > 12.0:
                    unreal.GameplayStatics.get_player_controller(world, 0).call_method('ClickedStartGameButton')
                    self.log('pressed start')
                    self.phase, self.mark = 'skip_tour', self.t
            elif self.phase == 'skip_tour':
                for prompt in self.in_viewport('HoldToSkip'):
                    prompt.call_method('StartHold')
                if self.t - self.mark > 8.0 and not self.in_viewport('HoldToSkip'):
                    self.radixite_before = self.resources()
                    manager = unreal.DirectionFindingManager.find_direction_finding_manager(self.world())
                    station = manager.get_editor_property('stations')[0]
                    station.call_method('ForceCaptureByPlayer', (1,))
                    self.captured_at = self.t
                    self.log(f'took DF 1 ({station.get_actor_label()}), radixite {self.radixite_before}')
                    self.phase = 'run'
            elif self.phase == 'run':
                if self.t >= self.next_sample:
                    self.next_sample = self.t + 1.0
                    enemies = self.sample()
                    if enemies and self.wave_seen_at is None:
                        self.wave_seen_at = self.t
                        loc = enemies[0].get_actor_location()
                        self.log(f'wave spawned near ({loc.x:.0f}, {loc.y:.0f})')
                    if (not self.killed and self.wave_seen_at is not None
                            and self.t - self.wave_seen_at > self.KILL_WAVE_AFTER):
                        for enemy in enemies:
                            enemy.destroy_actor()
                        self.killed = True
                        self.log(f'destroyed {len(enemies)} wave units')
                manuals = self.in_viewport('DirectionFindingTakeover')
                if manuals and self.manual_seen_at is None and not self.manual_shot:
                    self.manual_seen_at = self.t
                    self.log('field manual open')
                since_manual = self.t - self.manual_seen_at if self.manual_seen_at is not None else -1.0
                if manuals and self.manual_seen_at is not None and since_manual > self.MANUAL_SCREENSHOT_AFTER                         and not self.manual_shot:
                    unreal.SystemLibrary.execute_console_command(self.world(), 'shot showui')
                    self.manual_shot = True
                    self.log('screenshot of the manual')
                if manuals and self.manual_shot and since_manual > self.MANUAL_SCREENSHOT_AFTER + 1.5:
                    for manual in manuals:
                        manual.call_method('HandleUnderstoodClicked')
                    self.log('UNDERSTOOD')
                    self.manual_seen_at = None
                if self.killed and self.end_at is None:
                    self.end_at = self.t
                if self.end_at is not None:
                    self.tick_m05(self.t - self.end_at)
            if self.t > 900.0:
                self.log('timeout')
                self.finish()
        except Exception as error:  # noqa
            self.log(f'ERROR {self.phase}: {error!r}')
            self.finish()


_check = StageCheck()
_check.handle = unreal.register_slate_post_tick_callback(_check.tick)
