"""Creates / updates the Soviet counterattack mission assets of Ger_IntroMission (editor Python). 2026-11-18.

    DA_Intro_SovietCounterattack       USovietCounterattackSettings (difficulty blocks keep their C++ defaults unless
                                       they were edited in the asset; this script only fills the map-specific fields)
    Mission_Intro_SovietCounterattack  Blueprint child of USovietCounterattackMission, Settings = the data asset

Both in /Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/. Re-runnable: existing assets are updated.
setup() returns a list of what it did; it saves the two assets (not the map: use rewire_ridge_chain.rewire()).
The train fields come from train_path.json (written by train_path_probe.py) and BP_SovietCargoTrain.
"""
import json
import os

import unreal

DIR = '/Game/RTS_Survival/Blueprints/Missions/GerCampaign/IntroMission/'
DA_NAME = 'DA_Intro_SovietCounterattack'
BP_NAME = 'Mission_Intro_SovietCounterattack'
TIMER_WIDGET = '/Game/RTS_Survival/Blueprints/Missions/MissionUI/W_BP_MissionTimer.W_BP_MissionTimer_C'
TRAIN_BP = DIR + 'BP_SovietCargoTrain'
HERE = os.path.dirname(os.path.abspath(__file__)) if '__file__' in globals() else \
    'D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/counterattack'
TRAIN_PATH_JSON = os.path.join(HERE, 'train_path.json')
# Static tank meshes on the flatcars (set by the train sub-agent's findings; empty flatcars when missing).
CARGO_TANK_MESHES = [
    DIR + 'CargoTrain/SM_CargoTank_T3476.SM_CargoTank_T3476',
    DIR + 'CargoTrain/SM_CargoTank_KV1E.SM_CargoTank_KV1E',
    DIR + 'CargoTrain/SM_CargoTank_IS2.SM_CargoTank_IS2',
]


def _asset_tools():
    return unreal.AssetToolsHelpers.get_asset_tools()


def ensure_data_asset(report):
    path = DIR + DA_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property('data_asset_class', unreal.SovietCounterattackSettings)
    asset = _asset_tools().create_asset(DA_NAME, DIR.rstrip('/'), unreal.SovietCounterattackSettings, factory)
    report.append('created ' + path)
    return asset


def apply_map_fields(da, report):
    timer = unreal.load_class(None, TIMER_WIDGET)
    if timer is not None:
        da.set_editor_property('timer_widget_class', timer)
    else:
        report.append('WARNING: timer widget class not found: ' + TIMER_WIDGET)
    train = da.get_editor_property('train')
    if unreal.EditorAssetLibrary.does_asset_exist(TRAIN_BP):
        train.set_editor_property('train_class', unreal.load_asset(TRAIN_BP).generated_class())
    else:
        report.append('WARNING: no ' + TRAIN_BP + ' yet: the station units spawn without a train')
    if os.path.exists(TRAIN_PATH_JSON):
        data = json.load(open(TRAIN_PATH_JSON))
        train.set_editor_property('path_points', [unreal.Vector(*p) for p in data['points']])
        stop = data.get('stop_front_location')
        if stop:
            train.set_editor_property('stop_front_location', unreal.Vector(*stop))
        report.append(f"train path {len(data['points'])} points, stop {stop}")
    else:
        report.append('WARNING: no train_path.json')
    meshes = [unreal.load_asset(m) for m in CARGO_TANK_MESHES]
    train.set_editor_property('cargo_tank_meshes', [m for m in meshes if m is not None])
    da.set_editor_property('train', train)
    unreal.EditorAssetLibrary.save_loaded_asset(da, False)


def ensure_mission_blueprint(da, report):
    path = DIR + BP_NAME
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        bp = unreal.load_asset(path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property('parent_class', unreal.SovietCounterattackMission)
        bp = _asset_tools().create_asset(BP_NAME, DIR.rstrip('/'), unreal.Blueprint, factory)
        report.append('created ' + path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property('settings', da)
    unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    return bp


def setup():
    report = []
    da = ensure_data_asset(report)
    apply_map_fields(da, report)
    ensure_mission_blueprint(da, report)
    report.append('saved ' + DA_NAME + ' and ' + BP_NAME)
    return report
