# Lit editor-viewport pictures of the landing field (editor draws decals/water dark; judge those in game).
exec(open('D:/UE5Projects/RTS_Survival/Source/Tools/IntroMission/landing/landing_lib.py').read())
TAG = globals().get('SHOT_TAG', 'r0')
def P(u, v, z):
    x, y = W(u, v); return (x, y, z)
views = [
    ('three_quarter', P(-1500, 9500, 5200), P(0, 0, 150)),
    ('gate', P(-3200, 7200, 1500), P(-1500, 2600, 300)),
    ('top', P(0, 300, 9000), P(0, 0, 140)),
    ('east_low', P(7800, -2500, 1500), P(0, 0, 300)),
]
_defer = run_steps([view_shot('%s_%s' % (TAG, n), e, t) for n, e, t in views])
