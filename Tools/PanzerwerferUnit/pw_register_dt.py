"""Job 6b (outside the editor): DT_TrainingOptions row Train_Tank_Panzerwerfer (icon borrowed from the Pz 38(t) R;
no Panzerwerfer placard exists). Saving is done by pw_save_shared.py once no -game run holds the table.
Run: MSYS_NO_PATHCONV=1 python pw_register_dt.py"""
import json

import pw_uecp as u

DT = '/Game/RTS_Survival/Blueprints/GameUI/TrainingUI/DT_TrainingOptions'
ROW = 'Train_Tank_Panzerwerfer'
have = u.act('get_data_table_rows', table_path=DT, row_names=[ROW])
if have.get('returned'):
    print('row exists', json.dumps(have['rows'])[:600])
else:
    fields = {
        'ImageBrush': "/Script/Engine.SlateBrushAsset'/Game/RTS_Survival/Textures/GameUI/UnitIcons/Ger/LightTanks/"
                      "T_Ger_LightTank_Panzer38t_R_Brush.T_Ger_LightTank_Panzer38t_R_Brush'",
        'Description': 'NSLOCTEXT("TrainingOptions", "Train_Tank_Panzerwerfer_Description", "A Maultier half-track '
                       'with a ten-tube <Text_Armor>15 cm rocket launcher</> that saturates an area at artillery '
                       'range. <Text_Bad>Thin armor</> and a long reload of the whole rack.")',
        'Tooltip': 'NSLOCTEXT("TrainingOptions", "Train_Tank_Panzerwerfer_Tooltip", "Panzerwerfer 42")',
        'Title': 'NSLOCTEXT("TrainingOptions", "Train_Tank_Panzerwerfer_Title", "<Text_Title>Panzerwerfer 42</>")',
    }
    r = u.act('add_data_table_rows', table_path=DT, rows=[dict(row_name=ROW, **fields)])
    print('add', json.dumps(r)[:800])
print(json.dumps(u.act('get_data_table_rows', table_path=DT, row_names=[ROW, 'Train_Tank_Pz38t_R']))[:2500])
