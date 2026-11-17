"""Second export batch (normal / packed maps of the chosen sources). Commandlet only, see ue_export_wear_candidates.py."""
CANDIDATES_OVERRIDE = [
    '/Game/MilitaryAirport/Textures/Tileable/T_Planks_N',
    '/Game/MilitaryAirport/Textures/Tileable/T_Planks_DRO',
    '/Game/ModularIndustrialPipe/Textures/T_metal_rust_3_Normal',
    '/Game/ModularIndustrialPipe/Textures/T_metal_rust_3_OcclusionRoughnessMetallic',
    '/Game/QuarryEnv/Textures/MeshTextures/T_Rust_N',
    '/Game/QuarryEnv/Textures/MeshTextures/T_Rust_DRO',
    '/Game/Megascans/Surfaces/Rusty_Painted_Metal_Sheet_xhtnbhlg/T_Rusty_Painted_Metal_Sheet_xhtnbhlg_2K_N',
    '/Game/Megascans/Surfaces/Rusty_Metal_vmghfhro/T_Rusty_Metal_vmghfhro_4K_N',
]
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/SupplyPoint/Wear/ue_export_wear_candidates.py').read())
