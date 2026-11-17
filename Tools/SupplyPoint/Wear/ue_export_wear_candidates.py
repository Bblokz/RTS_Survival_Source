"""Exports candidate rust / paint / wood textures for the supply point wear pass to .hdr (from the texture source data).
Commandlet only, never the shared editor (forced PNG exports have killed it):
  UnrealEditor-Cmd.exe RTS_Survival.uproject -run=pythonscript -script=<this file> -unattended -nosplash
      -DisablePlugins=Fab,BpGeneratorUltimate -NoLiveCoding
Prints WEAREXPORT lines.
"""
import unreal

OUT = 'D:/UE5Projects/RTS_Survival/Saved/SupplyPointWear/tex'
CANDIDATES = globals().get('CANDIDATES_OVERRIDE') or [
    '/Game/Megascans/Surfaces/Rusty_Painted_Metal_Sheet_xhtnbhlg/T_Rusty_Painted_Metal_Sheet_xhtnbhlg_2K_D',
    '/Game/Megascans/Surfaces/Rusty_Painted_Metal_Sheet_xhtnbhlg/T_RustyPaintedMetalSheet_xhtnbhlg_2K_ORDp',
    '/Game/Megascans/Surfaces/Rusty_Painted_Metal_Sheet_vckjca2/T_Rusty_Painted_Metal_Sheet_vckjca2_2K_D',
    '/Game/Megascans/Surfaces/Rusty_Painted_Metal_Sheet_vckjca2/T_RustyPaintedMetalSheet_vckjca2_2K_ORDp',
    '/Game/Megascans/Surfaces/Rusty_Metal_vmghfhro/T_Rusty_Metal_vmghfhro_4K_D',
    '/Game/Megascans/Surfaces/Rusty_Metal_vmghfhro/T_RustyMetal_vmghfhro_4K_ORDp',
    '/Game/RTS_Survival/Materials/Buildings/HQ/Textures/T_ScratchedPaintedMetal_tduwej0c_4K_MR',
    '/Game/Megascans/Surfaces/Scratched_Metal_vdekfdgc/T_ScratchedMetal_vdekfdgc_4K_MR',
    '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Bunkers/FlakBunker/Material/T_BF_ConcreteNoise',
    '/Game/UVB80/Assets/UVB80/Buildings/Textures/T_LeakMask',
    '/Game/UVB80/Assets/UVB80/Buildings/Textures/T_WindowDirtLeaksGrunge',
    '/Game/Greek_island/Textures/T_Mask/T_grunge_mask',
    '/Game/ModularIndustrialPipe/Textures/T_metal_rust_3_BaseColor',
    '/Game/PostApoPowerplants/Textures/T_rusted_metal_bc',
    '/Game/QuarryEnv/Textures/MeshTextures/T_Rust_BC',
    '/Game/UVB80/Assets/UVB80/Buildings/Textures/T_Plane_RustyMetal_BaseColor',
    '/Game/MilitaryAirport/Textures/Tileable/T_Planks_BC',
    '/Game/MilitaryAirport/Textures/Tileable/T_Planks_BC_2',
    '/Game/ApocalypticCity/Textures/Wood_Planks_tex',
    '/Game/ApocalypticCity/Textures/Wood_Plank_tex',
    '/Game/MillitaryBase/Textures/T_Board_BoardWood_BaseColor',
    '/Game/MilitaryBaseCamp/Textures/FloorPlanks/T_floorplank_BaseColor',
    '/Game/Megascans/3D_Assets/Wooden_Crate_wlkjbhn/T_Wooden_Crate_wlkjbhn_1K_D',
    '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT1/OpelBlitz_Barracks/TempTentBarracks/Wood026_2K_Color',
    '/Game/Greek_island/Textures/T_wood/T_wood_D',
    '/Game/Hangar/Textures/T_constraction_set_wood_BC',
    '/Game/Bunkers/Textures/T_wood_bc',
    '/Game/LowPolyForest/Textures/T_Wood_Planks_01',
    '/Game/RTS_Survival/Cinematics/Intro1976/Sets/Textures/T_Intro_Wood_BaseColor',
    '/Game/RTS_Survival/Blueprints/Environment/Prypyat/Airfield/Textures/T_Prypyat_PropWood_base_color',
]


def main():
    for path in CANDIDATES:
        tex = unreal.load_asset(path)
        if tex is None or not isinstance(tex, unreal.Texture2D):
            unreal.log(f'WEAREXPORT MISSING {path}')
            continue
        name = path.split('/')[-1] + '.hdr'
        try:
            unreal.RenderingLibrary.export_texture2d(None, tex, OUT, name)
            unreal.log(f'WEAREXPORT OK {name} {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}')
        except Exception as error:  # noqa: BLE001
            unreal.log(f'WEAREXPORT FAIL {path} {error}')
    unreal.log('WEAREXPORT DONE')


main()
