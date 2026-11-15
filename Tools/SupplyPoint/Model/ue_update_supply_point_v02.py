"""v02 asset update (user feedback 2026-11-15): metal fills + metal load on M_RTS_MetalResource, radixite heaps resting
on the bay floor, radixite skip crystals seated on the skip floor. Reimports only those 12 meshes over the existing
assets. Run headless with the user's editor CLOSED (the packages are locked while it has them loaded):
  UnrealEditor-Cmd.exe D:/UE5Projects/RTS_Survival/RTS_Survival.uproject -run=pythonscript
      -script=D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Model/ue_update_supply_point_v02.py
      -unattended -nosplash -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Ends with SUPPLYMESH RESULT OK|FAIL.
"""
ONLY = ['SM_SupplyLoad_Metal', 'SM_SupplyLoad_Radixite'] + \
       [f'SM_SupplyPoint_{k}At{lv}' for k in ('Radixite', 'Metal') for lv in (10, 25, 50, 75, 100)]
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Model/ue_import_supply_point.py').read())
