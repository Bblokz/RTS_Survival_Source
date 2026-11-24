"""Look shots of the built Blueprints with their real Unreal materials (commandlet with a renderer; saves nothing).
  sh run_ue_py.sh ue_look.py look render        (check nvidia-smi first)
Pictures: Saved/OreMountains/sites/ue_oreplant/look/*.png"""
import sys
import traceback

sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/OreSitesModels/OrePlantHeapStation')
import unreal  # noqa: E402
import ue_shot_lib as L  # noqa: E402

OUT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/sites/ue_oreplant/look/'
S = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/'
# relative to the crusher pivot (cm, Unreal X north / Y east), the layout's fixed offsets
ITEMS = [(S + 'Sites/BaseA/BP_OreBaseA_CrusherHouse', (0, 0), 0),
         (S + 'Sites/BaseA/BP_OreBaseA_SiloGroup', (-1250, 1150), 0),
         (S + 'Sites/BaseA/BP_OreBaseA_NissenShed', (150, -2650), 90)]
HEAP_AT = (0, -6000)
try:
    unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
    L.streaming(False)
    for attempt in range(2):
        shown = [L.ground(15000), *L.lights()]
        for path, (x, y), yaw in ITEMS:
            shown.append(L.spawn(path, (x, y, 0), yaw))
        heap = L.spawn(S + 'HeapStations/BP_OreHeapStation_Erzrolle', (HEAP_AT[0], HEAP_AT[1], 0), 0)
        shown.append(heap)
        shown.append(L.block((-1250 - 480, 1150, 0), (250, 800, 280), (0.1, 0.2, 0.55)))   # lorry under spout 2
        for k in range(3):
            shown.append(L.block((-700, 300 + k * 80, 0), (50, 50, 180), (0.7, 0.08, 0.06)))
        shown.append(L.block((HEAP_AT[0] + 800, HEAP_AT[1], 0), (650, 280, 300), (0.1, 0.2, 0.55)))   # harvester at the drop-off
        unreal.SystemLibrary.execute_console_command(None, 'Editor.AsyncAssetCompilationFinishAll')
        tag = '' if attempt else '_warm'
        c = (-600.0, 300.0, 0.0)
        L.capture(OUT + 'baseA_rts%s.png' % tag, L.rts_eye(c, 4900), c, fov=80, show=shown)
        L.capture(OUT + 'baseA_low%s.png' % tag, (-6200, 2200, 1300), (-600, 0, 600), fov=55, show=shown)
        h = (HEAP_AT[0] + 200.0, HEAP_AT[1], 200.0)
        L.capture(OUT + 'heap_rts%s.png' % tag, L.rts_eye(h, 4000), h, fov=60, show=shown)
        L.capture(OUT + 'heap_low%s.png' % tag, (HEAP_AT[0] - 2000, HEAP_AT[1] + 2200, 700), h, fov=55, show=shown)
        for a in shown:
            unreal.get_editor_subsystem(unreal.EditorActorSubsystem).destroy_actor(a)
    unreal.log_warning('OREPLANT_LOOK ok')
except Exception:
    unreal.log_warning('OREPLANT_LOOK FAIL ' + traceback.format_exc())
finally:
    L.streaming(True)
