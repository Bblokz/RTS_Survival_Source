"""Adds the recon flight, flare curtain, air minefield and radixite nuke templates to DA_GlobalAbilitySystem.
Needs an editor built with UGA_StripFlight and the area aim settings. Re-running replaces the settings of templates that
already exist. Costs stay empty like every other global ability template; cooldowns are a first balance.
    exec(open(r"D:/UE5Projects/RTS_Survival/Source/Tools/GlobalAbilityTargetAreas/setup_target_area_abilities.py").read())
Run import_ability_icons.py afterwards for the icons."""
import unreal

DATA_ASSET = "/Game/RTS_Survival/Blueprints/GlobalAbilitySystem/DA_GlobalAbilitySystem"
AREAS = "/Game/RTS_Survival/Materials/AimMaterials/TargetAreas/"
AIRCRAFT = "/Game/RTS_Survival/Blueprints/Aircraft/"
MINES = "/Game/RTS_Survival/Blueprints/Squads/FieldConstructions/Mines/"
MINE_MESHES = "/Game/RTS_Survival/Meshes/Weapons/Mines/"
FLARE = "/Game/RTS_Survival/SpecialEffects/Flare/"
NUKE_FX = "/Game/RTS_Survival/SpecialEffects/RadixiteNuke/"
BEACON = "/Game/RTS_Survival/SpecialEffects/Smoke/SmokeVol1/Niagara/NS_GlobalAbility_Beacon.NS_GlobalAbility_Beacon"


def path(asset):
    return "%s.%s" % (asset, asset.split("/")[-1])


def bp(asset):
    return path(asset) + "_C"


def obj(asset):
    return '"%s"' % path(asset) if asset else "None"


FW189 = AIRCRAFT + "GerAircraft/FW_Recon/Ger_Aircraft_FW189_Recon"
PE2 = AIRCRAFT + "RusAircraft/PE2/Rus_PE2_Aircraft_Recon"
JU390 = AIRCRAFT + "GerAircraft/JU390B/Ger_Aircraft_JU390B_AirborneUnarmed"
ME410 = AIRCRAFT + "GerAircraft/ME410/Ger_Aircraft_ME5_Airbrone_Transport"
PE8 = AIRCRAFT + "RusAircraft/PE8Bomber/Rus_PE8_Aircraft_Transport"
V2 = "/Game/Ballistic_Missiles/Mesh/v2"
PARACHUTE = FLARE + "SM_Flare_Parachute"


def area_aim(shape, material, radius, inner=0.0, min_length=1.0, max_length=1.0, before_anchor=0.0, markers=0,
             drop_mesh=None, drop_material=None, drop_scale=3.0):
    return ('(AimType=None,AimRadius=500.000000,AreaAim=(Shape=%s,AreaMaterial=%s,AreaRadius=%f,InnerRadius=%f,'
            'MinStripLength=%f,MaxStripLength=%f,LengthBeforeAnchor=%f,ShaderMarkerCount=%d,DropPointMesh=%s,'
            'DropPointMaterial=%s,DropPointScale=%f))' % (shape, obj(AREAS + material), radius, inner, min_length,
                                                          max_length, before_anchor, markers, obj(drop_mesh),
                                                          obj(drop_material), drop_scale))


def strip_flight(aircraft, max_approach=25000.0, exit_distance=15000.0):
    return '(AircraftClass="%s",MaxApproachDistance=%f,ExitDistance=%f)' % (bp(aircraft), max_approach, exit_distance)


def recon(key, object_name, title, aircraft_name, aircraft):
    return dict(key=key, object_name=object_name, cls=unreal.GA_ReconFlight, title=title, cooldown=60,
                description="An unarmed %s flies the line you draw and lifts the fog along it for 25 seconds. "
                            "It carries no weapons: anti-air can shoot it down, and the reveal ends where it falls."
                            % aircraft_name,
                aim=area_aim("Strip", "M_TargetArea_Recon", 2500.0, min_length=2000.0, max_length=14000.0),
                properties={"M_StripFlightSettings": strip_flight(aircraft),
                            "M_ReconSettings": "(RevealSpacing=1200.000000,RevealHoldSeconds=25.000000)"})


def flare_visuals():
    return ('(LauncherMesh=None,FlareMesh=%s,ParachuteMesh=%s,LaunchEffect=None,TrailEffect=%s,BurnEffect=%s,'
            'LaunchSound=None,IgnitionSound=None,LightIntensity=160000.000000,LightAttenuationRadius=4500.000000,'
            'LightColor=(R=1.000000,G=0.780000,B=0.550000,A=1.000000),MeshScale=1.500000,BurnPointDepth=30.000000)'
            % (obj(FLARE + "SM_Flare_Cartridge"), obj(PARACHUTE), obj(FLARE + "NS_Flare_Trail"),
               obj(FLARE + "NS_Flare_Burn")))


def minefield(key, object_name, title, aircraft_name, aircraft, mine_classes, mine_mesh, ghost_scale, fall_scale):
    classes = ",".join('"%s"' % bp(MINES + name) for name in mine_classes)
    drop = '(FallingMesh=%s,ParachuteMesh=%s,MeshScale=%f,FallSeconds=3.000000)' % (
        obj(mine_mesh), obj(PARACHUTE), fall_scale)
    return dict(key=key, object_name=object_name, cls=unreal.GA_AirMinefield, title=title, cooldown=120,
                description="A %s flies the line you draw and scatters 14 mines on parachutes. Shorter lines give a "
                            "denser field. If it is shot down, the rest of its mines are lost." % aircraft_name,
                aim=area_aim("Strip", "M_TargetArea_Minefield", 900.0, min_length=1500.0, max_length=8000.0,
                             before_anchor=5000.0, drop_mesh=mine_mesh, drop_material=AREAS + "M_DropPointGhost",
                             drop_scale=ghost_scale),
                properties={"M_StripFlightSettings": strip_flight(aircraft),
                            "M_MinefieldSettings": "(MineClasses=(%s),MineCount=14,DropSettings=%s)" % (classes, drop)})


def nuke(key, object_name, title):
    visuals = ('(WarheadMesh="%s",WarheadScale=1.000000,WarheadRotation=(Pitch=-90.000000,Yaw=0.000000,Roll=0.000000),'
               'WarheadTrailEffect="%s",DetonationEffect="%s",DetonationEffectScale=3.000000,RadiationEffect="%s",'
               'WarningAreaMaterial="%s",ShockwaveMaterial="%s",ScarMaterial="%s",WarningSound="%s",DetonationSound="%s")'
               % (path(V2), path(FLARE + "NS_Flare_Trail"), path(NUKE_FX + "NS_RadixiteNuke"),
                  path("/Game/RTS_Survival/SpecialEffects/RadixiteVFX/NS_Radixite_Smoking1"),
                  path(AREAS + "M_TargetArea_RadixiteNuke"), path(AREAS + "M_RadixiteNukeShockwave"),
                  path(AREAS + "M_RadixiteNukeScar"),
                  path("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/RusBuildings/Megaphone/Sound/S_SirenStart"),
                  path("/Game/RTS_Survival/Sounds/Explosions/Debris/CityLarge/Cue_RTSDebris_HugeExplosion")))
    return dict(key=key, object_name=object_name, cls=unreal.GA_RadixiteNuke, title=title, cooldown=300,
                description="A radixite warhead lands 8 seconds after the order. Nothing survives the core, the blast "
                            "reaches 45 m and the ground stays poisoned for 30 seconds. It hurts both sides.",
                aim=area_aim("Circle", "M_TargetArea_RadixiteNuke", 4500.0, inner=1800.0),
                properties={"M_NukeVisuals": visuals})


ABILITIES = [
    recon("GA_GER_RECON_FLIGHT", "GA_ReconFlight_Ger", "Fw 189 Reconnaissance", "Fw 189", FW189),
    recon("GA_SOVIET_RECON_FLIGHT", "GA_ReconFlight_Sov", "Pe-2 Reconnaissance", "Pe-2", PE2),
    dict(key="GA_GER_FLARE_CURTAIN", object_name="GA_FlareCurtain_Ger", cls=unreal.GA_FlareCurtain,
         title="Flare Curtain", cooldown=90,
         description="A Ju 390 flies the line you draw and drops 5 parachute flares that light the area for 30 seconds.",
         aim=area_aim("Strip", "M_TargetArea_FlareCurtain", 2000.0, min_length=3000.0, max_length=10000.0,
                      before_anchor=8000.0, markers=5),
         properties={"M_StripFlightSettings": strip_flight(JU390),
                     "M_FlareCurtainSettings": "(IgnitionHeight=1200.000000,HangTimeSeconds=30.000000,"
                                               "FallSpeed=2500.000000,Visuals=%s)" % flare_visuals()}),
    minefield("GA_GER_AIR_MINEFIELD", "GA_AirMinefield_Ger", "Air-Dropped Minefield", "Me 410", ME410,
              ["Ger_AnitTank_MediumMine", "Ger_AnitTank_MediumMine", "Ger_AntiInfantry_FieldMine"],
              MINE_MESHES + "SM_TellerMine43", 1.5, 0.5),
    minefield("GA_SOVIET_AIR_MINEFIELD", "GA_AirMinefield_Sov", "Air-Dropped Minefield", "Pe-8", PE8,
              ["Rus_AnitTank_MediumMine", "Rus_AnitTank_MediumMine", "Rus_AntiInfantry_StickMine"],
              MINE_MESHES + "SM_TM41AT_Mine", 4.5, 1.5),
    nuke("GA_GER_RADIXITE_NUKE", "GA_RadixiteNuke_Ger", "Radixite V-Weapon"),
    nuke("GA_SOVIET_RADIXITE_NUKE", "GA_RadixiteNuke_Sov", "Radixite Warhead"),
]


def fresh(template, prop):
    """A new struct per template: edited copies of get_editor_property have leaked between templates before."""
    old = template.get_editor_property(prop)
    new = type(old)()
    new.import_text(old.export_text())
    return new


def configure(ability, key, entry):
    ability.set_editor_property("M_AbilityType", key)
    costs = fresh(ability, "M_AbilityCosts")
    costs.set_editor_property("CoolDownTime", entry["cooldown"])
    ability.set_editor_property("M_AbilityCosts", costs)

    marker = fresh(ability, "M_AbilityMarker")
    marker.import_text('(MarkerEffect="%s",MarkerColor=(R=1.000000,G=0.000000,B=0.000000,A=1.000000),'
                       'EffectTotalTime=15.000000)' % BEACON)
    ability.set_editor_property("M_AbilityMarker", marker)

    aim = fresh(ability, "M_AimSettings")
    aim.import_text(entry["aim"])
    ability.set_editor_property("M_AimSettings", aim)

    ui = fresh(ability, "M_UISettings")
    ui.set_editor_property("Title", unreal.Text("<Text_Title>%s</>" % entry["title"]))
    ui.set_editor_property("Description", unreal.Text(entry["description"]))
    ability.set_editor_property("M_UISettings", ui)

    for prop, text in entry["properties"].items():
        value = fresh(ability, prop)
        value.import_text(text)
        ability.set_editor_property(prop, value)


def setup():
    data_asset = unreal.load_asset(DATA_ASSET)
    templates = data_asset.get_editor_property("M_AbilityTemplatesByType")
    enum = type(list(templates.keys())[0])
    by_name = {key.name: value for key, value in templates.items()}
    data_asset.modify()
    for entry in ABILITIES:
        key = getattr(enum, entry["key"])
        ability = by_name.get(entry["key"]) or unreal.new_object(entry["cls"], outer=data_asset,
                                                                 name=entry["object_name"])
        configure(ability, key, entry)
        templates[key] = ability
    data_asset.set_editor_property("M_AbilityTemplatesByType", templates)
    print("SAVED_DA", unreal.EditorAssetLibrary.save_loaded_asset(data_asset, False))
    for entry in ABILITIES:
        ability = data_asset.get_editor_property("M_AbilityTemplatesByType")[getattr(enum, entry["key"])]
        print("TEMPLATE", entry["key"], ability.get_class().get_name(),
              ability.get_editor_property("M_AimSettings").export_text()[:120])


setup()
