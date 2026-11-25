"""Look test: silo mesh / material options for the silo group (commandlet with a renderer; saves nothing)."""
import sys
import traceback

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/OreSitesModels/OrePlantHeapStation')
import unreal  # noqa: E402
import ue_shot_lib as L  # noqa: E402

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/ue_oreplant/look/'
A = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
OPTIONS = [
    ('/Game/MillitaryBase/Meshes/SM_Silos', None),
    ('/Game/MillitaryBase/Meshes/SM_Silos', '/Game/MillitaryBase/Materials/MI_SiloBlue'),
    ('/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/PostApo/Meshes/SM_Scorch_Silo2', None),
    ('/Game/MillitaryBase/Meshes/SM_Silos', A + 'A4Viaduct/MI_A4_Concrete'),
    ('/Game/MillitaryBase/Meshes/SM_Silos', A + 'AditPortals/Materials/MI_OreAdit_Sheet'),
    ('/Game/MillitaryBase/Meshes/SM_Silos', A + 'AditPortals/Materials/MI_OreAdit_Paint'),
]
try:
    unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
    L.streaming(False)
    for attempt in range(2):
        shown = [L.ground(6000), *L.lights()]
        for k, (mesh, mat) in enumerate(OPTIONS):
            a = L.spawn(mesh, (0, -1500 + k * 600, 0), 0, scale=(1.15, 1.15, 1.15))
            if mat:
                c = a.static_mesh_component
                for i in range(c.get_num_materials()):
                    c.set_material(i, unreal.load_asset(mat))
            shown.append(a)
        unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
        L.capture(OUT + 'silo_options%s.png' % ('' if attempt else '_warm'), (-3200, 0, 1500), (0, 0, 600), fov=70,
                  show=shown, res=(1600, 800))
        for a in shown:
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(a)
    unreal.log_warning('OREPLANT_SILOOPT ok')
except Exception:
    unreal.log_warning('OREPLANT_SILOOPT FAIL ' + traceback.format_exc())
finally:
    L.streaming(True)
