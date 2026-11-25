"""Read-only (commandlet): supply point actors on Ger_Berlin - label, class, location, folder, owner settings."""
import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/RTS_Survival/Maps/GerMissions/Ger_Berlin')
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    name = a.get_class().get_name()
    if 'SupplyPoint' in name or 'SupplyTruck' in name or 'Supply' in a.get_actor_label():
        loc = a.get_actor_location()
        extra = ''
        for prop in ('initial_owning_player', 'enemy_can_capture', 'capture_radius', 'supply_interval_seconds'):
            try:
                extra += f' {prop}={a.get_editor_property(prop)}'
            except Exception:
                pass
        unreal.log_warning(f'BERLIN_SUPPLY {a.get_actor_label()} class={name} at ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f}) '
                           f'folder={a.get_folder_path()}{extra}')
