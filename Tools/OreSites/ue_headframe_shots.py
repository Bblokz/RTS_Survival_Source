"""Fresh-process check of BP_OreBaseB_Headframe plus look shots, headless (needs rendering, no -nullrhi):

  sh run_ue_py.sh ue_headframe_shots.py <log> -AllowCommandletRendering

Reloads the child Blueprint, logs its overrides and the parent's, spawns it into the commandlet's transient editor
world next to the guard tower it replaces (ground plane, sun, sky light), renders scene captures from the default RTS
camera angle and from the south-east to Saved/OreMountains/sites/models/ue_shots/, destroys everything, saves nothing.
"""
import math
import os

import unreal

HOME = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/Sites/BaseB'
BP = HOME + '/BP_OreBaseB_Headframe'
PARENT = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_LargeGuardTower'
LOOKOUT = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/LookoutTowers/BP_OreLookoutTower'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/models/ue_shots'
SPOT = unreal.Vector(0, 400000, 20000)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def log(*a):
    unreal.log_warning('ORE_HF ' + ' '.join(str(x) for x in a))


def spawn_class(path, loc, yaw=0.0):
    bp = unreal.load_asset(path)
    if bp is None:
        log('missing', path)
        return None
    return actors.spawn_actor_from_class(bp.generated_class(), loc, unreal.Rotator(0, 0, yaw))


def main():
    os.makedirs(OUT, exist_ok=True)
    spawned = []
    unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 0')
    try:
        plane = actors.spawn_actor_from_class(unreal.StaticMeshActor, SPOT)
        plane.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        plane.set_actor_scale3d(unreal.Vector(400, 400, 1))
        mid = plane.static_mesh_component.create_dynamic_material_instance(
            0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial'))
        mid.set_vector_parameter_value('Color', unreal.LinearColor(0.20, 0.15, 0.09, 1))
        spawned.append(plane)
        # review light from the south-south-east (the map's NNE light leaves the camera-facing south faces in shade)
        sun = actors.spawn_actor_from_class(unreal.DirectionalLight, SPOT, unreal.Rotator(roll=0, pitch=-42, yaw=-35))
        sun.light_component.set_intensity(7)
        sun.light_component.set_editor_property('atmosphere_sun_light', True)
        spawned.append(sun)
        atmo = actors.spawn_actor_from_class(unreal.SkyAtmosphere, SPOT)
        spawned.append(atmo)
        sky = actors.spawn_actor_from_class(unreal.SkyLight, SPOT)
        sky.light_component.set_editor_property('real_time_capture', True)
        sky.light_component.set_intensity(1.6)
        sky.light_component.recapture_sky()
        spawned.append(sky)
        hf = spawn_class(BP, SPOT)
        spawned.append(hf)
        old = spawn_class(PARENT, SPOT + unreal.Vector(0, -3200, 0))
        spawned.append(old)
        look = spawn_class(LOOKOUT, SPOT + unreal.Vector(0, 3000, 0))
        spawned.append(look)
        for a in (hf, old, look):
            if a is None:
                continue
            o, e = a.get_actor_bounds(False)
            log('bounds', a.get_class().get_name(), 'z_top=%.0f' % (o.z + e.z - SPOT.z), 'extent', e)
        if hf is not None:
            comps = hf.get_components_by_class(unreal.ActorComponent)
            for c in comps:
                d = c.get_class().get_name()
                if isinstance(c, unreal.SceneComponent):
                    l = c.get_relative_location() if hasattr(c, 'get_relative_location') else c.get_editor_property('relative_location')
                    d += ' rel=(%.0f,%.0f,%.0f)' % (l.x, l.y, l.z)
                if isinstance(c, unreal.StaticMeshComponent) and c.static_mesh:
                    d += ' mesh=' + c.static_mesh.get_name() + ' mats=' + ','.join(
                        m.get_name() if m else 'None' for m in c.get_materials())
                if isinstance(c, unreal.StaticMeshComponent):
                    d += ' collision=%s nav=%s' % (c.get_collision_profile_name(), c.get_editor_property('can_ever_affect_navigation'))
                if isinstance(c, unreal.WallHoleDamageComponent):
                    d += ' damaged=' + ','.join(m.get_name() if m else 'None' for m in c.get_editor_property('damaged_materials'))
                if isinstance(c, unreal.HealthComponent):
                    d += ' widget_offset=%s' % c.get_editor_property('relative_widget_offset')
                if isinstance(c, unreal.ChildActorComponent):
                    ca = c.get_editor_property('child_actor')
                    d += ' child=' + (ca.get_class().get_name() if ca else 'None')
                log('comp', c.get_name(), d)
        rt = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8)
        cam = actors.spawn_actor_from_class(unreal.SceneCapture2D, SPOT)
        spawned.append(cam)
        comp = cam.capture_component2d
        comp.texture_target = rt
        comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        unreal.SystemLibrary.execute_console_command(world, 'FlushRenderingCommands')

        def shot(name, eye, target, fov):
            cam.set_actor_location(eye, False, False)
            cam.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(eye, target), False)
            comp.fov_angle = fov
            for _ in range(2):
                comp.capture_scene()
            unreal.RenderingLibrary.export_render_target(world, rt, OUT, name + '.png')
            log('shot', name)

        # default RTS camera: 40 m over the site, pitch -55, looking NNW (X north, Y east)
        tgt = SPOT + unreal.Vector(-200, 100, 0)
        yaw, pitch = math.radians(-21.0), math.radians(55.0)
        dist = 4000.0 / math.sin(pitch)
        fwd = unreal.Vector(math.cos(yaw) * math.cos(pitch), math.sin(yaw) * math.cos(pitch), -math.sin(pitch))
        shot('game_view', tgt - fwd * dist, tgt, 90.0)
        shot('southeast', SPOT + unreal.Vector(-5200, 4000, 600), SPOT + unreal.Vector(0, -300, 900), 55.0)
        shot('southwest_close', SPOT + unreal.Vector(-2400, -2200, 1300), SPOT + unreal.Vector(0, 400, 1000), 60.0)
        shot('bunker_roof', SPOT + unreal.Vector(-1500, 1700, 2000), SPOT + unreal.Vector(-100, 850, 1200), 55.0)
        comp.capture_source = unreal.SceneCaptureSource.SCS_BASE_COLOR
        shot('southeast_basecolor', SPOT + unreal.Vector(-5200, 4000, 600), SPOT + unreal.Vector(0, -300, 900), 55.0)
    finally:
        unreal.SystemLibrary.execute_console_command(world, 'r.TextureStreaming 1')
        for a in spawned:
            if a is not None:
                actors.destroy_actor(a)
    log('RESULT done')


main()
