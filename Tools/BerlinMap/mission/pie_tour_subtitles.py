"""PIE check of a Berlin camera tour's top subtitle strip (UW_CinematicOverlay voice line mode).
Send to my own UECP editor with Ger_Berlin open: python ep.py <port> pie_tour_subtitles.py [Stage Function]
(edit STAGE / FUNCTION below); never `py` it from inside execute_python.
Starts PIE, presses start, calls STAGE.FUNCTION on the live stage object (a tour function), logs the strip's
speaker + words every second and takes `shot showui` screenshots at SHOTS wall seconds after the call.
Results: BERLINTOURSUBS lines in the editor log; screenshots in Saved/Screenshots/WindowsEditor.
execute_python drops the script's globals when it returns, so all state lives on one object and every method
imports what it needs.
"""
import unreal


class TourSubtitleCheck:
    STAGE = 'Mission_Berlin_Arrival'
    FUNCTION = 'PlayLeitturmTour'
    SHOTS = [3.0, 7.0, 11.0, 15.0]
    END_AFTER = 22.0

    def __init__(self):
        self.phase = 'begin'
        self.t = 0.0
        self.mark = 0.0
        self.wall0 = 0.0
        self.next_sample = 0.0
        self.shots = list(self.SHOTS)
        self.handle = None

    def log(self, message):
        import unreal
        unreal.log(f'BERLINTOURSUBS {self.t:.1f} {message}')

    def game_world(self):
        import unreal
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()

    def finish(self):
        import unreal
        self.phase = 'done'
        unreal.unregister_slate_post_tick_callback(self.handle)
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()

    def overlay_texts(self):
        import unreal
        rows = []
        for widget in unreal.ObjectIterator(unreal.UserWidget):
            if 'CinematicOverlay' not in widget.get_class().get_name() or not widget.is_in_viewport():
                continue
            texts = []
            for block in unreal.ObjectIterator(unreal.TextBlock):
                outer = block.get_outer()
                while outer is not None and outer != widget:
                    outer = outer.get_outer()
                if outer == widget and block.is_visible() and str(block.get_text()):
                    texts.append(str(block.get_text()))
            rows.append(f'{widget.get_name()}: ' + ' | '.join(texts))
        return rows or ['no overlay in viewport']

    def find_stage(self):
        import unreal
        world = self.game_world()
        for mission in unreal.ObjectIterator(unreal.MissionBase):
            if mission.get_class().get_name().startswith(self.STAGE) and mission.get_world() == world:
                return mission
        return None

    def tick(self, delta):
        import time
        import unreal
        if self.phase == 'done':
            return
        self.t += delta
        try:
            if self.phase == 'begin':
                unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
                self.phase, self.mark = 'wait_pie', self.t
            elif self.phase == 'wait_pie':
                world = self.game_world()
                if world is not None and self.t - self.mark > 15.0:
                    unreal.GameplayStatics.get_player_controller(world, 0).call_method('ClickedStartGameButton')
                    self.log('pressed start')
                    self.phase, self.mark = 'wait_hud', self.t
            elif self.phase == 'wait_hud' and self.t - self.mark > 6.0:
                stage = self.find_stage()
                self.log(f'stage {stage}')
                stage.call_method(self.FUNCTION)
                self.wall0 = time.perf_counter()
                self.phase = 'tour'
            elif self.phase == 'tour':
                since = time.perf_counter() - self.wall0
                if since >= self.next_sample:
                    self.next_sample += 1.0
                    self.log(f'wall {since:.1f} ' + ' || '.join(self.overlay_texts()))
                if self.shots and since >= self.shots[0]:
                    self.log(f'shot at {self.shots.pop(0):.1f}')
                    unreal.SystemLibrary.execute_console_command(self.game_world(), 'shot showui')
                if since > self.END_AFTER:
                    self.log('end')
                    self.finish()
            if self.t > 240.0:
                self.log('timeout')
                self.finish()
        except Exception as error:  # noqa
            self.log(f'ERROR {self.phase}: {error!r}')
            self.finish()


_check = TourSubtitleCheck()
_check.handle = unreal.register_slate_post_tick_callback(_check.tick)
