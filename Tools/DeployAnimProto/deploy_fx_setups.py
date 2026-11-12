# Deploy-effect setups for the VAT prototype (exec'd by deploy_fx_headless.py). PROTOTYPE, delete with the folder.
# GALLERY: (name, system path, scale) candidates shown side by side to pick building blocks.
# SETUPS[name]['events'][category] = [rule]; rule: system, scale, by_size (scale x size/300 clamped to min..max),
#   z (cm up from the cluster's ground point), spread (three spawns along long rows/walls).
# SETUPS[name]['continuous'] = [{system, at (mesh cm), scale, from, to (progress)}].
FX = '/Game/RTS_Survival/SpecialEffects/'
DEPLOY_FX = '/Game/RTS_Survival/_Prototypes/DeployAnim/VAT/FX/'
LAND_DUST = DEPLOY_FX + 'NS_Deploy_LandDust'
HEAVY_DUST = DEPLOY_FX + 'NS_Deploy_HeavyDust'
SPARKS = DEPLOY_FX + 'NS_Deploy_Sparks'
EXHAUST_PUFF = DEPLOY_FX + 'NS_Deploy_Exhaust'
CONSTRUCTION_SMOKE = FX + 'Smoke/ConstructionBuildingSmoke/NS_ConstructionSmokeRound_1'
SMOKE_PUFF = FX + 'Smoke/SmokeVol1/Niagara/NS_SmokePuff4a'
LIGHT_SMOKE = '/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke'
GALLERY = [
    ('LandDust', LAND_DUST, 1.0),
    ('LandDust15', LAND_DUST, 1.5),
    ('HeavyDust', HEAVY_DUST, 1.0),
    ('Sparks', SPARKS, 1.0),
    ('ConstrSmoke1', CONSTRUCTION_SMOKE, 1.0),
    ('SmokePuff04', SMOKE_PUFF, 0.4),
    ('LightSmoke', LIGHT_SMOKE, 1.0),
    ('LightSmoke15', LIGHT_SMOKE, 1.5),
]
SETUPS = {}
# Footprint ring and truck points in HQ mesh space (cm); the exhaust point is a guess at the SdKfz 7's front left.
FOOTPRINT = [(-700, -600, 0), (400, -650, 0), (-900, 300, 0), (300, 400, 0), (-150, -50, 0), (-500, 700, 0)]
EXHAUST = (200, -110, 260)  # just above the truck's front, so the cab never hides the puffs
WELD_POINTS = [(-200, -120, 250), (-200, 160, 250), (130, 30, 240)]
SETUPS = {
    # 1: grounded and quiet: every landing kicks up dust sized to the piece, the heavy pieces and the mast throw more
    'DustSettle': {
        'events': {
            'deck': [{'system': LAND_DUST, 'scale': 1.3, 'by_size': True, 'min': 0.8, 'max': 1.3, 'spread': True}],
            'sandbag': [{'system': LAND_DUST, 'scale': 1.0, 'spread': True}],
            'prop': [{'system': LAND_DUST, 'scale': 0.9, 'by_size': True, 'min': 0.7, 'max': 1.3}],
            'structure': [{'system': HEAVY_DUST, 'scale': 1.0, 'by_size': True, 'min': 0.8, 'max': 1.6}],
            'mast': [{'system': HEAVY_DUST, 'scale': 1.8}],
        },
    },
    # 2: mechanical: the truck's diesel runs the whole time, metal clangs throw sparks, welders flicker on the
    # containers after they land, light dust only
    'FieldEngineers': {
        'events': {
            'deck': [{'system': LAND_DUST, 'scale': 0.8}],
            'sandbag': [{'system': LAND_DUST, 'scale': 0.7}],
            'prop': [{'system': SPARKS, 'scale': 0.45, 'z': 20}],
            'structure': [{'system': SPARKS, 'scale': 0.8, 'z': 30}, {'system': LAND_DUST, 'scale': 1.0, 'by_size': True}],
            'antenna': [{'system': SPARKS, 'scale': 0.4, 'z': 10}],
            'mast': [{'system': SPARKS, 'scale': 1.2, 'z': 40}, {'system': HEAVY_DUST, 'scale': 1.2}],
        },
        'periodic': [
            {'system': EXHAUST_PUFF, 'at': [EXHAUST], 'scale': 1.0, 'from': 0.0, 'to': 0.92, 'every': 0.35},
            {'system': SPARKS, 'at': WELD_POINTS, 'scale': 0.35, 'from': 0.42, 'to': 0.86, 'every': 0.8},
        ],
    },
    # 3: cinematic: a thick light smoke cloud bursts around the footprint, the base emerges from it as it thins;
    # only the heavy pieces kick dust
    'SmokeScreen': {
        'events': {
            'structure': [{'system': HEAVY_DUST, 'scale': 1.1, 'by_size': True, 'min': 0.8, 'max': 1.4}],
            'mast': [{'system': HEAVY_DUST, 'scale': 1.6}],
        },
        'once': [
            {'system': LIGHT_SMOKE, 'at': FOOTPRINT, 'scale': 1.7, 'progress': 0.01},
            {'system': LIGHT_SMOKE, 'at': FOOTPRINT[:4], 'scale': 1.4, 'progress': 0.16},
            {'system': LIGHT_SMOKE, 'at': FOOTPRINT[2:], 'scale': 1.2, 'progress': 0.32},
        ],
    },
}
