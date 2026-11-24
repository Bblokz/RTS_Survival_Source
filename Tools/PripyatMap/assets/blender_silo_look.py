"""Quick look at SM_Quarry_Silo3/4 (shape only; the FBX carries no textures)."""
import sys
sys.path.insert(0, 'D:/UE5Projects/RTS_Survival/Source/Tools/PripyatMap/assets')
import bpy
import pry_blender_lib as L
L.reset_scene()
for i, n in enumerate(('SM_Quarry_Silo3', 'SM_Quarry_Silo4')):
    bpy.ops.import_scene.fbx(filepath='D:/Blender/BlenderProjects/BlenderTanks/AA_LaptopWork/Quarry/%s.FBX' % n)
    for o in bpy.context.selected_objects:
        o.location.x += i * 2200
        if o.type == 'MESH':
            o.data.materials.clear()
            o.data.materials.append(L.flat_material('Concrete', (0.45, 0.43, 0.40), 0.9))
L.render_preview('D:/UE5Projects/RTS_Survival/Saved/Pripyat/assets/silo_look.png', target=(1100, 0, 700), distance=5200,
                 elevation_deg=35, azimuth_deg=-60)
