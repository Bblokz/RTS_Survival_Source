"""Headless job: copy LakeTourShots from fl_stages.py (ARRIVAL cdo) onto Mission_FL_01_Arrival's CDO and save.

The same value build_fl_stages.py sets over UECP (set_blueprint_variable_default); use this when no UECP editor should
be started. From PowerShell:
  UnrealEditor.exe D:/UE5Projects/RTS_Survival/RTS_Survival.uproject
    -ExecutePythonScript=D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap/mission/run_set_tour_shots.py
    -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding -unattended -nosplash -abslog=<log>
Writes Saved/FrozenLake/cdo_Mission_FL_01_Arrival_tour.txt (the shots read back after the save).
"""
import sys

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/FrozenLakeMap/mission")
from fl_stages import ARRIVAL, path_of  # noqa: E402


def split_top_level(text):
    """'((a),(b))' -> ['(a)', '(b)'] by bracket depth (quotes are skipped)."""
    inner = text.strip()[1:-1]
    parts, depth, start, quoted = [], 0, 0, False
    for k, ch in enumerate(inner):
        if ch == '"':
            quoted = not quoted
        if quoted:
            continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        elif ch == "," and depth == 0:
            parts.append(inner[start:k].strip())
            start = k + 1
    parts.append(inner[start:].strip())
    return [p for p in parts if p]


def main():
    path = path_of(ARRIVAL)
    bp = unreal.load_asset(path)
    cls = unreal.load_class(None, path + "." + path.rsplit("/", 1)[1] + "_C")
    cdo = unreal.get_default_object(cls)
    shots = []
    for text in split_top_level(ARRIVAL["cdo"]["LakeTourShots"]):
        shot = unreal.MissionCameraTourShot()
        shot.import_text(text)
        shots.append(shot)
    cdo.set_editor_property("LakeTourShots", shots)
    cdo.modify()
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    back = unreal.get_default_object(cls).get_editor_property("LakeTourShots")
    report = [str(saved)] + [s.export_text() for s in back]
    with open(unreal.Paths.project_saved_dir() + "FrozenLake/cdo_Mission_FL_01_Arrival_tour.txt", "w") as handle:
        handle.write("\n".join(report))
    unreal.log_warning("FL_TOUR saved=%s shots=%d" % (saved, len(back)))


main()
