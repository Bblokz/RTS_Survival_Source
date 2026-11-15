"""Adds the sunken Demeter pod to the open Ger_PripyatMe410 level without touching anything else. Does not save.

Places exactly what a dress rebuild would (group SunkenDemeterPod + note sunken_pod from Pripyat_dress.json), so it can
be run in any editor that has the map open, the user's own included (Output Log, Cmd):

    py "D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/pripyat_place_sunken_pod.py"

Safe to rerun: it removes its own two actors first.
"""
import importlib
import sys

import unreal

sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap")
import pripyat_map_build as pmb

importlib.reload(pmb)
GROUP, NOTE = "SunkenDemeterPod", "sunken_pod"


def place():
    pmb.require_map()
    pmb.load_layout()
    data = pmb.dress()
    spec = data["inst"][GROUP]
    for label in (pmb.PREFIX + "I_" + GROUP, pmb.PREFIX + "Note_" + NOTE):
        for actor in pmb.all_actors():
            if actor.get_actor_label() == label:
                pmb.actors_sub().destroy_actor(actor)
    mesh = unreal.load_asset(spec["mesh"])
    if mesh is None:
        raise RuntimeError("missing mesh " + spec["mesh"])
    transforms = [unreal.Transform(unreal.Vector(x, y, spec["abs_z"]), unreal.Rotator(roll, pitch, yaw), unreal.Vector(scale, scale, scale))
                  for x, y, dz, yaw, scale, pitch, roll in spec["items"]]
    pmb._instance_actor(pmb.PREFIX + "I_" + GROUP, pmb.FOLDER + "Instances", mesh, transforms, collision=True)
    for nid, words, x, y in data["notes"]:
        if nid != NOTE:
            continue
        actor = pmb.actors_sub().spawn_actor_from_class(unreal.TextRenderActor, unreal.Vector(x, y, pmb.ground_z(x, y) + 320.0), unreal.Rotator(0.0, 90.0, 180.0))
        comp = actor.text_render
        comp.set_text(words)
        comp.set_world_size(300.0)
        comp.set_text_render_color(unreal.Color(255, 230, 40, 255))
        comp.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
        actor.set_actor_label(pmb.PREFIX + "Note_" + nid)
        actor.set_folder_path(pmb.FOLDER + "Notes")
        actor.set_editor_property("is_editor_only_actor", True)
    x, y = spec["items"][0][:2]
    pmb.log("sunken pod placed at %.0f %.0f, water z %.0f, bed z %.0f (not saved)" % (x, y, spec["abs_z"], pmb.ground_z(x, y)))


place()
