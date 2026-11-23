"""Read or set the refinery converter's rate (Ger_Ref_Converter's ConversionSettings variable default).

Run headless in a throwaway editor commandlet, never in the shared editor:
  UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file>" -unattended -nosplash -NoLiveCoding
      -DisablePlugins=Fab,BpGeneratorUltimate

Environment variables:
  CONVERTER_RATE   "In,Out,Seconds" to write, e.g. "15,5,6.667"; unset = read only.
  CONVERTER_OUT    JSON file that receives the settings read before and after the edit.
"""
import json
import os

import unreal

BP_PATH = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Refinery/RadixConvert/Ger_Ref_Converter'
VARIABLE = 'ConversionSettings'


def describe(settings):
    tick = settings.get_editor_property('tick')
    deltas = tick.get_editor_property('resource_deltas')
    return {
        'tick_rate': tick.get_editor_property('tick_rate'),
        'start_enabled': tick.get_editor_property('start_enabled'),
        'deltas': {str(key): int(value) for key, value in deltas.items()},
    }


def write_rate(cdo, settings, rate_text):
    amount_in, amount_out, seconds = [float(part) for part in rate_text.split(',')]
    tick = settings.get_editor_property('tick')
    deltas = tick.get_editor_property('resource_deltas')
    for key in list(deltas.keys()):
        name = str(key).upper()
        if 'RADIXITE' in name:
            deltas[key] = -int(amount_in)
        elif 'VEHICLE' in name:
            deltas[key] = int(amount_out)
    tick.set_editor_property('resource_deltas', deltas)
    tick.set_editor_property('tick_rate', seconds)
    settings.set_editor_property('tick', tick)
    cdo.set_editor_property(VARIABLE, settings)


def main():
    result = {}
    blueprint = unreal.load_asset(BP_PATH)
    cdo = unreal.get_default_object(blueprint.generated_class())
    settings = cdo.get_editor_property(VARIABLE)
    result['before'] = describe(settings)
    rate_text = os.environ.get('CONVERTER_RATE', '')
    if rate_text:
        write_rate(cdo, settings, rate_text)
        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        cdo = unreal.get_default_object(blueprint.generated_class())
        result['after_compile'] = describe(cdo.get_editor_property(VARIABLE))
        result['saved'] = unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False)
    out_path = os.environ.get('CONVERTER_OUT', '')
    if out_path:
        with open(out_path, 'w') as handle:
            json.dump(result, handle, indent=1)
    unreal.log('CONVERTER_RATE_RESULT ' + json.dumps(result))


main()
