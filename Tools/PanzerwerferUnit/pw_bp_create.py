"""Job 3: create the turret child (15 cm weapon), duplicate the unit BP from the Pz 38(t) R and the wreck from the
Sd.Kfz. 7 Flak wreck. Run: python pw_uecp.py py pw_common.py pw_bp_create.py   (skips assets that already exist)."""
assert_no_pie()
before = dirty_names()

TURRET = TURRET_DIR + 'Ger_Turret_PanzerWerfer42'
UNIT = PW + '/Ger_Vehicle_Panzerwerfer'
WRECK = PW + '/Env_Dest_Panzerwerfer'

if not EAL.does_asset_exist(TURRET):
    parent = load(TURRET_DIR + 'Ger_Turret_PanzerWerfer')
    unreal.BlueprintEditorLibrary.create_blueprint_asset_with_parent(TURRET, parent.generated_class())
turret = load(TURRET)
cdo = unreal.get_default_object(turret.generated_class())
cdo.modify()
# FName read by Global_GetWeaponNameFromFName (same as the Pz 38(t) rocket variants' turrets).
cdo.set_editor_property('Weapon Name', 'Panzerwerfer')
unreal.BlueprintEditorLibrary.compile_blueprint(turret)
print('turret weapon', unreal.get_default_object(turret.generated_class()).get_editor_property('Weapon Name'))
save(turret)

if not EAL.does_asset_exist(UNIT):
    EAL.duplicate_asset(PZ38 + 'Ger_Vehicle_Panzer38T_R', UNIT)
if not EAL.does_asset_exist(WRECK):
    EAL.duplicate_asset(TV + 'Ger_ArmoredCar/Sdkfz7_Flak/Env_Dest_Sdkfz7Flak', WRECK)
for p in (UNIT, WRECK):
    bp = load(p)
    for name, (h, d, o) in handles(bp).items():
        line = '  %s %s %s' % (name, o.get_class().get_name(), o.get_path_name().split('.')[-1][:90])
        for prop in ('static_mesh', 'skeletal_mesh_asset', 'child_actor_class', 'anim_class'):
            try:
                v = o.get_editor_property(prop)
                line += ' %s=%s' % (prop, v.get_name() if v else None)
            except Exception:
                pass
        print(line)
    save(bp)
print('DIRTY AFTER', [d for d in dirty_names() if d not in before])
