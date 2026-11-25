"""Headless: builds W_BpHealthBar_Refinery (W_BpHealthBar + an ActionProgressBar) and sets it on both Soviet refineries.

Run (no other process may hold the assets):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -nullrhi -unattended
      -DisablePlugins=Fab,BpGeneratorUltimate
The refinery fills the bar while it trains a T-34 Breaker (USovietRefineryComponent::StartTrainingProgressBar). The bar
copies W_BpHealthBar's ConversionProgressBar (slot, shear, style), so it sits just above the health band; refineries
never convert, so the two never show together. Amber fill, as on the harvesters' unload bar. Prints RHB lines;
idempotent (an existing bar or class is left as it is).
"""
import unreal

WIDGET_DIR = '/Game/RTS_Survival/Blueprints/GameUI/InLevelAttachmentWidgets/'
BASE_WIDGET = WIDGET_DIR + 'W_BpHealthBar'
REFINERY_WIDGET = WIDGET_DIR + 'W_BpHealthBar_Refinery'
BAR_NAME = 'ActionProgressBar'
SOURCE_BAR_NAME = 'ConversionProgressBar'
PANEL_NAME = 'CanvasPanel_47'
FILL_COLOR = unreal.LinearColor(1.0, 0.62, 0.12, 1.0)
REFINERY_DIR = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/SovietRefinery/'
REFINERIES = [REFINERY_DIR + 'Rus_Building_SovietRefinery_A1', REFINERY_DIR + 'Rus_Building_SovietRefinery_G2']
EAL = unreal.EditorAssetLibrary


def widget_in(bp, name):
    return unreal.find_object(None, bp.get_path_name() + ':WidgetTree.' + name)


def make_widget():
    if not EAL.does_asset_exist(REFINERY_WIDGET):
        print('RHB duplicate', EAL.duplicate_asset(BASE_WIDGET, REFINERY_WIDGET))
    bp = unreal.load_asset(REFINERY_WIDGET)
    if widget_in(bp, BAR_NAME):
        print('RHB bar already in', REFINERY_WIDGET)
        return bp
    source = widget_in(bp, SOURCE_BAR_NAME)
    panel = widget_in(bp, PANEL_NAME)
    if not source or not isinstance(panel, unreal.CanvasPanel):
        print('RHB FAIL source bar or panel missing', source, panel)
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
    print('RHB added', bar.get_path_name(), 'children', [c.get_name() for c in panel.get_all_children()])
    return bp


def set_on_refineries(widget_class):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for path in REFINERIES:
        bp = unreal.load_asset(path)
        health = None
        for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
            data = lib.get_data(handle)
            if isinstance(lib.get_object(data), unreal.HealthComponent):
                # Inherited from RTS_BP_BuildingExpansion: edit this Blueprint's own override, never the parent template.
                health = lib.get_object_for_blueprint(data, bp)
        if health is None:
            print('RHB FAIL no health component on', path)
            continue
        if health.get_editor_property('health_bar_widget_class') == widget_class:
            print('RHB already set', path)
            continue
        if not health.get_path_name().startswith(path + '.'):
            print('RHB FAIL template is not owned by', path, health.get_path_name())
            continue
        health.set_editor_property('health_bar_widget_class', widget_class)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        print('RHB set', path, 'saved', EAL.save_asset(path, only_if_is_dirty=False))


widget_bp = make_widget()
if widget_bp:
    unreal.BlueprintEditorLibrary.compile_blueprint(widget_bp)
    print('RHB widget saved', EAL.save_asset(REFINERY_WIDGET, only_if_is_dirty=False))
    widget_class = unreal.load_class(None, REFINERY_WIDGET + '.W_BpHealthBar_Refinery_C')
    print('RHB class', widget_class)
    if widget_class:
        set_on_refineries(widget_class)
print('RHB done')
