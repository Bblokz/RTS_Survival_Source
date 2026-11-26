# E6 Crystal_2 slot reachability (LightTank, game projection 850 + full path from the HQ); FC_DROP = nav box names to sink
import json, math
import unreal
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/fields/fc_nav_test.py').read().split('_res = {}')[0])
import importlib, sys
sys.path.insert(0, "D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/fields")
import fc_build as _fb
importlib.reload(_fb)
_fb.build_bp("E6", skip_boxes=FC_DROP)
_fb.place("E6")


def _slots():
    p = [q for q in _spec['E6']['pieces'] if q['label'].endswith(FC_PIECE)][0]
    lt = _navs['LightTank']
    out = []
    for k in range(8):
        a = math.radians(45 * k)
        sx, sy = p['x'] + math.cos(a) * p['rr'], p['y'] + math.sin(a) * p['rr']
        qg = _project(lt, unreal.Vector(sx, sy, _ground(sx, sy) + 50), 850)
        out.append(bool(qg and _path(lt, _HQ, qg)[0]))
    return out


exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/BerlinMap/poi/fields/fc_wait_nav.py').read())
_inner = _defer
_st2 = {"n": 0}


def _then():
    _st2["n"] += 1
    if _st2["n"] < 120:          # give the dynamic obstacle update a few seconds of ticks first
        return False
    if not _inner():
        return False
    print(FC_PIECE, 'slots reachable', _slots())
    return True


_defer = _then
