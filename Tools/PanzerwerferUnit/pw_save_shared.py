"""Job 7: force-save the three shared assets edited in memory by pw_register.py / pw_register_dt.py (async spawner,
DT_TrainingOptions, DA_TrainingLayout) and re-read the Panzerwerfer entries. Run only when no other process holds the
files (Restart Manager check). Run: python pw_uecp.py py pw_common.py pw_save_shared.py"""
assert_no_pie()
SPAWNER = '/Game/RTS_Survival/Blueprints/Player/RTS_BP_RTSAsyncSpawner'
DT = '/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions'
LAYOUT = '/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DA_TrainingLayout'
sp = load(SPAWNER)
rows = [r.export_text() for r in unreal.get_default_object(sp.generated_class()).get_editor_property('setup_light_ger_vehicles')
        if 'Panzerwerfer' in r.export_text()]
dt = load(DT)
has_row = 'Train_Tank_Panzerwerfer' in [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(dt)]
da = load(LAYOUT)
slot = 'Tank_Panzerwerfer' in da.get_editor_property('FactionLayouts')[unreal.RTSFaction.GER_STRIKE_DIVISION].get_editor_property(
    'Buildings')[unreal.NomadicSubtype.NOMADIC_GER_MECHANIZED_DEPOT].export_text()
print('in memory: spawner', rows, 'dt row', has_row, 'layout slot', slot)
assert rows and has_row and slot, 'an in-memory edit is missing: rerun pw_register.py / pw_register_dt.py first'
for a in (sp, dt, da):
    print('save', a.get_name(), save(a))
