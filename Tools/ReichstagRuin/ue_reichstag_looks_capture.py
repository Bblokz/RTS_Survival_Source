"""Lit look captures of the Reichstag ruin options, in a throwaway editor on UT_ReichstagRuin (never the shared editor).

Launch (-ExecCmds, not -ExecutePythonScript: the latter runs before the editor ever ticks, and one-shot lit captures then
come out black):
    UnrealEditor-Win64-DebugGame.exe RTS_Survival.uproject
        -ini:Engine:[/Script/EngineSettings.GameMapsSettings]:EditorStartupMap=/Game/RTS_Survival/Maps/UnitTests/UT_ReichstagRuin
        -ExecCmds="py D:/UE5Projects/RTS_Survival/Source/Tools/ReichstagRuin/ue_reichstag_looks_capture.py"
        -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -log=ReichstagLooks.log

A slate tick callback drives it: build the option instances (ue_reichstag_looks.py), light the map with a movable sun and
a daylight-cubemap sky (the map's own lights and fog are switched off), wait for shaders, then per option swap the
materials onto the placed ruin's ISM slots and capture every shot at a fixed exposure. Writes
Saved/ReichstagRuin/Looks/<option>_<shot>.png plus looks_report.json, then quits. Nothing is saved to the map.
"""
import json
import math
import os
import time

import unreal

HERE = 'D:/UE5Projects/RTS_Survival/Source/Tools/ReichstagRuin/'
OUT = 'D:/UE5Projects/RTS_Survival/Saved/ReichstagRuin/Looks'
RUIN_CLASS = 'BP_ReichstagRuin_B2'
EV = 0.6
SUN, SKY, FILL = 7.0, 3.0, 1.6   # FILL: shadowless bounce stand-in, so shadows are not black
GROUND_RGB = (0.16, 0.15, 0.14)  # neutral dusty grey under the ruin, in place of the test map's red desert
FIRST_WAIT_S = float(os.environ.get('RT_FIRST_WAIT', globals().get('RT_FIRST_WAIT', 120.0)))   # shader permutations of the new instances
OPTION_WAIT_S = 25.0    # streaming + Lumen settle after a swap
RES = (1600, 1000)
os.makedirs(OUT, exist_ok=True)

# RT_LOOKS_FINAL=1 in the environment: one pass named Final with the meshes' own slot materials (after the pick)
FINAL = os.environ.get('RT_LOOKS_FINAL') == '1'
ns = {'RT_LOOKS_BUILD': not FINAL}
exec(open(HERE + 'ue_reichstag_looks.py', encoding='utf-8').read(), ns)
OPTIONS = {'Final': {}} if FINAL else ns['OPTIONS']
LAYOUT = json.load(open(HERE + 'RT_Layout.json', encoding='utf-8'))

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = {'options': {}, 'log': []}


def log(msg):
    print('RTLOOKS', msg)
    report['log'].append(msg)


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ruin():
    for a in actors.get_all_level_actors():
        if RUIN_CLASS in a.get_class().get_name():
            return a
    raise RuntimeError('ruin actor not found')


def piece_xy(piece):
    rows = [r for r in LAYOUT['pieces'] if r['piece'] == piece]
    return rows[0]['location_cm'] if rows else (0, 0, 0)


def shots(origin):
    """label, eye, target (world cm)."""
    o = origin
    out = []

    def orbit(label, target, yaw_deg, elev_deg, dist, fov):
        y, e = math.radians(yaw_deg), math.radians(elev_deg)
        eye = target + unreal.Vector(math.cos(y) * math.cos(e), math.sin(y) * math.cos(e), math.sin(e)) * dist
        out.append((label, eye, target, fov))

    # game: the RTS camera's pitch, looking from the east (UE -Y) and a little south (+X)
    orbit('game', o + unreal.Vector(0, 0, 400), -70, 52, 15500, 45)
    # west front: the portico (UE +Y) at a low angle
    orbit('portico', o + unreal.Vector(0, 3600, 700), 95, 14, 6200, 45)
    # the tall tower (Director post) from outside its corner
    tx, ty, _ = piece_xy('SM_RT_Tower_Tall')
    t = o + unreal.Vector(tx, ty, 1300)
    orbit('tower', t, math.degrees(math.atan2(ty, tx)), 22, 5200, 45)
    # inside the hall, over the factory roof towards the dome ribs
    orbit('hall', o + unreal.Vector(0, 0, 900), -100, 38, 4600, 50)
    if os.environ.get('RT_LOOKS_DECALS') == '1':     # close-ups of single decals from the plan
        plan = json.load(open(HERE + 'RT_Decals.json', encoding='utf-8'))['decals']
        for key, label, dist in ((11, 'chalk', 900), (14, 'star', 1100), ('inscription', 'frieze', 2600), (0, 'soot', 1800)):
            d = next((d for d in plan if d['cell'] == key), None)
            if d:
                t = o + unreal.Vector(*d['location_cm'])
                n = d['normal']
                out.append((label, t + unreal.Vector(n[0], n[1], 0.25) * dist, t, 45))
    return out


class Run:
    def __init__(self):
        self.t0 = time.time()
        self.phase = 'setup'
        self.queue = list(OPTIONS)
        self.wait_until = 0.0
        self.rig = []

    def setup(self):
        w = world()
        unreal.SystemLibrary.execute_console_command(w, 'r.TextureStreaming 0')
        for a in actors.get_all_level_actors():
            for comp in a.get_components_by_class(unreal.LightComponentBase) + a.get_components_by_class(unreal.ExponentialHeightFogComponent):
                comp.set_visibility(False)
        self.ruin = ruin()
        self.origin = self.ruin.get_actor_location()
        log(f'ruin at {self.origin}')
        spot = self.origin + unreal.Vector(0, 0, 5000)
        sun = actors.spawn_actor_from_class(unreal.DirectionalLight, spot, unreal.Rotator(roll=0, pitch=-40, yaw=0))
        sun.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sun.light_component.set_intensity(SUN)
        sky = actors.spawn_actor_from_class(unreal.SkyLight, spot)
        sky.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        sl = sky.light_component
        sl.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        sl.set_editor_property('cubemap', unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
        sl.set_editor_property('lower_hemisphere_is_black', False)
        sl.set_intensity(SKY)
        sl.recapture_sky()
        fill = actors.spawn_actor_from_class(unreal.DirectionalLight, spot, unreal.Rotator(roll=0, pitch=-25, yaw=180))
        fill.root_component.set_mobility(unreal.ComponentMobility.MOVABLE)
        fill.light_component.set_intensity(FILL)
        fill.light_component.set_cast_shadows(False)
        ground = actors.spawn_actor_from_class(unreal.StaticMeshActor, self.origin + unreal.Vector(0, 0, 4))
        gm = ground.static_mesh_component
        gm.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Plane'))
        ground.set_actor_scale3d(unreal.Vector(500, 500, 1))
        try:
            gm.create_dynamic_material_instance(0, unreal.load_asset('/Engine/BasicShapes/BasicShapeMaterial')).set_vector_parameter_value(
                'Color', unreal.LinearColor(*GROUND_RGB, 1.0))
        except Exception as exc:
            log('ground colour ' + repr(exc))
        self.sky = sky
        cam = actors.spawn_actor_from_class(unreal.SceneCapture2D, spot)
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
        self.sun, self.cam = sun, cam
        self.rig = [sun, sky, cam, fill, ground]
        self.fill = fill
        # load every option once so all permutations start compiling now
        for slots in OPTIONS.values():
            for path in slots.values():
                unreal.load_asset(path)
        if os.environ.get('RT_DECALS_REBUILD') == '1':   # rebuild the decals in memory (the saved Blueprint may lag)
            exec(open(HERE + 'ue_reichstag_decals.py', encoding='utf-8').read(), {'RT_DECALS_ONLY': True, 'RT_DECALS_NOSAVE': True})
            self.ruin = ruin()
        log(f"{len(self.ruin.get_components_by_class(unreal.DecalComponent))} decal components")
        self.isms = [c for c in self.ruin.get_components_by_class(unreal.InstancedStaticMeshComponent)]
        log(f'{len(self.isms)} ISM components')
        self.apply(self.queue[0])
        self.setup_time = time.time()
        self.wait_until = time.time() + FIRST_WAIT_S
        self.phase = 'wait'

    def apply(self, option):
        if not OPTIONS[option]:
            log(f'{option}: mesh materials')
            return
        slots = {k: unreal.load_asset(v) for k, v in OPTIONS[option].items()}
        n = 0
        for ism in self.isms:
            mesh = ism.get_editor_property('static_mesh')
            for i, s in enumerate(mesh.get_editor_property('static_materials')):
                name = str(s.get_editor_property('material_slot_name'))
                for key, mat in slots.items():
                    if name.startswith(key):
                        ism.set_material(i, mat)
                        n += 1
        log(f'{option}: {n} slots set')

    def brightness(self):
        try:
            return self._brightness()
        except Exception as exc:
            return repr(exc)

    def _brightness(self):
        w = world()
        vals = []
        for i in range(8):
            for j in range(8):
                c = unreal.RenderingLibrary.read_render_target_raw_uv(w, self.rt, (i + 0.5) / 8, (j + 0.5) / 8)
                vals.append((c.r + c.g + c.b) / 3)
        return round(sum(vals) / len(vals), 3)

    def capture(self, option):
        w = world()
        comp = self.cam.capture_component2d
        out = {}
        for label, eye, target, fov in shots(self.origin):
            self.cam.set_actor_location(eye, False, False)
            rot = unreal.MathLibrary.find_look_at_rotation(eye, target)
            self.cam.set_actor_rotation(rot, False)
            self.sun.set_actor_rotation(unreal.Rotator(roll=0, pitch=-40, yaw=rot.yaw + 35), False)
            self.fill.set_actor_rotation(unreal.Rotator(roll=0, pitch=-20, yaw=rot.yaw + 180), False)
            comp.fov_angle = fov
            for _ in range(3):
                comp.capture_scene()
            unreal.RenderingLibrary.export_render_target(w, self.rt, OUT, f'{option}_{label}.png')
            out[label] = self.brightness()
        report['options'][option] = out
        log(f'{option} captured, mean {out}')
        if os.environ.get('RT_LOOKS_DECALS') == '1' and not option.endswith('_nodecals'):   # same shots without decals
            decals = self.ruin.get_components_by_class(unreal.DecalComponent)
            for dc in decals:
                dc.set_visibility(False)
            self.capture(option + '_nodecals')
            for dc in decals:
                dc.set_visibility(True)

    def tick(self, _dt):
        try:
            if self.phase == 'setup':
                if time.time() - self.t0 < 20:      # let the level finish loading
                    return
                self.setup()
            elif self.phase == 'wait':
                if not getattr(self, 'recaptured', False) and time.time() > self.setup_time + 8:
                    self.sky.light_component.recapture_sky()     # again once the level has ticked
                    self.recaptured = True
                if time.time() >= self.wait_until:
                    self.phase = 'capture'
            elif self.phase == 'capture':
                option = self.queue.pop(0)
                self.capture(option)
                if self.queue:
                    self.apply(self.queue[0])
                    self.wait_until = time.time() + OPTION_WAIT_S
                    self.phase = 'wait'
                else:
                    self.finish()
        except Exception as exc:     # never leave a throwaway editor hanging
            import traceback
            log('FAIL ' + repr(exc) + traceback.format_exc())
            self.finish()

    def finish(self):
        self.phase = 'done'
        unreal.unregister_slate_post_tick_callback(HANDLE)
        unreal.SystemLibrary.execute_console_command(world(), 'r.TextureStreaming 1')
        with open(OUT + '/looks_report.json', 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=1)
        log('done')
        unreal.SystemLibrary.quit_editor()


RUN = Run()
HANDLE = unreal.register_slate_post_tick_callback(RUN.tick)
