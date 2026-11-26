"""PIE check of Mission_OW_Arrival (stage M00) in my own UECP editor with Ger_OderWarthe open:
    python ../../BerlinMap/mission/ep.py <port> pie_arrival_check.py
MODE 'natural' lets the tour play out; MODE 'skip' holds Space (UW_HoldToSkip.StartHold, the real hold path) SKIP_AT
seconds after the tour took the screen, i.e. during the title card. Logs OWARRIVAL lines every second (tour shot,
title card on screen, column state, player units near the ridge, HQ count) and takes `shot showui` screenshots.
Never `py` it from inside execute_python; all state lives on one object (execute_python drops globals).
"""
import unreal

MODE = 'natural'


class ArrivalCheck:
    STAGE = 'Mission_OW_Arrival'
    SKIP_AT = 2.0
    NATURAL_SHOTS = [1.0, 4.0, 14.0, 24.0, 33.0, 44.0]
    SKIP_SHOTS = [1.5, 3.5, 6.0, 12.0]
    RIDGE = (-20500.0, -24900.0)
    RIDGE_RADIUS = 3500.0

    def __init__(self, mode):
        self.mode = mode
        self.phase = 'begin'
        self.t = 0.0
        self.mark = 0.0
        self.tour_t0 = None
        self.next_sample = 0.0
        self.shots = list(self.NATURAL_SHOTS if mode == 'natural' else self.SKIP_SHOTS)
        self.skipped = False
        self.end_t = None
        self.handle = None

    def log(self, message):
        import unreal
        unreal.log_warning(f'OWARRIVAL {self.mode} {self.t:.1f} {message}')

    def world(self):
        import unreal
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()

    def finish(self):
        import unreal
        self.phase = 'done'
        unreal.unregister_slate_post_tick_callback(self.handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

    def stage(self):
        import unreal
        for mission in unreal.ObjectIterator(unreal.MissionBase):
            if mission.get_class().get_name().startswith(self.STAGE) and mission.get_world() == self.world():
                return mission
        return None

    def widgets_in_viewport(self, class_part):
        import unreal
        return [w for w in unreal.ObjectIterator(unreal.UserWidget)
                if class_part in w.get_class().get_name() and w.is_in_viewport() and w.get_world() == self.world()]

    def units_near_ridge(self):
        import unreal
        count = 0
        for actor in unreal.GameplayStatics.get_all_actors_of_class(self.world(), unreal.TankMaster):
            loc = actor.get_actor_location()
            if (loc.x - self.RIDGE[0]) ** 2 + (loc.y - self.RIDGE[1]) ** 2 < self.RIDGE_RADIUS ** 2:
                count += 1
        return count

    def sample(self, stage):
        import unreal
        playing = stage.call_method('GetIsCameraTourPlaying')
        shot = stage.call_method('GetCameraTourShotIndex')
        title = len(self.widgets_in_viewport('LocationIntro'))
        skip = len(self.widgets_in_viewport('HoldToSkip'))
        hqs = len(stage.call_method('GetPlayerNomadicsOfType', (unreal.NomadicSubtype.NOMADIC_GER_HQ,)))
        tanks = len(unreal.GameplayStatics.get_all_actors_of_class(self.world(), unreal.TankMaster))
        self.log(f'tour={playing} shot={shot} title={title} skipPrompt={skip} '
                 f'released={stage.get_editor_property("bColumnReleased")} '
                 f'tourDone={stage.get_editor_property("bTourFinished")} attempts='
                 f'{stage.get_editor_property("ReleaseAttempts")} hqs={hqs} vehicles={tanks} '
                 f'nearRidge={self.units_near_ridge()} deck={stage.call_method("GetHasPlayerProfileLoaded")}')
        return playing

    def log_vehicles(self):
        import unreal
        for actor in unreal.GameplayStatics.get_all_actors_of_class(self.world(), unreal.TankMaster):
            loc = actor.get_actor_location()
            self.log(f'vehicle {actor.get_class().get_name()} at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f})')

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
                    self.phase = 'run'
            elif self.phase == 'run':
                stage = self.stage()
                if stage is None:
                    return
                if self.tour_t0 is None and stage.call_method('GetCameraTourShotIndex') >= 0:
                    self.tour_t0 = self.t
                    self.log('tour on screen')
                since = self.t - self.tour_t0 if self.tour_t0 is not None else -1.0
                if self.t >= self.next_sample:
                    self.next_sample = self.t + 1.0
                    self.sample(stage)
                if self.mode == 'skip' and not self.skipped and since >= self.SKIP_AT:
                    prompts = self.widgets_in_viewport('HoldToSkip')
                    self.log(f'holding Space on {len(prompts)} prompt(s)')
                    for prompt in prompts:
                        prompt.call_method('StartHold')
                    self.skipped = True
                if self.shots and since >= self.shots[0]:
                    self.log(f'screenshot at {self.shots.pop(0):.1f}')
                    unreal.SystemLibrary.execute_console_command(self.world(), 'shot showui')
                if self.tour_t0 is not None and self.end_t is None and not stage.call_method('GetIsCameraTourPlaying'):
                    self.end_t = self.t
                    self.log('tour over')
                    self.log_vehicles()
                if self.end_t is not None and self.t - self.end_t > 25.0:
                    self.sample(stage)
                    self.log_vehicles()
                    self.log('end')
                    self.finish()
            if self.t > 200.0:
                self.log('timeout')
                self.finish()
        except Exception as error:  # noqa
            self.log(f'ERROR {self.phase}: {error!r}')
            self.finish()


_check = ArrivalCheck(MODE)
_check.handle = unreal.register_slate_post_tick_callback(_check.tick)
