"""Configures the 9 Rus concrete walls for field holes + tank breaches (RTS.WallBreach.ConfigureRusWall, C++).
Run in a throwaway DebugGame editor or commandlet that has the new DLL (never the shared editor with an old DLL):
    UnrealEditor-Win64-DebugGame-Cmd.exe <uproject> -run=pythonscript -script=<this file> ...
Base walls first (they own the hole component and get the breach component + nav), then the _Soviet children
(only their damaged materials; everything else is inherited).
"""
import unreal

WALLS = '/Game/RTS_Survival/Blueprints/Buildings/Walls/RusWalls/'
HOLES = WALLS + 'Meshes/Holes/'
NAV_AREA = '/Game/RTS_Survival/Blueprints/Nav/SpecificNavTypeBlockers/RTS_NavArea_BlockCharLight'
NAV_BLOCKER = WALLS + 'RTS_BP_NavBloker_RusWall_TankPassable'
DUST = '/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/NomadicDeploy/FX/NS_Deploy_HeavyDust'
SOUND = '/Game/RTS_Survival/StarterContent/Audio/Collapse_Cue'
ATTENUATION = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
CONCURRENCY = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'
# SM_Wall_13: leg A along -X at y 0..20, leg B along +Y at x -1140..-1120, 5 m tall; centre line through both legs.
CORNER_PATH = '0,10;-1130,10;-1130,1160'

RUS_CONCRETE = HOLES + 'MI_Rus_BunkerConcrete_05_WallHolesField'
CORNER_RUS = HOLES + 'MI_SM_Wall_13_Rus_BunkerConcrete_05_WallHolesField'
CORNER_SOVIET = HOLES + 'MI_SM_Wall_13_SovietConcrete_WallHolesField'
LONG_SOVIET = HOLES + 'MI_SM_Wall_14_SovietConcrete_WallHolesField'
SAND_BAGS = HOLES + 'MI_SM_Rus_ConcreteSandWall_Rus_SandBag_01_WallHolesField'
SAND_RUS = HOLES + 'MI_SM_Rus_ConcreteSandWall_Rus_BunkerConcrete_05_WallHolesField'
SAND_SOVIET = HOLES + 'MI_SM_Rus_ConcreteSandWall_SovietConcrete_WallHolesField'

# (Blueprint, damaged materials per slot, path, thickness); thickness 0 = short side of the mesh bounds.
BASE_WALLS = [
    ('Rus_Wall_Concrete_Corner', [CORNER_RUS], CORNER_PATH, 20),
    ('Rus_Wall_Concrete_CornerNoFlag', [CORNER_RUS], CORNER_PATH, 20),
    ('Rus_Wall_Concrete_Long', [RUS_CONCRETE], '', 20),
    ('Rus_Wall_Concrete_Long_Holes', [RUS_CONCRETE], '', 20),
    ('Rus_Wall_Concrete_LongSandBags', [SAND_BAGS, SAND_RUS], '', 0),
    ('Rus_Wall_Concrete_LongSandBagsNoFlag', [SAND_BAGS, SAND_RUS], '', 0),
]
CHILD_WALLS = [
    ('Rus_Wall_Concrete_CornerNoFlag_Soviet', [CORNER_SOVIET]),
    ('Rus_Wall_Concrete_Long_Soviet', [LONG_SOVIET]),
    ('Rus_Wall_Concrete_LongSandBags_Soviet', [SAND_BAGS, SAND_SOVIET]),
]


def run(command):
    unreal.log('WALL_BREACH_SETUP ' + command)
    unreal.SystemLibrary.execute_console_command(None, command)


def configure_all():
    for name, materials, path, thickness in BASE_WALLS:
        command = (f'RTS.WallBreach.ConfigureRusWall Blueprint={WALLS}{name} Materials={"|".join(materials)} '
                   f'Thickness={thickness} NavArea={NAV_AREA} NavBlocker={NAV_BLOCKER} Dust={DUST} Sound={SOUND} '
                   f'Attenuation={ATTENUATION} Concurrency={CONCURRENCY}')
        if path:
            command += f' Path={path}'
        run(command)
    for name, materials in CHILD_WALLS:
        run(f'RTS.WallBreach.ConfigureRusWall Blueprint={WALLS}{name} Materials={"|".join(materials)}')


configure_all()
