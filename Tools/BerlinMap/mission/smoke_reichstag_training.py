"""Headless smoke check of the Reichstag training (-game run of Ger_Berlin, -ExecCmds="slomo 3, py <this file>").
A bare -game launch never starts the mission stages (they wait for the main game UI), so this calls the subsystem
directly: StartReichstagTraining(DA_Berlin_ReichstagTraining, Normal) after 5 s of game time, the attack phase at
ATTACK_AT game seconds (target = the PlayerStart), and logs RT_SMOKE status lines. Grep the log for RT_SMOKE, RT_TRAIN
and AI_TRAIN.
"""
import unreal

DATA = '/Game/RTS_Survival/Maps/GerMissions/Berlin/DataAssets/DA_Berlin_ReichstagTraining'
START_AT = 5.0
ATTACK_AT = 480.0
STATUS_EVERY = 60.0
S = {'started': False, 'attack': False, 'next_status': 30.0, 'handle': None}


def log(text):
    unreal.log_warning('RT_SMOKE ' + str(text))


def game_world():
    # A standalone -game process has no editor subsystems: take the loaded Ger_Berlin world that has actors.
    if S.get('world') is not None:
        return S['world']
    for w in unreal.ObjectIterator(unreal.World):
        if w.get_name() == 'Ger_Berlin' and unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart):
            S['world'] = w
            log(f'world {w.get_path_name()}')
            return w
    return None


def normal_difficulty():
    # The FRTSGameDifficulty struct shadows the enum's Python name: take the enum type from the struct's field.
    value = unreal.RTSGameDifficulty().get_editor_property('difficulty_level')
    return type(value).NORMAL


def soviet_units(w):
    """Trained Soviet units alive (the map has no other mobile Soviet units before the stages run)."""
    rows = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(w, unreal.Actor):
        name = a.get_class().get_name()
        if not (name.startswith('Rus_Squad_') or name.startswith('Rus_Vehicle_')):
            continue
        p = a.get_actor_location()
        rows.append(f'{a.get_name()}@({p.x:.0f},{p.y:.0f})')
    return f'{len(rows)} [' + ' '.join(sorted(rows)) + ']'


def tick(_dt):
    w = game_world()
    if w is None:
        return
    if unreal.GameplayStatics.is_game_paused(w):
        # A bare -game launch waits paused for the main game UI that never comes; the AI needs game time.
        unreal.GameplayStatics.set_game_paused(w, False)
        log(f'unpaused at real {unreal.GameplayStatics.get_real_time_seconds(w):.0f}')
    t = unreal.GameplayStatics.get_time_seconds(w)
    if not S.get('first_t_logged'):
        S['first_t_logged'] = True
        log(f'first tick game time {t:.1f}, dilation {unreal.GameplayStatics.get_global_time_dilation(w)}')
    sub =unreal.BerlinReichstagTrainingSubsystem.get_berlin_reichstag_training_subsystem(w)
    if not S['started'] and t >= START_AT:
        S['started'] = True
        ok = sub.start_reichstag_training(unreal.load_asset(DATA), normal_difficulty())
        log(f't={t:.0f} start_reichstag_training -> {ok}')
    if S['started'] and not S['attack'] and t >= ATTACK_AT:
        S['attack'] = True
        starts = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.PlayerStart)
        target = starts[0] if starts else None
        sub.start_attacks_on_player_base(target)
        log(f't={t:.0f} attack phase, target {target.get_actor_location() if target else None}')
    if t >= S['next_status']:
        S['next_status'] = t + STATUS_EVERY
        log(f't={t:.0f} patrols {sub.get_active_patrol_count()} attack_phase {sub.get_is_attack_phase()} '
            f'soviet units {soviet_units(w)}')


S['handle'] = unreal.register_slate_post_tick_callback(tick)
log('smoke script armed')
