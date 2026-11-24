"""Read-only probe of the Berlin ruin kit wall pieces (B1-B6) for tank breaching: components, owner, mesh slots and
bounds, UCX hulls, NavBlock boxes and hole settings. Run in a DebugGame commandlet; writes Saved/WallChunks/Breach/
berlin_probe.json."""
import json
import unreal

BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/'
PIECES = ['B1_Facade1St_A', 'B1_Facade1St_B', 'B2_Facade2St_A', 'B2_Facade2St_B', 'B3_Facade4St', 'B4_Firewall',
          'B5_CornerL', 'B6_Stub_A', 'B6_Stub_B']
OUT = 'D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach/berlin_probe.json'


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def probe(piece):
    bp = unreal.load_asset(BP_DIR + 'BP_BerlinRuin_' + piece)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    info = dict(components=[])
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(
            unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle), bp)
        if obj is None:
            continue
        cls = obj.get_class().get_name()
        row = dict(name=obj.get_name(), cls=cls)
        if isinstance(obj, unreal.StaticMeshComponent) and not isinstance(obj, unreal.InstancedStaticMeshComponent):
            mesh = obj.get_editor_property('static_mesh')
            if mesh:
                box = mesh.get_bounding_box()
                row.update(mesh=mesh.get_name(), min=vec(box.min), max=vec(box.max),
                           slots=[str(m.get_editor_property('material_slot_name')) for m in mesh.get_editor_property('static_materials')],
                           overrides=[m.get_name() if m else None for m in obj.get_editor_property('override_materials')])
        if isinstance(obj, unreal.BoxComponent):
            row.update(loc=vec(obj.get_editor_property('relative_location')),
                       rot=str(obj.get_editor_property('relative_rotation')),
                       extent=vec(obj.get_editor_property('box_extent')),
                       area=str(obj.get_editor_property('area_class_override')),
                       dynamic=obj.get_editor_property('dynamic_obstacle'))
        if cls == 'WallHoleDamageComponent':
            for prop in ('wall_mesh_component_name', 'damaged_materials', 'debris_meshes', 'hole_settings',
                         'grid_settings', 'collapse_settings', 'mask_storage'):
                try:
                    row[prop] = str(obj.get_editor_property(prop))[:400]
                except Exception as error:  # noqa: BLE001
                    row[prop] = 'ERR ' + repr(error)[:80]
        info['components'].append(row)
    cdo = unreal.get_default_object(bp.generated_class())
    for prop in ('health', 'damage_reduction', 'aim_offset_points'):
        try:
            info[prop] = str(cdo.get_editor_property(prop))[:300]
        except Exception as error:  # noqa: BLE001
            info[prop] = 'ERR ' + repr(error)[:80]
    return info


result = {}
for piece in PIECES:
    try:
        result[piece] = probe(piece)
    except Exception as error:  # noqa: BLE001
        result[piece] = dict(error=repr(error))
json.dump(result, open(OUT, 'w'), indent=1, default=str)
unreal.log('BERLIN_PROBE written ' + OUT)
