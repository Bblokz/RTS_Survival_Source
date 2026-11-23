# Read-only: how much of the play area projects onto each RecastNavMesh (grid 25 m), plus tile counts if exposed.
import unreal

w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
navs = unreal.GameplayStatics.get_all_actors_of_class(w, unreal.RecastNavMesh)
for nav in navs:
    hit = tot = 0
    x = -25000
    while x <= 25000:
        y = -49000
        while y <= 49000:
            tot += 1
            p = unreal.NavigationSystemV1.project_point_to_navigation(w, unreal.Vector(x, y, 0), nav, None,
                                                                    unreal.Vector(100, 100, 2000))
            if p is not None and (abs(p.x) + abs(p.y) + abs(p.z)) > 0:
                hit += 1
            y += 2500
        x += 2500
    print("%-28s on nav %d / %d grid points" % (nav.get_name(), hit, tot))
