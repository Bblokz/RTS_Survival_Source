# Editor-world check of the fuel depot's editor-only node visuals (they never exist in -game or commandlet runs).
# Throwaway DebugGame editor, nothing saved:
#   UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
#       -ExecCmds="py <this file>" -DisablePlugins=Fab -NoLiveCoding -unattended  (-ExecutePythonScript quits first)
# Places Rus_BXP_LongCamoBunker_FuelDepot, checks each node has its coloured arrow + label, moves one node on the
# instance, then the actor, and checks the node kept its instance offset. Takes a screenshot over the depot, then quits.
# Logs RTS_FUELDEPOT_EDITOR lines.
import unreal

BP_CLASS = ('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/'
            'Rus_BXP_LongCamoBunker_FuelDepot.Rus_BXP_LongCamoBunker_FuelDepot_C')
LOCATION = unreal.Vector(16796.0, -15808.0, 150.0)
OUT = unreal.Paths.project_saved_dir() + 'FuelDepotTest/editor_nodes.png'
NODE_MOVE = unreal.Vector(0.0, -800.0, 0.0)
ACTOR_MOVE = unreal.Vector(600.0, 0.0, 0.0)

state = {'frame': 0, 'failures': [], 'actor': None, 'camera': None}


def log(text):
    unreal.log_warning('RTS_FUELDEPOT_EDITOR ' + text)


def fail(reason):
    state['failures'].append(reason)
    log('FAIL ' + reason)


def node_components(actor):
    return {c.get_name(): c for c in actor.get_components_by_class(unreal.FuelDeliveryNodeComponent)}


def check_visuals(actor):
    nodes = node_components(actor)
    arrows = actor.get_components_by_class(unreal.ArrowComponent)
    labels = actor.get_components_by_class(unreal.TextRenderComponent)
    log('nodes=%s arrows=%d labels=%d' % (sorted(nodes), len(arrows), len(labels)))
    if len(nodes) != 3:
        fail('expected 3 nodes')
    for arrow in arrows:
        parent = arrow.get_attach_parent()
        colour = arrow.get_editor_property('arrow_color')
        log('arrow on %s colour=(%d,%d,%d) visible=%s visible_flag=%s parent_visible_flag=%s hidden_in_game=%s '
            'editor_only=%s' % (
            parent.get_name() if parent else None, colour.r, colour.g, colour.b, arrow.is_visible(),
            arrow.get_editor_property('visible'), parent.get_editor_property('visible') if parent else None,
            arrow.get_editor_property('hidden_in_game'), arrow.get_editor_property('is_editor_only')))
    for label in labels:
        parent = label.get_attach_parent()
        log('label on %s text=%s' % (parent.get_name() if parent else None, label.get_editor_property('text')))
    node_arrows = [a for a in arrows if a.get_attach_parent() and
                   isinstance(a.get_attach_parent(), unreal.FuelDeliveryNodeComponent)]
    if len(node_arrows) != 3:
        fail('expected one arrow per node, got %d' % len(node_arrows))
    if len([l for l in labels if isinstance(l.get_attach_parent(), unreal.FuelDeliveryNodeComponent)]) != 3:
        fail('expected one label per node')


def check_instance_move(actor):
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    nodes = node_components(actor)
    spawn = nodes.get('FuelTruckSpawnNode')
    if spawn is None:
        fail('no FuelTruckSpawnNode on the instance')
        return
    moved_relative = spawn.get_editor_property('relative_location') + NODE_MOVE
    spawn.set_editor_property('relative_location', moved_relative)
    actor.set_actor_location(actor.get_actor_location() + ACTOR_MOVE, False, False)
    spawn_after = node_components(actor).get('FuelTruckSpawnNode')
    relative_after = spawn_after.get_editor_property('relative_location')
    expected_world = actor.get_actor_transform().transform_location(moved_relative)
    world_after = spawn_after.get_world_location()
    log('instance node relative %s after move+rerun %s world %s expected %s' % (
        moved_relative, relative_after, world_after, expected_world))
    if (relative_after - moved_relative).length() > 1.0:
        fail('instance node offset lost after rerunning construction')
    if (world_after - expected_world).length() > 1.0:
        fail('node did not follow the actor')
    # Visuals still there after the rerun.
    check_visuals(actor)
    del actor_subsystem


def setup():
    actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    depot_class = unreal.load_class(None, BP_CLASS)
    actor = actor_subsystem.spawn_actor_from_class(depot_class, LOCATION, unreal.Rotator(0.0, 0.0, 0.0))
    if actor is None:
        fail('spawn failed')
        return
    state['actor'] = actor
    log('spawned %s' % actor.get_name())
    check_visuals(actor)
    check_instance_move(actor)
    # Camera high over the depot, looking down at the whole node triangle (spawn node is ~35 m south).
    focus = actor.get_actor_location() + unreal.Vector(0.0, -1800.0, 0.0)
    camera = actor_subsystem.spawn_actor_from_class(unreal.CameraActor, focus + unreal.Vector(-3200.0, 0.0, 5200.0),
                                                    unreal.Rotator(0.0, -58.0, 0.0))
    camera.get_editor_property('camera_component').set_editor_property('field_of_view', 70.0)
    state['camera'] = camera
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        camera.get_actor_location(), camera.get_actor_rotation())


def tick(delta_seconds):
    state['frame'] += 1
    frame = state['frame']
    if frame == 30:
        try:
            setup()
        except Exception as error:  # a broken check must fail the run, not pass it
            fail('exception %s' % error)
    elif frame == 120 and state['camera'] is not None:
        unreal.AutomationLibrary.take_high_res_screenshot(1600, 900, OUT, state['camera'])
        log('screenshot requested %s' % OUT)
    elif frame == 260:
        log('RESULT %s failures=%s' % ('PASS' if not state['failures'] else 'FAIL', state['failures']))
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()


handle = unreal.register_slate_post_tick_callback(tick)
