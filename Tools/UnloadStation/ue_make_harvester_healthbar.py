"""Headless: builds W_BpHealthBar_Harvester (W_BpHealthBar_Modules + an ActionProgressBar) and sets it on every harvester.

Run (no other process may hold the assets):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -nullrhi -unattended
      -DisablePlugins=Fab,BpGeneratorUltimate
The bar copies W_BpHealthBar's ConversionProgressBar (slot, shear, style) so it sits just above the health band, with an
amber fill. UW_HealthBar binds it by name (BindWidgetOptional ActionProgressBar) and keeps it collapsed until a
harvester docks at an unload station. Prints HHB lines; idempotent (an existing bar or class is left as it is).
"""
import unreal

WIDGET_DIR = '/Game/RTS_Survival/Blueprints/GameUI/InLevelAttachmentWidgets/'
SOURCE_BAR_WIDGET = WIDGET_DIR + 'W_BpHealthBar'
BASE_WIDGET = WIDGET_DIR + 'W_BpHealthBar_Modules'
HARVESTER_WIDGET = WIDGET_DIR + 'W_BpHealthBar_Harvester'
BAR_NAME = 'ActionProgressBar'
SOURCE_BAR_NAME = 'ConversionProgressBar'
PANEL_NAME = 'CanvasPanel_47'
# Amber: reads as "cargo moving", apart from the green health band and the converter's green bar.
FILL_COLOR = unreal.LinearColor(1.0, 0.62, 0.12, 1.0)
GROUND_VEHICLES = '/Game/RTS_Survival/Blueprints/GroundVehicles'
EAL = unreal.EditorAssetLibrary


def widget_in(bp, name):
    return unreal.find_object(None, bp.get_path_name() + ':WidgetTree.' + name)


def make_widget():
    if not EAL.does_asset_exist(HARVESTER_WIDGET):
        print('HHB duplicate', EAL.duplicate_asset(BASE_WIDGET, HARVESTER_WIDGET))
    bp = unreal.load_asset(HARVESTER_WIDGET)
    if widget_in(bp, BAR_NAME):
        print('HHB bar already in', HARVESTER_WIDGET)
        return bp
    source = widget_in(unreal.load_asset(SOURCE_BAR_WIDGET), SOURCE_BAR_NAME)
    panel = widget_in(bp, PANEL_NAME)
    if not source or not isinstance(panel, unreal.CanvasPanel):
        print('HHB FAIL source bar or panel missing', source, panel)
        return None
    tree = unreal.find_object(None, bp.get_path_name() + ':WidgetTree')
    bar = unreal.new_object(unreal.ProgressBar, outer=tree, name=BAR_NAME)
    for prop in ('widget_style', 'bar_fill_type', 'render_transform', 'render_transform_pivot', 'border_padding',
                 'is_marquee'):
        bar.set_editor_property(prop, source.get_editor_property(prop))
    bar.set_editor_property('fill_color_and_opacity', FILL_COLOR)
    bar.set_editor_property('percent', 0.0)
    bar.set_editor_property('visibility', unreal.SlateVisibility.COLLAPSED)
    slot = panel.add_child(bar)
    source_slot = source.get_editor_property('slot')
    slot.set_editor_property('layout_data', source_slot.get_editor_property('layout_data'))
    slot.set_auto_size(source_slot.get_auto_size())
    slot.set_z_order(source_slot.get_z_order())
    print('HHB added', bar.get_path_name(), 'children', [c.get_name() for c in panel.get_all_children()])
    return bp


def set_on_harvesters(widget_class):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    ar = unreal.AssetRegistryHelpers.get_asset_registry()
    flt = unreal.ARFilter(package_paths=[GROUND_VEHICLES], recursive_paths=True,
                          class_paths=[unreal.TopLevelAssetPath('/Script/Engine', 'Blueprint')])
    for data in ar.get_assets(flt):
        path = str(data.package_name)
        bp = unreal.load_asset(path)
        if not bp:
            continue
        harvester, health = None, None
        for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
            data = lib.get_data(handle)
            obj = lib.get_object(data)
            if isinstance(obj, unreal.Harvester):
                harvester = obj
            elif isinstance(obj, unreal.HealthComponent):
                # The health component is inherited from RTS_BP_TrackedTankMaster: edit this Blueprint's own override
                # template, never the shared parent template.
                health = lib.get_object_for_blueprint(data, bp)
        if harvester is None:
            continue
        if health is None:
            print('HHB FAIL no health component on', path)
            continue
        if health.get_editor_property('health_bar_widget_class') == widget_class:
            print('HHB already set', path)
            continue
        if not health.get_path_name().startswith(path + '.'):
            print('HHB FAIL template is not owned by', path, health.get_path_name())
            continue
        health.set_editor_property('health_bar_widget_class', widget_class)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        print('HHB set', path, 'saved', EAL.save_asset(path, only_if_is_dirty=False))


widget_bp = make_widget()
if widget_bp:
    unreal.BlueprintEditorLibrary.compile_blueprint(widget_bp)
    print('HHB widget saved', EAL.save_asset(HARVESTER_WIDGET, only_if_is_dirty=False))
    widget_class = unreal.load_class(None, HARVESTER_WIDGET + '.W_BpHealthBar_Harvester_C')
    print('HHB class', widget_class)
    if widget_class:
        set_on_harvesters(widget_class)
print('HHB done')
