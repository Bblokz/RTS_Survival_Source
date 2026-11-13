"""Lit look captures + collision check of the Forge survey station, in a throwaway editor on a unit-test map
(never the shared editor, never Ger_Berlin). Nothing is saved.

Launch (-ExecCmds, so the editor ticks before the captures; one-shot lit captures are black otherwise):
    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
        -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall
        -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/forge_station_look_capture.py"
        -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -log=ForgeStationLook.log

FS_LOOK_TEMP=1 (default): imports the FBX into /Game/__ForgeStationPreview in memory only, so it works while another
editor locks the real asset. FS_LOOK_TEMP=0: uses the saved /Game/.../ForgeStation/SM_Berlin_ForgeStation.
Writes Saved/Berlin/forge_style/look/<shot>.png and look_report.json, then quits.
"""
import json
import math
import os
import time
import traceback

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/forge_style/look'
TEMP = os.environ.get('FS_LOOK_TEMP', '1') == '1'
EV = 0.6
SUN, SKY, FILL = 7.0, 3.0, 1.6
GROUND_RGB = (0.045, 0.05, 0.048)     # dark blast glass, as on the crater rim
WAIT_S = float(os.environ.get('FS_LOOK_WAIT', '100'))
RES = (1600, 1000)
SPOT = unreal.Vector(0.0, 0.0, 30000.0)   # well above the test map's own content
os.makedirs(OUT, exist_ok=True)

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = {'log': [], 'shots': {}, 'traces': {}}


def log(msg):
    print('FSLOOK', msg)
    report['log'].append(str(msg))


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def orbit(target, yaw_deg, elev_deg, dist):
    y, e = math.radians(yaw_deg), math.radians(elev_deg)
    return target + unreal.Vector(math.cos(y) * math.cos(e), math.sin(y) * math.cos(e), math.sin(e)) * dist


def shots(o):
    """label, eye, target, fov. UE frame: +X crater, Y mirrored from Blender (cryo block at -X, +Y)."""
    c = o + unreal.Vector(0, 0, 250)
    out = [
        ('rts_a', orbit(c, 125, 50, 5200), c, 45),      # from the west/south side (cryo block in front)
        ('rts_b', orbit(c, -50, 48, 5200), c, 45),      # from the east/north side (receivers, sample bunker)
        ('rts_c', orbit(c, -125, 52, 5200), c, 45),
        ('rts_d', orbit(c, 50, 52, 5200), c, 45),
        ('rts_far', orbit(c, 150, 55, 8200), c, 45),
        ('top', orbit(c, 90, 89.5, 6500), c, 40),
    ]
    gate = o + unreal.Vector(325, 590, 420)
    out.append(('gate', orbit(gate, 92, 8, 2600), gate, 50))
    roofs = o + unreal.Vector(-850, 150, 450)
    out.append(('roofs', orbit(roofs, 150, 40, 2200), roofs, 50))
    rig = o + unreal.Vector(1150, 300, 450)
    out.append(('rig', orbit(rig, -35, 30, 2600), rig, 50))
    console = o + unreal.Vector(-350, -520, 200)
    out.append(('console', orbit(console, -40, 32, 1500), console, 50))
    return out


class Run:
    def __init__(self):
        self.t0 = time.time()
        self.phase = 'setup'

    def mesh(self):
        ns = {}
        exec(open(HERE + 'forge_station_import.py', encoding='utf-8').read(), ns)
        if TEMP:
            ns['HOME'] = '/Game/__ForgeStationPreview'
            res = ns['run'](save=False)
            report['import'] = res
            return unreal.load_asset(res['mesh'].split('.')[0])
        return unreal.load_asset(ns['HOME'] + '/' + ns['MESH'])

    def setup(self):
        w = world()
        unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 0')
        for a in actors.get_all_level_actors():
            for comp in a.get_components_by_class(unreal.LightComponentBase) + a.get_components_by_class(unreal.ExponentialHeightFogComponent):
                comp.set_visibility(False)
        mesh = self.mesh()
        self.origin = SPOT
        st = actors.spawn_actor_from_class(unreal.StaticMeshActor, SPOT)
        st.static_mesh_component.set_static_mesh(mesh)
        self.station = st
        box = mesh.get_bounding_box()
        report['bounds_cm'] = [[round(v, 1) for v in (box.min.x, box.min.y, box.min.z)], [round(v, 1) for v in (box.max.x, box.max.y, box.max.z)]]
        report['materials'] = [m.get_path_name() if m else None for m in st.static_mesh_component.get_materials()]
        top = SPOT + unreal.Vector(0, 0, 5000)
        sun = actors.spawn_actor_from_class(unreal.DirectionalLight, top, unreal.Rotator(roll=0, pitch=-40, yaw=0))
        sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sun.light_component.set_intensity(SUN)
        sky = actors.spawn_actor_from_class(unreal.SkyLight, top)
        sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sl = sky.light_component
        sl.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        sl.set_editor_property('cubemap', unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
        sl.set_editor_property('lower_hemisphere_is_black', False)
        sl.set_intensity(SKY)
        sl.recapture_sky()
        fill = actors.spawn_actor_from_class(unreal.DirectionalLight, top, unreal.Rotator(roll=0, pitch=-25, yaw=180))
        fill.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        fill.light_component.set_intensity(FILL)
        fill.light_component.set_cast_shadows(False)
        ground = actors.spawn_actor_from_class(unreal.StaticMeshActor, SPOT + unreal.Vector(0, 0, -1))
        gm = ground.static_mesh_component
        gm.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        ground.set_actor_scale3d(unreal.Vector(400, 400, 1))
        try:
            gm.create_dynamic_material_instance(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')).set_vector_parameter_value(
                'Color', unreal.LinearColor(*GROUND_RGB, 1.0))
        except Exception as exc:
            log('ground colour ' + repr(exc))
        # scale references: a medium tank in the lane (south of the gate) and a trooper on the console stand point
        cube = unreal.load_asset('/Engine/BasicShapes/Cube')
        for loc, scale in ((unreal.Vector(325, 1100, 75), unreal.Vector(3.0, 6.0, 1.5)), (unreal.Vector(325, 1050, 195), unreal.Vector(2.0, 2.2, 0.9)),
                           (unreal.Vector(-90, -520, 90), unreal.Vector(0.5, 0.5, 1.8))):
            ref = actors.spawn_actor_from_class(unreal.StaticMeshActor, SPOT + loc)
            ref.static_mesh_component.set_static_mesh(cube)
            ref.set_actor_scale3d(scale)
            try:
                ref.static_mesh_component.create_dynamic_material_instance(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')).set_vector_parameter_value(
                    'Color', unreal.LinearColor(0.2, 0.24, 0.3, 1.0))
            except Exception:
                pass
            ref.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        cam = actors.spawn_actor_from_class(unreal.SceneCapture2D, top)
        comp = cam.capture_component2d
        self.rt = unreal.RenderingLibrary.create_render_target2d(w, RES[0], RES[1], unreal.TextureRenderTargetFormat.RTF_RGBA8)
        comp.texture_target = self.rt
        comp.capture_source = unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
        pps = comp.get_editor_property('post_process_settings')
        for k, v in (('override_auto_exposure_min_brightness', True), ('override_auto_exposure_max_brightness', True),
                     ('auto_exposure_min_brightness', EV), ('auto_exposure_max_brightness', EV)):
            pps.set_editor_property(k, v)
        comp.set_editor_property('post_process_settings', pps)
        comp.set_editor_property('post_process_blend_weight', 1.0)
        self.sun, self.sky, self.fill, self.cam = sun, sky, fill, cam
        self.setup_time = time.time()
        self.wait_until = time.time() + WAIT_S
        self.phase = 'wait'
        log('setup done')

    def traces(self):
        """Simple-collision line traces (the hulls): the lane must be free, the blocks must stop a trace."""
        w = world()
        o = self.origin

        def hit(a, b):
            res = unreal.SystemLibrary.line_trace_single(w, o + unreal.Vector(*a), o + unreal.Vector(*b), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,
                                                         False, [], unreal.DrawDebugTrace.NONE, True)
            if not res:
                return None
            t = res.to_tuple()
            actor = next((x for x in t if isinstance(x, unreal.Actor)), None)
            loc = next((x for x in t if isinstance(x, unreal.Vector)), None)
            return {'actor': actor.get_actor_label() if actor else None, 'at': [round(loc.x - o.x), round(loc.y - o.y), round(loc.z - o.z)] if loc else None}

        t = report['traces']
        for x in (-30, 150, 325, 500, 680):
            for z in (60, 250, 440):
                t[f'lane x{x} z{z}'] = hit((x, -1500, z), (x, 1500, z))
        t['through gate piers y590 z200'] = hit((-1600, 590, 200), (1600, 590, 200))
        t['command bunker y-520 z150 from west'] = hit((-1600, -520, 150), (-200, -520, 150))
        t['console stand from above'] = hit((-90, -520, 1200), (-90, -520, 5))
        t['receivers y-480 z150 from east'] = hit((1600, -480, 150), (720, -480, 150))
        t['down on arch crown'] = hit((325, 590, 2000), (325, 590, 5))
        report['lane_clear'] = all(v is None for k, v in t.items() if k.startswith('lane '))
        log('lane_clear ' + str(report['lane_clear']))

    def brightness(self):
        try:
            vals = []
            for i in range(8):
                for j in range(8):
                    c = unreal.RenderingLibrary.read_render_target_raw_uv(world(), self.rt, (i + 0.5) / 8, (j + 0.5) / 8)
                    vals.append((c.r + c.g + c.b) / 3)
            return round(sum(vals) / len(vals), 3)
        except Exception as exc:
            return repr(exc)

    def expose(self):
        """The test map's own post process leaves the capture far too bright: step the exposure bias down on the first
        shot until its mean sits in a normal range."""
        comp = self.cam.capture_component2d
        label, eye, target, fov = shots(self.origin)[0]
        self.cam.set_actor_location(eye, False, False)
        rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
        self.cam.set_actor_rotation(rot, False)
        self.sun.set_actor_rotation(unreal.Rotator(roll=0, pitch=-42, yaw=rot.yaw + 35), False)
        comp.fov_angle = fov
        tried = {}
        for bias in (0.0, -1.0, -2.0, -3.0, -4.0, -5.0, -6.0, -7.0, -8.0):
            pps = comp.get_editor_property('post_process_settings')
            pps.set_editor_property('override_auto_exposure_bias', True)
            pps.set_editor_property('auto_exposure_bias', bias)
            pps.set_editor_property('override_bloom_intensity', True)
            pps.set_editor_property('bloom_intensity', 0.2)
            comp.set_editor_property('post_process_settings', pps)
            for _ in range(3):
                comp.capture_scene()
            mean = self.brightness()
            tried[bias] = mean
            if isinstance(mean, float) and mean < 105:
                break
        report['exposure_bias'] = tried
        log('exposure ' + json.dumps(tried))

    def capture(self):
        self.expose()
        comp = self.cam.capture_component2d
        for label, eye, target, fov in shots(self.origin):
            self.cam.set_actor_location(eye, False, False)
            rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
            self.cam.set_actor_rotation(rot, False)
            self.sun.set_actor_rotation(unreal.Rotator(roll=0, pitch=-42, yaw=rot.yaw + 35), False)
            self.fill.set_actor_rotation(unreal.Rotator(roll=0, pitch=-20, yaw=rot.yaw + 180), False)
            comp.fov_angle = fov
            for _ in range(3):
                comp.capture_scene()
            unreal.RenderingLibrary.export_render_target(world(), self.rt, OUT, f'{label}.png')
            report['shots'][label] = self.brightness()
        log('captured ' + json.dumps(report['shots']))

    def tick(self, _dt):
        try:
            if self.phase == 'setup':
                if time.time() - self.t0 < 20:
                    return
                self.phase = 'busy'      # spawning pumps Slate, which re-enters this callback
                self.setup()
            elif self.phase == 'wait':
                if not getattr(self, 'recaptured', False) and time.time() > self.setup_time + 8:
                    self.sky.light_component.recapture_sky()
                    self.recaptured = True
                if time.time() >= self.wait_until:
                    self.phase = 'busy'
                    self.traces()
                    self.capture()
                    self.finish()
        except Exception as exc:     # never leave a throwaway editor hanging
            log('FAIL ' + repr(exc) + traceback.format_exc())
            self.finish()

    def finish(self):
        self.phase = 'done'
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.execute_console_command(world(), 'r.TextureStreaming 1')
        with open(OUT + '/look_report.json', 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=1)
        log('done')
        unreal.SystemLibrary.quit_editor()


RUN = Run()
HANDLE = unreal.register_slate_post_tick_callback(RUN.tick)
