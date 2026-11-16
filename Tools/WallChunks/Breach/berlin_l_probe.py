"""Read-only: vertex layout of SM_BerlinRuin_B5_CornerL (legs of the L) for the breach centre line."""
import collections
import json
import unreal

mesh = unreal.EditorAssetLibrary.load_asset('/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Meshes/SM_BerlinRuin_B5_CornerL')
points = []
for section in range(mesh.get_num_sections(0)):
    data = unreal.ProceduralMeshLibrary.get_section_from_static_mesh(mesh, 0, section)
    points += [(v.x, v.y, v.z) for v in data[0]]
tall = [p for p in points if p[2] > 300]
out = dict(count=len(points), tall=len(tall),
           tall_x=sorted(collections.Counter(round(p[0] / 20) * 20 for p in tall).items()),
           tall_y=sorted(collections.Counter(round(p[1] / 20) * 20 for p in tall).items()))
json.dump(out, open('D:/UE5Projects/RTS_Survival/Saved/WallChunks/Breach/berlin_l_probe.json', 'w'), indent=1)
unreal.log('BERLIN_L written')
