# One-off: derives rock_crush_test.py from the Ore crushable rock engine test (kept for the record).
SRC = 'D:/Blender/BlenderProjects/BlenderTanks/CustomMapItems/Environment/OreCrushableRocks/rock_engine_test.py'
p = 'D:/UE5Projects/RTS_Survival/Source/Tools/DesertRocks/rock_crush_test.py'
s = open(SRC, encoding='utf-8').read()


def rep(old, new):
    global s
    if old not in s:
        raise SystemExit('not found: ' + old[:90])
    s = s.replace(old, new)


def between(start, end, new):
    global s
    i = s.index(start)
    j = s.index(end)
    s = s[:i] + new + s[j:]


a = s.index('import json')
s = '''"""Throwaway-editor crush test of the desert rocks (ue_blueprints.py): crush lanes, navigation, death by damage.
Derived from the Ore rock_engine_test.py (look phase dropped). Never in the shared editor; RTS_ToCopySmall is never
saved; the editor quits at the end. Launch: ROCK_SCRIPT=rock_crush_test.py sh fire_test_run.sh <logname> 560
Lanes along world Y; each vehicle starts 24 m before its rock and is ordered 24 m past it:
  A medium M4_Cracked (MediumOrHeavy)   Luchs (light)  -> path round, rock untouched
  B medium M1_Block   (MediumOrHeavy)   Pz IV G        -> crush
  C medium M7_Long    (MediumOrHeavy)   Pz IV G        -> crush
  D large  L1_Block   (not crushable)   Pz IV G        -> path round, rock untouched
Then two crushable medium rocks take lethal damage: debris burst, actor gone (the crushable death-by-damage path).
Results: Saved/DesertRocks/crush/result.json + PNGs.
"""
''' + s[a:]
rep("ROOT = 'D:/UE5Projects/RTS_Survival/Saved/OreMountains/rocks/'", "ROOT = 'D:/UE5Projects/RTS_Survival/Saved/DesertRocks/crush/'")
rep("OUT = ROOT + 'test/result.json'", "OUT = ROOT + 'result.json'")
rep("BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Blueprints/'",
    "BP_DIR = '/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Blueprints/'")
between("LANES = {", "LOOK_Y = 6000.0", '''LANES = {
    'A_medium_light': (-9000, 'BP_DesertRock_M4_Cracked', 'Luchs(Light)', 'detour'),
    'B_medium_medium': (-6000, 'BP_DesertRock_M1_Block', 'PzIV_G(Medium)', 'crush'),
    'C_medium_medium': (-3000, 'BP_DesertRock_M7_Long', 'PzIV_G(Medium)', 'crush'),
    'D_large_medium': (0, 'BP_DesertRock_L1_Block', 'PzIV_G(Medium)', 'detour'),
}
KILL = {'E_kill_medium': (3000, 'BP_DesertRock_M6_Cube'), 'F_kill_medium': (4000, 'BP_DesertRock_M8_Wedge')}
ROCKS = []
''')
rep("CLIFF_PIECES = (('SM_OreCliff_Block2', 1900, LOOK_Y - 200, 20), ('SM_OreCliff_Chunk3', 2900, LOOK_Y + 500, -30))",
    "CLIFF_PIECES = ()")
rep("S['origins']['A_small_light']", "S['origins']['A_medium_light']")
rep("PAIR_ROCKS = (('M01_Block', 1900, LOOK_Y - 570, 10), ('M06_Chunk', 2570, LOOK_Y + 500, 40))", "PAIR_ROCKS = ()")
rep("HIT = (4600, 'BP_OreRock_M02_Wedge')", "HIT = (5200, 'BP_DesertRock_M2_Slab')")
rep("'/Game/RTS_Survival/Blueprints/Environment/OreMountains/CrushableRocks/Materials/M_OreCrushRock'",
    "'/Game/RTS_Survival/Blueprints/Environment/Desert/DesertRocks/Materials/M_DesertRock'")
between("def look_shots():", "def tick(delta):", '''def look_shots():
    return []


''')
open(p, 'w', encoding='utf-8').write(s)
left = [w for w in ('OreRock_S', 'OreRock_M', 'OreMountains/rocks', 'OreCliff_') if w in s]
print('ok, leftovers:', left)
