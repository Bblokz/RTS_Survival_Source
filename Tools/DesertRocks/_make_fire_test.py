# One-off: derives rock_fire_test.py from the Berlin ruin kit real-fire test (kept for the record).
SRC = 'D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/ruinkit/ruin_holes_headless_test.py'
p = 'D:/UE5Projects/RTS_Survival/Source/Tools/DesertRocks/rock_fire_test.py'
s = open(SRC, encoding='utf-8').read()


def rep(old, new, count=1):
    global s
    if old not in s:
        raise SystemExit('not found: ' + old[:90])
    s = s.replace(old, new) if count == 0 else s.replace(old, new, count)


a = s.index('import json')
s = '''"""Throwaway-editor test of the desert rocks (ue_blueprints.py): real tank fire, holes, death, nav.
Derived from Tools/BerlinMap/ruinkit/ruin_holes_headless_test.py (same phases). Never in the shared editor; the map
(RTS_ToCopySmall) is never saved; the editor quits at the end. Launch: sh fire_test_run.sh <logname> [timeout_s].
Rocks stand in a row along X at y 0 (yaw 90), one player Pz IV G 35 m in front of each (+Y, sunlit side).
Results: Saved/DesertRocks/test/result.json + PNGs.
"""
''' + s[a:]
rep("ROOT = 'D:/UE5Projects/RTS_Survival/Saved/Berlin/ruinkit_test/'", "ROOT = 'D:/UE5Projects/RTS_Survival/Saved/DesertRocks/test/'")
rep("BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Berlin/RuinKit/Blueprints/'",
    "BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/'")
i = s.index('RUINS = {')
j = s.index('# B8 is rubble only')
s = s[:i] + '''import os
_ONLY = [k for k in os.environ.get('DROCK_ONLY', '').split(',') if k]
_ALL = {
    'L1_Block': (-3000, (1000, 2300, 900), (0, 0, 170)),
    'M4_Cracked': (2000, (450, 1200, 500), (0, 0, 70)),
}
RUINS = {k: v for k, v in _ALL.items() if not _ONLY or k in _ONLY}
''' + s[j:]
rep("NO_TANK = ('B8_DebrisBed',)", "NO_TANK = ()")
rep("KILL = ['E3_CoreRow', 'E5_Shell1St_A', 'B7_WallCore', 'B8_DebrisBed']", "KILL = list(RUINS)")
rep("BP_BerlinRuin_", "BP_DesertRock_", 0)
rep("if c.get_name() == 'StaticMesh'][0]", "if c.get_name() == 'MainMesh'][0]")
rep("if c.get_name().startswith('NavBlock_')]", "if c.get_name().startswith('NavBlock')]")
# health lives in the HealthComponent on ADestructableActorHealth
rep('''def log(message):''', '''def hp(actor):
    comp = actor.get_component_by_class(unreal.HealthComponent)
    for name in ('get_current_health', 'GetCurrentHealth'):
        fn = getattr(comp, name, None)
        if fn:
            return float(fn())
    return float(comp.get_editor_property('CurrentHealth'))


def log(message):''')
s = s.replace("ruin.get_editor_property('Health')", "hp(ruin)")
s = s.replace('ruin.get_editor_property("Health")', 'hp(ruin)')
s = s.replace("actor.get_editor_property(\"Health\")", "hp(actor)")
s = s.replace(' reduction {actor.get_editor_property("DamageReduction"):.0f}', '')
s = s.replace("S['pics'].append(('_warmup', view_of('B3_Facade4St')))", "S['pics'].append(('_warmup', view_of(list(RUINS)[0])))")
s = s.replace("close=('E1_CornerTower', 'B3_Facade4St', 'B7_WallCore')", "close=tuple(RUINS)")
s = s.replace("close=('E1_CornerTower', 'B3_Facade4St')", "close=tuple(RUINS)")
open(p, 'w', encoding='utf-8').write(s)
left = [w for w in ('BerlinRuin', 'E1_', 'B3_', 'B7_', "get_editor_property('Health')") if w in s]
print('ok, leftovers:', left)
