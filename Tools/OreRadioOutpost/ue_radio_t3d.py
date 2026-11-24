"""Read-only: T3D export of Rus_Bxp_ContainerHut, RTS_BP_BuildingExpansion and SM_RusHut_Container (graph pin defaults,
collision) into Saved/OreMountains/radio/t3d/."""
import os

import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/radio/t3d/'
os.makedirs(OUT, exist_ok=True)
for path in ('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_ContainerHut',
             '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RTS_BP_BuildingExpansion',
             '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Megaphone/SM_RusHut_Container'):
    obj = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = obj
    task.filename = OUT + path.split('/')[-1] + '.t3d'
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = unreal.ObjectExporterT3D()
    ok = unreal.Exporter.run_asset_export_task(task)
    unreal.log_warning('ORE_RADIO t3d %s %s' % (path, ok))
m = unreal.load_asset('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Megaphone/SM_RusHut_Container')
body = m.get_editor_property('body_setup')
g = body.get_editor_property('agg_geom')
unreal.log_warning('ORE_RADIO container collision trace=%s boxes=%d convex=%d spheres=%d' % (
    body.get_editor_property('collision_trace_flag'), len(g.get_editor_property('box_elems')),
    len(g.get_editor_property('convex_elems')), len(g.get_editor_property('sphere_elems'))))
bp = unreal.load_asset('/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Rus_Bxp_ContainerHut')
cdo = unreal.get_default_object(bp.generated_class())
for c in cdo.get_components_by_class(unreal.ActorComponent):
    unreal.log_warning('ORE_RADIO cdo comp %s %s' % (c.get_name(), c.get_class().get_name()))
    if isinstance(c, unreal.DecalComponent):
        unreal.log_warning('ORE_RADIO decal size %s' % c.get_editor_property('decal_size'))
    if isinstance(c, unreal.StaticMeshComponent):
        unreal.log_warning('ORE_RADIO mesh comp override mats %s' % c.get_editor_property('override_materials'))
