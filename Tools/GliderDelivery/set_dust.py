"""Re-applies BP_GliderTankDelivery_Mammut with the game model (after a DUST or mesh change in the setup script)."""
import unreal
MAMMUT_SKIP_MAIN = True
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/GliderDelivery/setup_mammut_delivery.py', encoding='utf8').read())
MESH_FOLDER = '/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/Mammut/Meshes'
make_blueprint()
log('DUST DONE')
