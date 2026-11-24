"""Job 5: component templates of Ger_Vehicle_Panzerwerfer (dup of the Pz 38(t) R) and Env_Dest_Panzerwerfer (dup of the
Sd.Kfz. 7 Flak wreck). Run: python pw_uecp.py py pw_common.py pw_bp_components.py   (idempotent)."""
assert_no_pie()
before = dirty_names()
UNIT = PW + '/Ger_Vehicle_Panzerwerfer'
WRECK = PW + '/Env_Dest_Panzerwerfer'
TURRET = TURRET_DIR + 'Ger_Turret_PanzerWerfer42'


def setp(o, name, value):
    o.modify()
    o.set_editor_property(name, value)


unit = load(UNIT)
h = handles(unit)
for comp in ('TrackMesh', 'Hull', 'HullMg', 'Turret', 'Selectionarea', 'SelectedDecal', 'RTS'):
    assert unit.generated_class().get_name() in h[comp][2].get_path_name(), comp + ' template is not the unit own'

track = h['TrackMesh'][2]
setp(track, 'skeletal_mesh_asset', load(MESHES + '/SK_PanzerWerfer_Tracks'))
setp(track, 'anim_class', load(PW + '/AnimBp_PanzerWerfer_Tracks').generated_class())
setp(track, 'override_materials', [])

hull = h['Hull'][2]
setp(hull, 'static_mesh', load(MESHES + '/SM_PanzerWerfer_Hull'))
setp(hull, 'override_materials', [])
setp(hull, 'relative_location', unreal.Vector(0, 0, 0))

mg = h['HullMg'][2]
setp(mg, 'static_mesh', load(MESHES + '/SM_PanzerWerfer_HullMg'))
setp(mg, 'override_materials', [])
setp(mg, 'relative_location', unreal.Vector(0, 0, 0))

turret = h['Turret'][2]
setp(turret, 'child_actor_class', load(TURRET).generated_class())
setp(turret, 'relative_location', unreal.Vector(0, 0, 0))

# Selection box and decal sized to the 5.9 m Maultier (Pz 38(t) R: box scale 7/3.5/3, decal 8/11 for 4.7 m).
sel = h['Selectionarea'][2]
setp(sel, 'relative_location', unreal.Vector(-50.0, 0.0, 120.0))
setp(sel, 'relative_scale3d', unreal.Vector(9.3, 3.5, 3.7))
dec = h['SelectedDecal'][2]
setp(dec, 'relative_location', unreal.Vector(-50.0, 0.0, -10.5))
setp(dec, 'relative_scale3d', unreal.Vector(1.0, 8.0, 13.5))

# Voice: the half-track armoured car family (Sd.Kfz. 250/251), not the Pz 38(t)'s light tank lines.
rts = h['RTS'][2]
setp(rts, 'UnitVoiceLineType', unreal.RTSVoiceLineUnitType.ARMORED_CAR)

unreal.BlueprintEditorLibrary.compile_blueprint(unit)
save(unit)

wreck = load(WRECK)
wh = handles(wreck)
whull = wh['Hull'][2]
assert wreck.generated_class().get_name() in whull.get_path_name()
setp(whull, 'static_mesh', load(MESHES + '/SM_Destroyed_PanzerWerfer'))
setp(whull, 'override_materials', [])
unreal.BlueprintEditorLibrary.compile_blueprint(wreck)
save(wreck)

# Verify on spawned instances (lesson: templates can lie for inherited values).
for cls_bp in (unit, wreck):
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(cls_bp.generated_class(), unreal.Vector(0, 0, -100000))
    try:
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            info = ''
            if isinstance(c, unreal.SkeletalMeshComponent):
                sk = c.get_editor_property('skeletal_mesh_asset')
                info = 'SK=%s anim=%s' % (sk.get_name() if sk else None, c.get_editor_property('anim_class').get_name()
                                          if c.get_editor_property('anim_class') else None)
            elif isinstance(c, unreal.StaticMeshComponent):
                sm = c.get_editor_property('static_mesh')
                info = 'SM=%s' % (sm.get_name() if sm else None)
            else:
                continue
            print('  ', a.get_class().get_name(), c.get_name(), info, 'parent', c.get_attach_parent().get_name()
                  if c.get_attach_parent() else None, c.get_attach_socket_name(),
                  'mats', [m.get_name() if m else None for m in c.get_materials()])
        for c in a.get_components_by_class(unreal.ChildActorComponent):
            print('   child', c.get_name(), c.get_editor_property('child_actor_class').get_name(), c.get_attach_socket_name(),
                  c.get_world_location())
        rts = a.get_component_by_class(unreal.RTSComponent)
        if rts:
            print('   voice', rts.get_editor_property('UnitVoiceLineType'))
    finally:
        a.destroy_actor()
print('DIRTY AFTER', [d for d in dirty_names() if d not in before])
