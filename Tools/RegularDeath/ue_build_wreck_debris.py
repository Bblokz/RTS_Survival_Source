"""Finishes NS_WreckDebrisBurst's mesh renderer: debris meshes with their scales and the material bound to
User.DebrisMaterial. The editor tooling cannot reach these fields, so UWreckDebrisAssetTools does it in C++.

Run headless with the DebugGame editor (the helper is editor-only C++):
  UnrealEditor-Win64-DebugGame-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nop4 -NullRHI
The module stack (burst bound to User.PieceCount, cone to User.ThrowDirection/ConeAngle, scale to User.DebrisScale,
gravity, CPU ray-traced collision with rest, random tumble) was built in the editor; see Docs or the asset itself.
"""
import unreal

SYSTEM = '/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckDebris/NS_WreckDebrisBurst'
CHUNK_MATERIAL = '/Game/RTS_Survival/SpecialEffects/AllExplosions/WreckDebris/M_WreckDebrisChunk'
MESH_ROOT = '/Game/RTS_Survival/SpecialEffects/AllExplosions/Mesh/'
# Armour chunks are ~8 cm meshes thrown at ~0.6 m; the engine deck parts are full size and shrink to bits of plate.
MESHES = [
    ('SM_Derbis_1', 6.5),
    ('SM_Derbis_2', 7.5),
    ('SM_Derbis_3', 7.5),
    ('SM_Derbis_1', 4.5),
    ('EngineDeck/SM_EngineDeck_Plate_B', 0.55),
    ('EngineDeck/SM_EngineDeck_Grille', 0.45),
    ('EngineDeck/SM_EngineDeck_HatchLid', 0.5),
    ('EngineDeck/SM_EngineDeck_Muffler', 0.5),
    ('EngineDeck/SM_EngineDeck_Fan', 0.5),
]

system = unreal.load_asset(SYSTEM)
material = unreal.load_asset(CHUNK_MATERIAL)
meshes = [unreal.load_asset(MESH_ROOT + name) for name, _ in MESHES]
scales = [scale for _, scale in MESHES]
if system is None or material is None or None in meshes:
    raise RuntimeError('RTS_WDB missing asset')
ok = unreal.WreckDebrisAssetTools.setup_debris_mesh_renderer(system, meshes, scales, 'DebrisMaterial', material)
print('RTS_WDB setup', ok)
print('RTS_WDB saved', unreal.EditorAssetLibrary.save_asset(SYSTEM, False))
