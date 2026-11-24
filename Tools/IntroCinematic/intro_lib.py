"""Shared helpers for the 1976 intro cinematic build scripts (run inside the throwaway editor, see intro_editor_server.py).

Safety rule learned the hard way (2026-11-14): EditorLevelUtils.add_level_to_world makes the added sublevel the
CURRENT level, so spawn_actor / save_current_level then act on the mission map. Never call save_current_level or
save_dirty_packages here. Use safe_save(), which refuses every package outside the cinematic folders, and call
make_persistent_current() before spawning anything.
"""
import unreal

ROOT = "/Game/RTS_Survival/Cinematics/Intro1976"
MAP = "/Game/RTS_Survival/Maps/Cinematics/RTS_IntroCinematic_1976"
SUBMAP_ROOT = "/Game/RTS_Survival/Maps/Cinematics/Intro1976"
OWN_PREFIXES = (ROOT + "/", MAP, SUBMAP_ROOT + "/")


def editor_world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def is_own(package_name):
    return any(package_name == p or package_name.startswith(p) for p in OWN_PREFIXES)


def safe_save(packages_or_names):
    """Saves only packages inside the cinematic folders; raises on anything else."""
    packages = []
    for item in packages_or_names:
        package = unreal.find_package(item) if isinstance(item, str) else item
        if package is None:
            raise RuntimeError("safe_save: package not loaded: %s" % item)
        name = package.get_name()
        if not is_own(name):
            raise RuntimeError("safe_save REFUSED (not a cinematic package): %s" % name)
        packages.append(package)
    ok = unreal.EditorLoadingAndSavingUtils.save_packages(packages, False)
    print("safe_save", [p.get_name() for p in packages], ok)
    return ok


def save_map():
    world = editor_world()
    if world.get_outermost().get_name() != MAP:
        raise RuntimeError("save_map: editor world is %s" % world.get_outermost().get_name())
    return safe_save([world.get_outermost()])


def make_persistent_current():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.set_current_level_by_name("RTS_IntroCinematic_1976")
    path = les.get_current_level().get_path_name()
    if not path.startswith(MAP + "."):
        raise RuntimeError("current level is not the cinematic map: %s" % path)
    return path


def foreign_dirty():
    """Dirty packages that are not ours (never save these)."""
    utils = unreal.EditorLoadingAndSavingUtils
    dirty = [p.get_name() for p in utils.get_dirty_map_packages()] + [p.get_name() for p in utils.get_dirty_content_packages()]
    return [n for n in dirty if not is_own(n)]
