"""Read-only: dumps Rus_Bxp_ContainerHut (parent class, component tree, meshes and bounds, graphs) for the radio hut child."""
import unreal

PATH = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_ContainerHut'
SDS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
SDL = unreal.SubobjectDataBlueprintFunctionLibrary


def log(*a):
    unreal.log_warning('ORE_RADIO ' + ' '.join(str(x) for x in a))


bp = unreal.load_asset(PATH)
gc = bp.generated_class()
log('class', gc.get_name(), 'parent', unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_name()
    if hasattr(unreal.BlueprintEditorLibrary, 'get_blueprint_parent_class') else '?')
cdo = unreal.get_default_object(gc)
c = cdo.get_class()
chain = []
while c:
    chain.append(c.get_name())
    try:
        c = c.get_super_class()
    except Exception:
        break
log('chain', chain)
for h in SDS.k2_gather_subobject_data_for_blueprint(bp):
    d = SDS.k2_find_subobject_data_from_handle(h)
    o = SDL.get_object_for_blueprint(d, bp)
    if o is None:
        continue
    info = [o.get_name(), o.get_class().get_name(), 'inherited=%s' % SDL.is_inherited_component(d)]
    if isinstance(o, unreal.SceneComponent):
        info.append('loc=%s rot=%s scale=%s' % (o.get_editor_property('relative_location'), o.get_editor_property('relative_rotation'),
                                               o.get_editor_property('relative_scale3d')))
        try:
            info.append('parent=%s' % (o.get_attach_parent().get_name() if o.get_attach_parent() else None))
        except Exception:
            pass
    if isinstance(o, unreal.StaticMeshComponent):
        m = o.get_editor_property('static_mesh')
        if m:
            b = m.get_bounding_box()
            info.append('mesh=%s bounds=(%.0f %.0f %.0f)-(%.0f %.0f %.0f) tris=%d' % (m.get_path_name(), b.min.x, b.min.y, b.min.z,
                                                                                  b.max.x, b.max.y, b.max.z, m.get_num_triangles(0)))
        info.append('coll=%s nav=%s tags=%s' % (o.get_collision_profile_name(), o.get_editor_property('can_ever_affect_navigation'),
                                                o.get_editor_property('component_tags')))
        info.append('mats=%s' % [o.get_material(i).get_name() if o.get_material(i) else None for i in range(o.get_num_materials())])
    if isinstance(o, unreal.ShapeComponent) or isinstance(o, unreal.BoxComponent):
        try:
            info.append('extent=%s coll=%s' % (o.get_editor_property('box_extent'), o.get_collision_profile_name()))
        except Exception:
            pass
    log('comp', ' | '.join(info))
try:
    log('tags', cdo.get_editor_property('tags'))
except Exception as e:
    log('tags err', e)
for g in unreal.BlueprintEditorLibrary.find_event_graph(bp), :
    log('event graph', g)
# whole-class defaults that might matter
for prop in ('max_health', 'Health', 'HealthComponent'):
    try:
        log(prop, cdo.get_editor_property(prop))
    except Exception:
        pass
