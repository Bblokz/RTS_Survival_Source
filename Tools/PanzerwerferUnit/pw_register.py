"""Job 6: register Tank_Panzerwerfer for training: async spawner row and the Strike Division Mechanized Depot layout
slot (Recons, slot 6). The DT_TrainingOptions row is added by pw_register_dt.py (UECP add_data_table_rows).
Run: python pw_uecp.py py pw_common.py pw_register.py   (idempotent)."""
assert_no_pie()
before = dirty_names()
SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
LAYOUT = '/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DA_TrainingLayout'
UNIT_CLASS = PW + '/Ger_Vehicle_Panzerwerfer.Ger_Vehicle_Panzerwerfer_C'
LAYOUT_SUBSECTION = 0   # Recons (251 transport, 140, 221, 251 mortar, 251/22, ...)
LAYOUT_SLOT = 5         # first empty slot after the 251/22
dirty_shared = [d for d in before if d.split('/')[-1] in ('DA_TrainingLayout',)]
if dirty_shared:
    raise RuntimeError('shared asset already dirty (another session?): %s' % dirty_shared)

# ---------------- async spawner ----------------
sp = load(SPAWNER)
cdo = unreal.get_default_object(sp.generated_class())
arrays = ('setup_light_ger_vehicles', 'setup_medium_ger_vehicles', 'setup_heavy_ger_vehicles')
present = None
target = None
for arr_name in arrays:
    for row in cdo.get_editor_property(arr_name):
        t = row.export_text()
        if 'Tank_Panzerwerfer' in t:
            present = (arr_name, t)
        if 'TankSubtype=Tank_Pz38t_R,' in t:
            target = arr_name
counts_before = {a: len(cdo.get_editor_property(a)) for a in arrays}
print('Pz38t R array', target, 'present', present, counts_before)
if present is None:
    arr = list(cdo.get_editor_property(target))
    row = unreal.TankTrainingOptionSetup()
    row.import_text('(TankSubtype=Tank_Panzerwerfer,UnitClass="%s")' % UNIT_CLASS)
    assert 'Ger_Vehicle_Panzerwerfer_C' in row.export_text(), row.export_text()
    arr.append(row)
    cdo.modify()
    cdo.set_editor_property(target, arr)
    unreal.BlueprintEditorLibrary.compile_blueprint(sp)
    save(sp)
print('spawner rows', {a: len(cdo.get_editor_property(a)) for a in arrays},
      [r.export_text() for r in cdo.get_editor_property(target) if 'Panzerwerfer' in r.export_text()])

# ---------------- training layout ----------------
da = load(LAYOUT)
fl = da.get_editor_property('FactionLayouts')
fac = unreal.RTSFaction.GER_STRIKE_DIVISION
lay = fl[fac]
buildings = lay.get_editor_property('Buildings')
md = buildings[unreal.NomadicSubtype.NOMADIC_GER_MECHANIZED_DEPOT]
text = md.export_text()
print('layout before', text)
if 'Tank_Panzerwerfer' not in text:
    subs = md.get_editor_property('SubSections')
    sub = subs[LAYOUT_SUBSECTION]
    slots = sub.get_editor_property('Slots')
    assert 'UnitType=UNType_None' in slots[LAYOUT_SLOT].export_text(), slots[LAYOUT_SLOT].export_text()
    new_slot = type(slots[LAYOUT_SLOT])()
    new_slot.import_text('(UnitType=UNType_Tank,TankSubtype=Tank_Panzerwerfer)')
    slots[LAYOUT_SLOT] = new_slot
    sub.set_editor_property('Slots', slots)
    subs[LAYOUT_SUBSECTION] = sub
    md.set_editor_property('SubSections', subs)
    buildings[unreal.NomadicSubtype.NOMADIC_GER_MECHANIZED_DEPOT] = md
    lay.set_editor_property('Buildings', buildings)
    fl[fac] = lay
    da.modify()
    da.set_editor_property('FactionLayouts', fl)
    save(da)
da = load(LAYOUT)
print('layout after', da.get_editor_property('FactionLayouts')[fac].get_editor_property('Buildings')[
    unreal.NomadicSubtype.NOMADIC_GER_MECHANIZED_DEPOT].export_text())
print('DIRTY AFTER', [d for d in dirty_names() if d not in before])
