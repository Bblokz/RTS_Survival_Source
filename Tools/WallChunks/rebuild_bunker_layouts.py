"""Rebuild every bunker chunk layout in place, keeping its slots, debris pieces and damaged materials.

Run inside the editor: exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/WallChunks/rebuild_bunker_layouts.py').read())
Use it after a change to RTS.WallChunks.BuildLayoutFromMesh (2026-11-09: debris pieces matched on bounds, not area).
"""
import collections
import unreal

BUNKER_CHUNK_MESHES = {
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/RusMgBunkerMeshes/Chunks/SM_RusBunker_MG_Chunks': 1,
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/MilitaryAnd05/Chunks/SM_Rus_Bunker05_Chunks': 2,
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/MilitaryAnd05/Chunks/SM_Rus_DomeBunker_Chunks': 3,
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/MilitaryAnd05/Chunks/SM_Rus_LongCamoBunker_Chunks': 2,
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/BunkerOnedCamoed/Chunks/SM_RusBunkerOneCamoed_Chunks': 1,
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Bunker03/Chunks/SM_RusBunker03_Chunks': 3,
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/Bunker02Meshes/Chunks/SM_RTS_RusBunker02_Chunks': 1,
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBunkers/AdminBunker/Chunks/SM_SquareBunker_Administrative_Chunks': 2,
    '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/GarrisonableBuildings/RusBunkers/Chunks/SM_RusInfBunker_03_Chunks': 2,
}


def get_layout(mesh):
    for user_data in (mesh.get_editor_property('asset_user_data') or []):
        if user_data and user_data.get_class().get_name() == 'WallChunkLayout':
            return user_data
    return None


def describe(layout):
    chunks = [c for c in layout.get_editor_property('M_Chunks') if c.get_editor_property('Area') > 0]
    pieces = collections.Counter(c.get_editor_property('DebrisPiece') for c in chunks)
    scales = [c.get_editor_property('DebrisScale') for c in chunks]
    return f"{len(chunks)} chunks, pieces {dict(sorted(pieces.items()))}, scale {min(scales):.2f}/{sum(scales) / len(scales):.2f}/{max(scales):.2f}"


def rebuild(mesh_path, uv_channel):
    mesh = unreal.load_asset(mesh_path)
    layout = get_layout(mesh) if mesh else None
    if not layout:
        print('LAYOUT_REBUILD SKIP no layout', mesh_path)
        return
    before = describe(layout)
    damaged = layout.get_editor_property('M_DamagedSlotMaterials')
    slots = [index for index, material in enumerate(damaged) if material]
    debris = [piece.get_path_name().split('.')[0] for piece in layout.get_editor_property('M_DebrisPieces')]
    damaged_paths = [material.get_path_name().split('.')[0] if material else 'None' for material in damaged]
    command = (f"RTS.WallChunks.BuildLayoutFromMesh Mesh={mesh_path} Slots={'|'.join(str(s) for s in slots)} "
               f"Debris={'|'.join(debris)} Damaged={'|'.join(damaged_paths)} UvChannel={uv_channel}")
    unreal.SystemLibrary.execute_console_command(None, command)
    after = describe(get_layout(unreal.load_asset(mesh_path)))
    print('LAYOUT_REBUILD', mesh_path.split('/')[-1], '\n  before', before, '\n  after ', after)


for path, uv in BUNKER_CHUNK_MESHES.items():
    rebuild(path, uv)
