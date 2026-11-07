# Prints every hint actor on Ger_Trains with its location (headless: -run=pythonscript).
import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/RTS_Survival/Maps/GerMissions/Ger_Trains')
out = []
for a in unreal.EditorLevelLibrary.get_all_level_actors():
    if 'Hint' in a.get_class().get_name():
        out.append('%s %s %s' % (a.get_actor_label(), a.get_class().get_name(), a.get_actor_location()))
open(unreal.Paths.project_saved_dir() + 'NomadicAirDelivery/trains_hints.txt', 'w').write('\n'.join(out))
