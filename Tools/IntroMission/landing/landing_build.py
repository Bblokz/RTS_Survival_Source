"""Ger_IntroMission rear depot -> Soviet landing field (4 pads for the Pe-8 transport drops), 2026-11-17.

Run in the throwaway editor (landing_editor_server.py) through landing_cmd.py:
    python landing_cmd.py landing_build.py            # build_all(): clear old clutter + every LAND_* step
Every step clears and respawns its own LAND_<Step>_* actors (outliner folder IntroRear/LandingField/<Step>), so any
step can be re-run alone:  exec(open(BUILD).read()); step_lights()
Local frame (u, v): slab centre (18300, -15700), yaw 18. The slab (6 BP_Airport_Ground1 tiles, REAR_Depot_Tile_*) spans
u +-3600, v +-2400, top z 146. +v is the road D side (north-west).
"""
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/landing/landing_lib.py').read())
import json

ENV = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/'
FIELD = '/Game/RTS_Survival/Blueprints/Environment/IntroMission/LandingField/'
TOWER_BP = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_GuardTowerMG_Soviet'
FENCE_BP = ENV + 'Airport/BP_Airport_Dest_FenceWire'
GATE_FRAME = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/Gates/Meshes/SM_RusWallGateA_Frame'
FLOOD_MESH = '/Game/MilitaryAirport/Meshes/Props/SM_FloodLight_2'
LAMP_SHAPE = '/Engine/BasicShapes/Cylinder'
SPHERE_SHAPE = '/Engine/BasicShapes/Sphere'

SLAB_U, SLAB_V = 3600.0, 2400.0
PAD_U, PAD_V = 1850.0, 1250.0
# pad number -> local centre. The RTS camera looks along +u (world +X) with +v to the right, so this reads
# 1 2 (far row) / 3 4 (near row) on screen (review r1).
PADS = {1: (PAD_U, -PAD_V), 2: (PAD_U, PAD_V), 3: (-PAD_U, -PAD_V), 4: (-PAD_U, PAD_V)}
FENCE_U, FENCE_V = 4600.0, 3400.0
GATE_U = -1880.0            # where road D crosses the north fence line (v = +FENCE_V)
GATE_HALF = 475.0           # SM_RusWallGateA_Frame spans x -935..15 (950 cm)

OLD_CLUTTER = ('REAR_Depot_Prop_', 'REAR_Extra_DepotLight_', 'REAR_Extra_DepotBarrier_', 'REAR_Decal_Debris_')
OLD_SINGLES = ('REAR_Decal_Scatter_22', 'RTS_BP_NavBlocker_Long114', 'RTS_BP_NavBlocker_Long115',
               'REAR_Decal_Tracks_02')   # old rut decal by the old road end (r2: reads as a claw smear)


def slab_top():
    x, y = W(0, 0)
    z, a = ground_hit(x, y)
    return z


# ---- clear ----------------------------------------------------------------------------------------------------------
def step_clear():
    """The old depot clutter on and around the slab (the tiles stay), plus the stray probe aircraft."""
    n = 0
    for a in all_actors():
        lab = a.get_actor_label()
        p = a.get_actor_location()
        u, v = L(p.x, p.y)
        # rear-area detail scatter (stones, shrubs, crater stones) that ended up inside the fence or under the slab
        inside = abs(u) < FENCE_U + 150.0 and abs(v) < FENCE_V + 150.0
        if inside and lab.startswith(('REAR_Detail_Stone', 'REAR_Detail_Shrub', 'REAR_CraterStone', 'REAR_Decal_Tracks_04')):
            actors_sub().destroy_actor(a)
            n += 1
            continue
        if lab.startswith(OLD_CLUTTER) or lab in OLD_SINGLES or p.z < -150000:
            actors_sub().destroy_actor(a)
            n += 1
    log('cleared %d old depot actors' % n)


# ---- pads -----------------------------------------------------------------------------------------------------------
def step_pads():
    clear_prefix('LAND_Pad_')
    mesh = FIELD + 'SM_LandingPad_Soviet'
    if unreal.load_asset(mesh) is None:
        log('pads: %s not built yet, skipped' % mesh)
        return
    top = slab_top()
    for num, (u, v) in PADS.items():
        x, y = W(u, v)
        mi = FIELD + 'MI_LandingPad_Soviet_%d' % num
        spawn_mesh(mesh, 'LAND_Pad_%d' % num, (x, y, top - 1.0), YAW, 'Pads',
                   materials=None)
        a = [b for b in all_actors() if b.get_actor_label() == 'LAND_Pad_%d' % num][0]
        if unreal.load_asset(mi):
            comp = a.static_mesh_component
            for i, m in enumerate(comp.get_materials()):
                if m and 'LandingPad' in m.get_name() and 'Glass' not in m.get_name() and 'Lamp' not in m.get_name():
                    comp.set_material(i, unreal.load_asset(mi))
    log('pads placed')


# ---- fence ----------------------------------------------------------------------------------------------------------
def _fence_dims():
    """Actor-local X span of the fence BP's visible mesh (it runs along X)."""
    cls = unreal.load_class(None, FENCE_BP + '.' + FENCE_BP.split('/')[-1] + '_C')
    probe = actors_sub().spawn_actor_from_class(cls, unreal.Vector(0, 0, -100000), unreal.Rotator(0, 0, 0))
    try:
        c = [m for m in probe.get_components_by_class(unreal.StaticMeshComponent) if m.static_mesh][0]
        bb = c.static_mesh.get_bounding_box()
        rel = c.get_editor_property('relative_location')
        return bb.min.x + rel.x, bb.max.x + rel.x
    finally:
        probe.destroy_actor()


def _fence_run(tag, a, b, x0, x1, k0):
    """Fence segments between local points a -> b (u, v), whole number of pieces, each stretched to fit exactly."""
    length = math.hypot(b[0] - a[0], b[1] - a[1])
    seg = x1 - x0
    n = max(1, int(round(length / seg)))
    scale = length / (n * seg)
    du, dv = (b[0] - a[0]) / length, (b[1] - a[1]) / length
    yaw = YAW + math.degrees(math.atan2(dv, du))
    k = k0
    for i in range(n):
        # piece i covers [i, i+1] * length / n; actor origin sits at -x0*scale from the piece start
        s = i * length / n - x0 * scale
        u, v = a[0] + du * s, a[1] + dv * s
        su, sv = a[0] + du * (i + 0.5) * length / n, a[1] + dv * (i + 0.5) * length / n
        zs = []
        for f in (0.05, 0.5, 0.95):
            pu = a[0] + du * (i + f) * length / n
            pv = a[1] + dv * (i + f) * length / n
            zz = landscape_z(*W(pu, pv))
            if zz is not None:
                zs.append(zz)
        z = min(zs) - 3.0
        x, y = W(u, v)
        act = spawn_bp(FENCE_BP, 'LAND_Fence_%s_%02d' % (tag, k), (x, y, z), yaw, 'Fence', scale=(scale, 1.0, 1.0))
        k += 1
    return k


def step_fence():
    clear_prefix('LAND_Fence_')
    x0, x1 = _fence_dims()
    log('fence piece x %.1f..%.1f' % (x0, x1))
    U, V = FENCE_U, FENCE_V
    k = 0
    # north side (road side) in two runs either side of the gate, then east, south, west - clockwise seen from above
    k = _fence_run('N', (-U, V), (GATE_U - GATE_HALF, V), x0, x1, k)
    k = _fence_run('N', (GATE_U + GATE_HALF, V), (U, V), x0, x1, k)
    k = _fence_run('E', (U, V), (U, -V), x0, x1, 0)
    k = _fence_run('S', (U, -V), (-U, -V), x0, x1, 0)
    k = _fence_run('W', (-U, -V), (-U, V), x0, x1, 0)
    # gate portal (open: the Soviet wall-gate frame without its leaves), road D runs through it
    clear_prefix('LAND_Gate_')
    gu = GATE_U + GATE_HALF - 15.0       # frame x max (15) sits on the east edge of the opening
    x, y = W(gu, V)
    z = min(landscape_z(*W(GATE_U - GATE_HALF, V)), landscape_z(*W(GATE_U + GATE_HALF, V))) - 3.0
    spawn_mesh(GATE_FRAME, 'LAND_Gate_Frame', (x, y, z), YAW, 'Fence')
    log('fence + gate placed')


# ---- towers ---------------------------------------------------------------------------------------------------------
# local u, v, facing (deg, local). Off the flood-mast corners (r1): one over the gate / road D approach from the ridge
# side, one on the open east flank behind the slab end; both >= 20 m from pad centres and clear of the flood beams.
TOWERS = [(-3100.0, 2900.0, 90.0), (4050.0, 0.0, 0.0)]


def step_towers():
    clear_prefix('LAND_Tower_')
    for k, (u, v, face) in enumerate(TOWERS):
        x, y = W(u, v)
        zs = [landscape_z(*W(u + du, v + dv)) for du in (-280, 280) for dv in (-280, 280)]
        a = spawn_bp(TOWER_BP, 'LAND_Tower_%d' % k, (x, y, min(z for z in zs if z is not None) - 3.0), YAW + face,
                     'Towers')
    log('towers placed')


# ---- lights ---------------------------------------------------------------------------------------------------------
LAMP_MAT = FIELD + 'M_LandingField_Lamp'
# lamp custom primitive data: 0 = blink phase (0..1), 1 = blink amount (0 steady .. 1 full flash), 2 = blink rate (Hz)
CPD_PHASE, CPD_BLINK, CPD_RATE = 0, 1, 2


def _param(mel, mat, cls, name, x, y, default=None, cpd=None):
    e = mel.create_material_expression(mat, cls, x, y)
    e.set_editor_property('parameter_name', name)
    if default is not None:
        e.set_editor_property('default_value', default)
    if cpd is not None:
        e.set_editor_property('use_custom_primitive_data', True)
        e.set_editor_property('primitive_data_index', cpd)
    return e


def ensure_lamp_material():
    """Unlit emissive lamp master: Colour * Intensity, optionally flashing. Flash = a short pulse (25 % of the period) per
    cycle of Time * Rate + Phase; Rate, Phase and blink amount come from custom primitive data, so every lamp of one
    colour shares one MI and still runs its own phase (the lead-in 'rabbit' and the mast beacons)."""
    mel = unreal.MaterialEditingLibrary
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mat = unreal.load_asset(LAMP_MAT)
    if mat is None:
        mat = tools.create_asset('M_LandingField_Lamp', FIELD.rstrip('/'), unreal.Material, unreal.MaterialFactoryNew())
    if mel.get_num_material_expressions(mat) < 12:
        mel.delete_all_material_expressions(mat)
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        col = _param(mel, mat, unreal.MaterialExpressionVectorParameter, 'Colour', -900, -200,
                     unreal.LinearColor(1.0, 0.55, 0.15, 1.0))
        inten = _param(mel, mat, unreal.MaterialExpressionScalarParameter, 'Intensity', -900, 0, 40.0)
        phase = _param(mel, mat, unreal.MaterialExpressionScalarParameter, 'BlinkPhase', -1300, 250, 0.0, CPD_PHASE)
        amount = _param(mel, mat, unreal.MaterialExpressionScalarParameter, 'BlinkAmount', -700, 450, 0.0, CPD_BLINK)
        rate = _param(mel, mat, unreal.MaterialExpressionScalarParameter, 'BlinkRate', -1500, 150, 1.0, CPD_RATE)
        time = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1500, 50)
        t_rate = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1300, 100)
        mel.connect_material_expressions(time, '', t_rate, 'A')
        mel.connect_material_expressions(rate, '', t_rate, 'B')
        t_add = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -1100, 150)
        mel.connect_material_expressions(t_rate, '', t_add, 'A')
        mel.connect_material_expressions(phase, '', t_add, 'B')
        frac = mel.create_material_expression(mat, unreal.MaterialExpressionFrac, -950, 150)
        mel.connect_material_expressions(t_add, '', frac, '')
        sub = mel.create_material_expression(mat, unreal.MaterialExpressionSubtract, -800, 150)
        sub.set_editor_property('const_b', 0.75)
        mel.connect_material_expressions(frac, '', sub, 'A')
        sharp = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 150)
        sharp.set_editor_property('const_b', 40.0)
        mel.connect_material_expressions(sub, '', sharp, 'A')
        pulse = mel.create_material_expression(mat, unreal.MaterialExpressionSaturate, -500, 150)
        mel.connect_material_expressions(sharp, '', pulse, '')
        # steady lamps keep 1; flashing lamps drop to a 6 % glow between pulses: lerp(1, 0.06 + pulse, amount)
        base = mel.create_material_expression(mat, unreal.MaterialExpressionAdd, -380, 150)
        base.set_editor_property('const_b', 0.06)
        mel.connect_material_expressions(pulse, '', base, 'A')
        lerp = mel.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -250, 250)
        lerp.set_editor_property('const_a', 1.0)
        mel.connect_material_expressions(base, '', lerp, 'B')
        mel.connect_material_expressions(amount, '', lerp, 'Alpha')
        mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -500, -100)
        mel.connect_material_expressions(col, '', mul, 'A')
        mel.connect_material_expressions(inten, '', mul, 'B')
        out = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 0)
        mel.connect_material_expressions(mul, '', out, 'A')
        mel.connect_material_expressions(lerp, '', out, 'B')
        mel.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(mat)
        unreal.EditorLoadingAndSavingUtils.save_packages([mat.get_outermost()], False)
        log('lamp master rebuilt (%d expressions)' % mel.get_num_material_expressions(mat))
    for name, colour, inten in (('Amber', (1.0, 0.5, 0.12), 60.0), ('Green', (0.15, 1.0, 0.3), 50.0),
                                ('Red', (1.0, 0.05, 0.02), 80.0), ('White', (1.0, 0.85, 0.65), 40.0)):
        path = FIELD + 'MI_LandingField_Lamp_' + name
        mi = unreal.load_asset(path)
        if mi is None:
            mi = tools.create_asset('MI_LandingField_Lamp_' + name, FIELD.rstrip('/'), unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
            mi.set_editor_property('parent', mat)
        mel.set_material_instance_vector_parameter_value(mi, 'Colour', unreal.LinearColor(colour[0], colour[1], colour[2], 1))
        mel.set_material_instance_scalar_parameter_value(mi, 'Intensity', inten)
        unreal.EditorLoadingAndSavingUtils.save_packages([mi.get_outermost()], False)


def _lamp(label, u, v, z, colour, folder='Lights', radius=12.0, height=8.0, sphere=False, blink=None):
    """Emissive lamp mesh, no collision / shadow / nav. blink = (phase, amount, rate) for a flashing lamp."""
    x, y = W(u, v)
    shape = SPHERE_SHAPE if sphere else LAMP_SHAPE
    s = radius * 2.0 / 100.0
    a = spawn_mesh(shape, label, (x, y, z + (0 if sphere else height / 2.0)), YAW, folder,
                   scale=(s, s, (s if sphere else height / 100.0)), collision=False,
                   materials=[FIELD + 'MI_LandingField_Lamp_' + colour])
    comp = a.static_mesh_component
    comp.set_editor_property('cast_shadow', False)
    phase, amount, rate = blink or (0.0, 0.0, 1.0)
    comp.set_default_custom_primitive_data_float(CPD_PHASE, phase)
    comp.set_default_custom_primitive_data_float(CPD_BLINK, amount)
    comp.set_default_custom_primitive_data_float(CPD_RATE, rate)
    return a


def _point(label, u, v, z, colour, intensity, radius, folder='Lights'):
    x, y = W(u, v)
    a = actors_sub().spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, z))
    a.set_actor_label(label)
    a.set_folder_path(FOLDER + folder)
    lc = a.point_light_component
    lc.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
    lc.set_intensity(intensity)
    lc.set_light_color(unreal.LinearColor(*colour))
    lc.set_attenuation_radius(radius)
    lc.set_cast_shadows(False)
    lc.set_editor_property('source_radius', 10.0)
    return a


def _spot(label, eye, target, colour, intensity, radius, outer, inner, folder='Lights'):
    ex, ey = W(eye[0], eye[1])
    tx, ty = W(target[0], target[1])
    ev, tv = unreal.Vector(ex, ey, eye[2]), unreal.Vector(tx, ty, target[2])
    rot = unreal.MathLibrary.find_look_at_rotation(ev, tv)
    a = actors_sub().spawn_actor_from_class(unreal.SpotLight, ev, rot)
    a.set_actor_label(label)
    a.set_folder_path(FOLDER + folder)
    lc = a.spot_light_component
    lc.set_editor_property('intensity_units', unreal.LightUnits.CANDELAS)
    lc.set_intensity(intensity)
    lc.set_light_color(unreal.LinearColor(*colour))
    lc.set_attenuation_radius(radius)
    lc.set_outer_cone_angle(outer)
    lc.set_inner_cone_angle(inner)
    lc.set_cast_shadows(False)
    return a


FLOODS = [(-4250.0, 3050.0), (4250.0, 3050.0), (4250.0, -3050.0), (-4250.0, -3050.0)]
WARM = (1.0, 0.78, 0.52)
# road D's last stretch, rerouted (step_road) through the gate onto the slab at the centre lane's mouth
# (r2) square through the opening for ~6 m, then a gentle bend that meets the slab edge at ~40 degrees
ROAD_TAIL = [(-1880.0, 3750.0), (-1880.0, 3400.0), (-1880.0, 3150.0), (-1250.0, 2950.0), (-550.0, 2800.0),
             (-150.0, 2600.0), (0.0, 2470.0)]


def _road_spline():
    for a in all_actors():
        if a.get_actor_label() == 'REAR_Road_Depot':
            return a, a.get_component_by_class(unreal.SplineComponent)
    raise RuntimeError('REAR_Road_Depot not found')


def step_road():
    """Road D used to stop on a pad corner: keep it up to the point before the gate, then run it through the gate centre
    and onto the slab at the centre lane's mouth, where the green centreline picks it up."""
    actor, spline = _road_spline()
    pts = [spline.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)
           for i in range(spline.get_number_of_spline_points())]
    keep = [p for p in pts if L(p.x, p.y)[1] > 3950.0]          # everything before the straight run into the gate
    z = keep[-1].z if keep else 150.0
    new = keep + [unreal.Vector(W(u, v)[0], W(u, v)[1], z) for u, v in ROAD_TAIL]
    spline.set_spline_points(new, unreal.SplineCoordinateSpace.WORLD, True)
    spline.set_editor_property('spline_has_been_edited', True)
    width = actor.get_editor_property('PathWidth')
    actor.set_editor_property('PathWidth', width + 1.0)
    actor.set_editor_property('PathWidth', width)
    log('road D: %d -> %d points, ends at lane mouth' % (len(pts), len(new)))


def _pad_hit(u, v, margin=120.0):
    return any(abs(u - pu) < 1050.0 + margin and abs(v - pv) < 1050.0 + margin for pu, pv in PADS.values())


def step_lights():
    clear_prefix('LAND_Light_')
    clear_prefix('LAND_Lamp_')
    ensure_lamp_material()
    top = slab_top()
    # floodlight masts in the fence corners: a tight warm pool on the nearest pad each (no shadows)
    for k, (u, v) in enumerate(FLOODS):
        z = landscape_z(*W(u, v)) - 3.0
        face = math.degrees(math.atan2(-v, -u))
        x, y = W(u, v)
        spawn_mesh(FLOOD_MESH, 'LAND_Light_FloodTower_%d' % k, (x, y, z), YAW + face, 'Lights')
        pu, pv = min(PADS.values(), key=lambda p: math.hypot(p[0] - u, p[1] - v))
        sp = _spot('LAND_Light_FloodSpot_%d' % k, (u, v, z + 1080.0), (pu, pv, top), WARM, 4000.0, 4000.0, 18.0, 8.0)
        sp.spot_light_component.set_editor_property('specular_scale', 0.25)   # r2: no bleached stars
        # red aviation obstruction beacon on the mast top, slow flash, masts out of step
        _lamp('LAND_Lamp_Obstruction_%d' % k, u, v, z + 1125.0, 'Red', radius=16.0, sphere=True,
              blink=(k * 0.25, 0.85, 0.6))
    # one green centreline from outside the gate, along road D's new tail, down the centre lane (u = 0) to the far edge
    actor, spline = _road_spline()
    total = spline.get_spline_length()
    k = 0
    pts = []
    d = total
    while d > 0:
        p = spline.get_location_at_distance_along_spline(d, unreal.SplineCoordinateSpace.WORLD)
        lu, lv = L(p.x, p.y)
        if lv > FENCE_V + 1000.0:
            break
        if lv >= 2550.0:            # the slab part is placed evenly below
            pts.append((lu, lv))
        d -= 500.0
    pts.reverse()                   # outside the gate -> slab edge
    n_road = len(pts)
    for k8 in range(9):             # evenly from the lane mouth to the far edge (r2)
        pts.append((0.0, 2380.0 - 595.0 * k8))
    for i, (lu, lv) in enumerate(pts):
        if _pad_hit(lu, lv):
            continue
        on_slab = abs(lu) <= SLAB_U and abs(lv) <= SLAB_V
        zz = top if on_slab else landscape_z(*W(lu, lv)) - 2.0
        # the road part runs a sequenced flash toward the slab ('rabbit'); the slab part burns steady
        blink = (-i * 0.07, 0.75, 0.7) if i < n_road else None
        _lamp('LAND_Lamp_Centre_%02d' % k, lu, lv, zz, 'Green', radius=11.0, height=(6.0 if on_slab else 18.0),
              blink=blink)
        k += 1
    # gate: one soft warm light over the road between the pillars
    zz = landscape_z(*W(GATE_U, FENCE_V))
    _point('LAND_Light_Gate', GATE_U, FENCE_V - 100.0, zz + 550.0, WARM, 80.0, 900.0)
    log('lights placed (%d centreline lamps)' % k)


# ---- extras ---------------------------------------------------------------------------------------------------------
SERVICE_V = 2960.0


def step_extras():
    """A tidy service strip on the north apron east of the gate: crew/radio hut + flag, fuel bowser and two barrel
    stacks in one row; and a raised boom at the gate. All low (< 3.5 m) and >= 17 m from pad centres."""
    clear_prefix('LAND_Extra_')

    def seat(u, v):
        return landscape_z(*W(u, v)) - 3.0
    x, y = W(1300.0, SERVICE_V)
    spawn_bp('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_ContainerHut',
             'LAND_Extra_Hut', (x, y, min(seat(1300 + du, SERVICE_V + dv) for du in (-170, 170) for dv in (-170, 170))),
             YAW + 90.0, 'Extras')
    x, y = W(700.0, SERVICE_V + 120.0)
    spawn_bp(ENV + 'Dest_SovietFlag', 'LAND_Extra_Flag', (x, y, seat(700.0, SERVICE_V + 120.0)), YAW + 180.0, 'Extras')
    # fuel bowser lengthwise along the fence (mesh is long along Y), then two barrel stacks
    # (r2) on two pallets so it reads as a parked bowser, not a dumped tank; the tank spans u 2381..2942, v +-21..251
    pallet = '/Game/MilitaryAirport/Meshes/Props/SM_Pallet_1'
    ph = unreal.load_asset(pallet).get_bounding_box().max.z
    tz = min(seat(2450 + du, SERVICE_V) for du in (-300, 300))
    for k, pu in enumerate((2480.0, 2840.0)):
        x, y = W(pu, SERVICE_V + 175.0)
        spawn_mesh(pallet, 'LAND_Extra_FuelPallet_%d' % k, (x, y, tz), YAW + 90.0, 'Extras')
    x, y = W(2450.0, SERVICE_V + 60.0)
    spawn_mesh('/Game/MilitaryAirport/Meshes/Props/SM_FuelTank', 'LAND_Extra_FuelTank', (x, y, tz + ph - 1.0), YAW + 90.0,
               'Extras')
    for k, u in enumerate((3150.0, 3500.0)):
        x, y = W(u, SERVICE_V + 60.0)
        spawn_bp(ENV + 'Airport/BP_Airport_Dest_BarrelStack', 'LAND_Extra_Barrels_%d' % k, (x, y, seat(u, SERVICE_V)),
                 YAW + 90.0, 'Extras')
    # raised boom barrier inside the gate's east pillar (bar raised, pointing up and back over the road)
    bu, bv = GATE_U + GATE_HALF - 90.0, FENCE_V - 140.0
    x, y = W(bu, bv)
    z = seat(bu, bv)
    spawn_mesh('/Game/MilitaryAirport/Meshes/Props/SM_GateBarrier_1', 'LAND_Extra_GateBoomPost', (x, y, z), YAW, 'Extras')
    # bar (pivot at its +X end, runs along -X) raised 55 degrees and leaning inward over the apron (r2)
    spawn_mesh('/Game/MilitaryAirport/Meshes/Props/SM_GateBarrier_2_Bar', 'LAND_Extra_GateBoomBar', (x, y, z + 105.0),
               YAW + 90.0, 'Extras', collision=False, pitch=-55.0)
    log('extras placed')


def build_all():
    require_map()
    step_clear()
    step_road()
    step_pads()
    step_fence()
    step_towers()
    step_lights()
    step_extras()
    log('landing field built')


if globals().get('RUN_BUILD', True):
    build_all()
