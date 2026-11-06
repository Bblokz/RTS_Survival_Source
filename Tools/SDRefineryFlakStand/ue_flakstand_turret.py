"""2-crew Flak 37 turret for the SD refinery flak stand (run in the editor's Python, e.g. UECP execute_python).

A copy of the Flak 37 pit turret (RTS_BP_EmbTurret_GerPit_Flak37: same gun mesh, weapon, anim BP and fire feedback)
with only the gunner and the loader:
  Crew1  loader   pit loader spot (-97.9, 85.1, 0), yaw -90, Anim_GerPitFlak_Loader_Idle / Montage_GerPit_LoadFlak
  Crew2  gunner   seated at the left handwheels (-5, -55, 0), yaw -90, AN_Infantry_Flak_ControlWheels (team weapon gunner)
Removed: Crew3 (spotter), Crew4 (commander), ReadyCrate and both PitRound clips, so there is no round hand-over
(GunPitCrewComponent::GetHasShellCarry is false without the two rounds; the loader's drill plays with empty hands, as on
the Flakvierling bunker). make_turret() is safe to rerun: it deletes and re-copies the turret.
"""
import unreal

SOURCE = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerPits/Turrets/RTS_BP_EmbTurret_GerPit_Flak37'
HOME = '/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/GerBxps/Refinery/FlakStand'
TURRET = HOME + '/RTS_BP_EmbTurret_SD_FlakStand37'
GUNNER_IDLE = '/Game/RTS_Survival/Blueprints/Squads/SquadUnits/Anims/TeamWeaponAnims/Flak/AN_Infantry_Flak_ControlWheels'
REMOVE = ('Crew3', 'Crew4', 'ReadyCrate', 'Round1', 'Round2')
GUNNER = ('Crew2', unreal.Vector(-5.0, -55.0, 0.0), -90.0)

SUBOBJECTS = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
LIB = unreal.SubobjectDataBlueprintFunctionLibrary


def handles(bp):
    out = {}
    for h in SUBOBJECTS.k2_gather_subobject_data_for_blueprint(bp):
        data = LIB.get_data(h)
        obj = LIB.get_object(data)
        if obj is not None:
            out[obj.get_name().replace('_GEN_VARIABLE', '')] = (h, obj)
    return out


def make_turret():
    eal = unreal.EditorAssetLibrary
    if eal.does_asset_exist(TURRET):
        eal.delete_asset(TURRET)
    bp = eal.duplicate_asset(SOURCE, TURRET)
    found = handles(bp)
    root = SUBOBJECTS.k2_gather_subobject_data_for_blueprint(bp)[0]
    doomed = [found[n][0] for n in REMOVE if n in found]
    removed = SUBOBJECTS.delete_subobjects(root, doomed, bp)
    print('REMOVED', removed, [n for n in REMOVE if n in found])
    found = handles(bp)
    name, loc, yaw = GUNNER
    gunner = found[name][1]
    gunner.set_editor_property('relative_location', loc)
    gunner.set_editor_property('relative_rotation', unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    crew = found['PitCrew'][1]
    reload = crew.get_editor_property('ReloadSettings')
    anims = list(reload.get_editor_property('CrewAnimations'))
    gunner_anims = unreal.GunPitCrewMemberAnimations()
    # EditDefaultsOnly struct members refuse set_editor_property; import_text writes them
    gunner_anims.import_text('(IdleAnimation="%s.%s")' % (GUNNER_IDLE, GUNNER_IDLE.split('/')[-1]))
    reload.set_editor_property('CrewAnimations', [anims[0], gunner_anims])
    crew.set_editor_property('ReloadSettings', reload)
    carry = crew.get_editor_property('ShellCarry')
    carry.import_text('(HandlerCrewIndex=-1,LoaderCrewIndex=-1)')
    crew.set_editor_property('ShellCarry', carry)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    ok = unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    print('TURRET', TURRET, 'saved', ok)
    report(bp)
    return bp


def report(bp=None):
    bp = bp or unreal.load_asset(TURRET)
    for name, (h, c) in sorted(handles(bp).items()):
        line = name + ' ' + c.get_class().get_name()
        if isinstance(c, unreal.SceneComponent):
            l, r = c.get_editor_property('relative_location'), c.get_editor_property('relative_rotation')
            line += ' loc(%.1f, %.1f, %.1f) yaw %.1f tags %s' % (l.x, l.y, l.z, r.yaw, [str(t) for t in c.component_tags])
        if c.get_class().get_name() == 'GunPitCrewComponent':
            rs, sc = c.get_editor_property('ReloadSettings'), c.get_editor_property('ShellCarry')
            line += ' anims %s carry handler %d loader %d' % (
                [(a.get_editor_property('IdleAnimation').get_name() if a.get_editor_property('IdleAnimation') else None,
                  a.get_editor_property('ReloadMontage').get_name() if a.get_editor_property('ReloadMontage') else None)
                 for a in rs.get_editor_property('CrewAnimations')],
                sc.get_editor_property('HandlerCrewIndex'), sc.get_editor_property('LoaderCrewIndex'))
        print(line)
