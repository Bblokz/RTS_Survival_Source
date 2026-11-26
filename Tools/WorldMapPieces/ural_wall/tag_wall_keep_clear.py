"""Tags the wall splines and the gate in RTS_WorldCampaign with WorldAnchorKeepClear and saves the level.

The campaign generator keeps generated anchors M_KeepClearDistance away from actors with that tag, so no map piece
stands in the wall. Run in a throwaway editor on the level (see run_tag.ps1).
"""
import time

import unreal

TAG = "WorldAnchorKeepClear"
WALL_MESH_PREFIX = "SM_WorldWall_UralShield"
EXPECTED_ACTORS = 3


def log(message):
    unreal.log("URALWALL " + str(message))


def wall_mesh(actor):
    if actor.get_class().get_name() == "BP_RoadSpline_C":
        return actor.get_editor_property("road_mesh")
    if isinstance(actor, unreal.StaticMeshActor):
        return actor.static_mesh_component.get_editor_property("static_mesh")
    return None


def tag_actors():
    tagged = 0
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        mesh = wall_mesh(actor)
        if mesh is None or not mesh.get_name().startswith(WALL_MESH_PREFIX):
            continue
        tags = [str(tag) for tag in actor.tags]
        if TAG not in tags:
            actor.set_editor_property("tags", tags + [TAG])
        log("tagged %s: %s" % (actor.get_actor_label(), [str(tag) for tag in actor.tags]))
        tagged += 1
    return tagged


class Run:
    def __init__(self):
        self.start = time.time()
        self.phase = "wait"
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def tick(self, delta):
        now = time.time() - self.start
        if self.phase == "wait" and now > 25.0:
            self.phase = "quit"
            self.mark = now
            tagged = tag_actors()
            if tagged != EXPECTED_ACTORS:
                log("NOT saved: tagged %d actors, expected %d" % (tagged, EXPECTED_ACTORS))
                return
            world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
            log("saved level: %s" % unreal.EditorLoadingAndSavingUtils.save_packages([world.get_outermost()], False))
        elif self.phase == "quit" and now - self.mark > 4.0:
            self.phase = "done"
            unreal.unregister_slate_post_tick_callback(self.handle)
            unreal.SystemLibrary.quit_editor()


RUN = Run()
