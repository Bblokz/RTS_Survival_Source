# Unreal (editor python). Makes chosen deploy material instances two-sided (config "two_sided_slots").
# Kit walls are one-sided shells: on the finished building nobody sees their inside, but while a deploy plays the
# camera looks into the open box, the wall body is culled and only its trim (rails, mullions) shows, floating.
# Only the deploy mesh's own MI_<Building>_Deploy_<n> instances change; the building keeps its materials.
# The config list is the whole truth: a deploy instance of a slot that is not listed loses the override again.
# Leave out the truck slots and decal slots (decal quads facing away from the camera would show up).
# Globals: DEPLOY_MESH (imported VAT mesh), SLOTS (slot indices of the deploy mesh).
import unreal

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

deploy = unreal.load_asset(DEPLOY_MESH)
slots = deploy.get_editor_property('static_materials')
for index in SLOTS:
    mi = slots[index].material_interface
    if not isinstance(mi, unreal.MaterialInstanceConstant) or '_Deploy_' not in mi.get_name():
        raise RuntimeError('slot %d holds %s, not a deploy material instance (run the mis step first)'
                           % (index, mi.get_name() if mi else None))
for index, slot in enumerate(slots):
    mi = slot.material_interface
    if not isinstance(mi, unreal.MaterialInstanceConstant) or '_Deploy_' not in mi.get_name():
        continue
    wanted = index in SLOTS
    overrides = mi.get_editor_property('base_property_overrides')
    has = bool(overrides.get_editor_property('override_two_sided') and overrides.get_editor_property('two_sided'))
    if has == wanted:
        print('slot', index, mi.get_name(), 'two sided override', has, 'unchanged')
        continue
    overrides.set_editor_property('override_two_sided', wanted)
    overrides.set_editor_property('two_sided', wanted)
    mi.set_editor_property('base_property_overrides', overrides)
    MEL.update_material_instance(mi)
    back = mi.get_editor_property('base_property_overrides')
    print('slot', index, mi.get_name(), 'two sided override',
          bool(back.get_editor_property('override_two_sided') and back.get_editor_property('two_sided')),
          'saved', EAL.save_loaded_asset(mi, False))
