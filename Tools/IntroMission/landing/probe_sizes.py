# Probe: Pe-8 transport size, existing guard tower setup, sun, depot terrain. Read-only (spawned probes destroyed).
import unreal
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/rear/rear_lib.py').read())
out = []
sub = actors_sub()
def bounds_of(actor):
    lo = [1e9]*3; hi = [-1e9]*3
    for c in actor.get_components_by_class(unreal.PrimitiveComponent):
        if isinstance(c, (unreal.BillboardComponent,)) or not c.is_visible():
            continue
        o, e, r = unreal.SystemLibrary.get_component_bounds(c)
        out.append('   comp %s %s o=(%.0f,%.0f,%.0f) e=(%.0f,%.0f,%.0f)' % (c.get_class().get_name(), c.get_name(), o.x, o.y, o.z, e.x, e.y, e.z))
for p in ['/Game/RTS_Survival/Blueprints/Aircraft/RusAircraft/PE8Bomber/Rus_PE8_Aircraft_Transport',
          '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_GuardTowerMG_Soviet',
          '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Rus_BXP_LargeGuardTower',
          '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Industrial/BP_Industrial_Dest_FenceWire',
          '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Airport/BP_Airport_Dest_FenceWire']:
    try:
        cls = unreal.load_class(None, p + '.' + p.split('/')[-1] + '_C')
        a = sub.spawn_actor_from_class(cls, unreal.Vector(0, 0, -200000), unreal.Rotator(0, 0, 0))
        out.append(p)
        try:
            bounds_of(a)
        finally:
            a.destroy_actor()
    except Exception as ex:
        out.append('FAIL %s %s' % (p, ex))
for a in all_actors():
    c = a.get_class().get_name()
    if c in ('DirectionalLight', 'SkyLight', 'ExponentialHeightFog', 'PostProcessVolume', 'SkyAtmosphere') or c.startswith('Rus_BXP_GuardTower'):
        out.append('%s %s loc %s rot %s' % (a.get_actor_label(), c, a.get_actor_location(), a.get_actor_rotation()))
        if c.startswith('Rus_BXP'):
            for prop in ('owning_player',):
                pass
            rc = a.get_component_by_class(unreal.load_class(None, '/Script/RTS_Survival.RTSComponent'))
            if rc: out.append('   rts owner ' + str(rc.get_editor_property('owning_player') if hasattr(rc,'owning_player') else '?'))
        if c == 'DirectionalLight':
            lc = a.light_component
            out.append('   intensity %s mobility %s' % (lc.intensity, a.root_component.mobility))
    if a.get_actor_location().z < -150000:
        out.append('STRAY %s %s' % (a.get_actor_label(), c))
open('D:/UE5Projects/RTS_Survival/Saved/IntroLanding/probe.txt', 'w').write('\n'.join(out))
print('probe done', len(out))
