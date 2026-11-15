"""Berlin furniture crush sounds: the one table every step reads (offline build, Unreal import, Blueprint build, test).

No Unreal import here, so plain Python can load it too.
A cue is Modulator -> Random (without replacement) -> one wave player per variation, in
/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Sounds/.
"""
SOUNDS_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/Furniture/Sounds'
CUE_VOLUME = 0.75                                        # as Cue_Collapse_Tree and the other collapse cues
MODULATOR = dict(pitch=(0.94, 1.06), volume=(0.88, 1.0))
# The crush plays the cue with these two (BP_BerlinFurniture_Base.SetupCrush); a death by damage does too.
ATTENUATION = '/Game/RTS_Survival/Sounds/Settings/Collapse/Att_Collapse'
CONCURRENCY = '/Game/RTS_Survival/Sounds/Settings/Collapse/SC_Collapse'

CUES = [
    dict(key='WoodSmall', variations=3, what='thin wood splintering: two cracks, a snap, bits clattering'),
    dict(key='WoodCabinet', variations=3, what='tall hollow cabinet: boom, crack and creak, glass and crockery inside'),
    dict(key='Upholstery', variations=3, what='stuffed seat: soft whump, muffled frame crack, springs'),
    dict(key='IronBed', variations=3, what='tube frame clang, spring mesh rattle and squeak, muffled mattress'),
    dict(key='Bathtub', variations=3, what='cast iron: deep dull boom, hard knock, dull enamel ring, scrape'),
    dict(key='Radiator', variations=3, what='cast iron: clang, ribs knocking in a run, pipe scrape'),
    dict(key='TiledStove', variations=3, what='masonry crunch, tiles cracking, dull ceramic shatter, tiles sliding'),
    dict(key='Washstand', variations=3, what='marble slab snapping, basin shattering, light wooden stand'),
    dict(key='Piano', variations=3, what='case boom and crack, all strings ringing out of tune, two strings letting go'),
    dict(key='Clock', variations=3, what='thin case crack, door glass, two sagging chime strikes, movement running down'),
]
DEFAULT_CUE = 'WoodSmall'                                # BP_BerlinFurniture_Base.CrushSound default

# piece (the NN_Name part of BP_BerlinFurn_<NN>_<Name>) -> cue key
PIECE_CUE = {
    '01_Wardrobe': 'WoodCabinet',
    '02_IronBed': 'IronBed',
    '03_KitchenTable': 'WoodSmall',
    '04_ChairFallen': 'WoodSmall',
    '05_Armchair': 'Upholstery',
    '06_Sofa': 'Upholstery',
    '07_Bathtub': 'Bathtub',
    '08_TiledStove': 'TiledStove',
    '09_Piano': 'Piano',
    '10_ChestOfDrawers': 'WoodSmall',
    '11_BookshelfFallen': 'WoodSmall',
    '12_KitchenDresser': 'WoodCabinet',
    '13_Desk': 'WoodSmall',
    '14_Washstand': 'Washstand',
    '15_ClockFallen': 'Clock',
    '16_Radiator': 'Radiator',
    # Second set (2026-11-14): no new cues; each piece plays the closest existing one. The modeller's table suggests a
    # group per piece (sound_group in furniture_table.json); all are taken as suggested except 31.
    '17_Toilet': 'Washstand',              # glazed ceramic bowl: the basin shatter
    '18_KitchenRange': 'TiledStove',       # tiled body, iron top
    '19_Sideboard': 'WoodCabinet',
    '20_RoundTableTipped': 'WoodCabinet',  # heavy table with a plate; WoodSmall would also do
    '21_Pram': 'WoodSmall',                # weak fit: wicker on a light iron chassis
    '22_SewingTable': 'IronBed',           # cast-iron frame and flywheel under a wooden top
    '23_Gramophone': 'WoodCabinet',        # the brass horn has no cue
    '24_TrunkAndSuitcases': 'WoodCabinet', # hollow trunk; the cue's crockery is a stretch
    '25_CoatStand': 'WoodCabinet',         # bench chest and mirror
    '26_CellarShelf': 'WoodCabinet',       # jars, crocks and bottles
    '27_WashTubMangle': 'Bathtub',         # weak fit: a zinc tub and a wooden mangle get the cast-iron boom
    '28_Vitrine': 'WoodCabinet',           # crockery on the shelves
    '29_LampAndRug': 'WoodSmall',          # weak fit: thin brass pole and a cloth shade
    '30_BarrelAndCrates': 'WoodCabinet',   # barrel and a crate of bottles
    '31_DoorLeafFallen': 'WoodSmall',      # suggested WoodCabinet; a flat door leaf has nothing hollow or breakable in it
    '32_HandCart': 'WoodSmall',
}


def cue_name(key):
    return 'Cue_BerlinFurn_Crush_' + key


def cue_path(key):
    return f'{SOUNDS_DIR}/{cue_name(key)}'


def wave_name(key, variation):
    return f'S_BerlinFurn_Crush_{key}_{variation + 1}'


def piece_cue_path(piece):
    return cue_path(PIECE_CUE[piece])
