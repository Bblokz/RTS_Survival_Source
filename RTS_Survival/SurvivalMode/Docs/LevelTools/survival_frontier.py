"""Round 5 crop (owner: "too large"): the playable square shrinks to PLAY. Heights for the cropped playable square: inside unchanged, outside a dune rim rising to a crest, then the landscape edge
band down to EDGE_Z where the flat horizon skirt starts. Same maths as survival_terrain_gen.Generator.frontier()."""
import numpy as np
PLAY = (-69400.0, 66200.0, -64400.0, 71200.0)  # round 5, set from the island footprint plus wave-entry room
EDGE_Z = 470.0
BAND = 900.0
HALF = 75600.0

def value_noise_1d(t, seed):
    rng = np.random.default_rng(seed)
    knots = rng.random(4096)
    i = np.floor(t).astype(int); f = t - i
    a = knots[i % 4096]; b = knots[(i + 1) % 4096]
    f = f * f * (3 - 2 * f)
    return a + (b - a) * f

def periodic_noise(theta, wavelength_cm, radius_cm, seed):
    """Value noise along the rim, periodic once round the map so the seam of the angle does not show."""
    n = max(4, int(round(2.0 * np.pi * radius_cm / wavelength_cm)))
    rng = np.random.default_rng(seed)
    knots = rng.random(n)
    t = (theta + np.pi) / (2.0 * np.pi) * n
    i = np.floor(t).astype(int); f = t - i
    f = f * f * (3 - 2 * f)
    return knots[i % n] + (knots[(i + 1) % n] - knots[i % n]) * f


def frontier(Z, X, Y, play=PLAY, half=HALF):
    """Round 6 rim: separate dune humps along a wobbling crest line, rounded corners, inner slopes long and lee slip faces
    steep, tallest behind Dune Ridge (+Y). Inside the playable square nothing changes."""
    x0, x1, y0, y1 = play
    cx, cy = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    hx, hy = (x1 - x0) / 2.0, (y1 - y0) / 2.0
    dx = np.maximum(np.maximum(x0 - X, X - x1), 0.0)
    dy = np.maximum(np.maximum(y0 - Y, Y - y1), 0.0)
    dout = np.hypot(dx, dy)
    # Distance outside a rounded square (corner radius 80 m): the crest bends round the corners instead of mitring.
    corner = 8000.0
    qx = np.maximum(np.abs(X - cx) - (hx - corner), 0.0)
    qy = np.maximum(np.abs(Y - cy) - (hy - corner), 0.0)
    dr = np.maximum(np.hypot(qx, qy) - corner, 0.0)
    theta = np.arctan2(Y - cy, X - cx)
    # Crest line wobbles in plan by about +-18 m with a wavelength of about 450 m.
    crest_at = (2700.0 + 1300.0 * (2.0 * periodic_noise(theta, 45000.0, 70000.0, 21) - 1.0)
                + 700.0 * (2.0 * periodic_noise(theta, 11000.0, 70000.0, 24) - 1.0))  # round 7: also a 110 m wobble
    # Separate humps 2-8 m high, about 150-250 m apart, some saddles nearly flat.
    hump = periodic_noise(theta, 17000.0, 70000.0, 22) * 0.7 + periodic_noise(theta, 6000.0, 70000.0, 23) * 0.3
    height = 200.0 + 650.0 * np.clip((hump - 0.25) / 0.6, 0.0, 1.0) ** 1.4
    height = height * (1.0 + 0.35 * np.clip((Y - (y1 - 2000.0)) / 4000.0, 0.0, 1.0))  # Dune Ridge's rear rim
    # Round 7: saddles every 40-80 m so the rim breaks into separate crests inside one camera frame.
    saddle = periodic_noise(theta, 5500.0, 70000.0, 25)
    height = height * (0.3 + 0.7 * np.clip((saddle - 0.2) / 0.55, 0.0, 1.0))
    u = dr - crest_at
    inner = np.clip((u + 2600.0) / 2600.0, 0.0, 1.0)
    inner = inner * inner * (3.0 - 2.0 * inner)
    outer = 1.0 - 0.75 * np.clip(u / 900.0, 0.0, 1.0) ** 1.5
    rise = height * np.where(u < 0.0, inner, outer)
    rise = rise * np.clip(dout / 1500.0, 0.0, 1.0)  # zero at the playable edge, so nothing inside moves
    Z = Z + rise
    edge = np.minimum(half - np.abs(X), half - np.abs(Y))
    t = np.clip(edge / BAND, 0.0, 1.0)
    return EDGE_Z + (Z - EDGE_Z) * (t * t * (3.0 - 2.0 * t)), dout


# Round 6: the gaps between neighbouring islands are the map's chokes; three were under 25 m of drivable ground.
def smooth(t): t=np.clip(t,0,1); return t*t*(3-2*t)
def apply_corridors(Z, X, Y, lines, half_width=1400.0, feather=1400.0):
    """Lowers the ground along each line to its floor height (never raises): full within half_width, feathered beyond."""
    for pts in lines:
        P=np.array(pts,float)
        # smooth the floor heights along the line
        zf=np.convolve(np.pad(P[:,2],(2,2),mode='edge'),np.ones(5)/5,mode='valid')
        x0,x1=P[:,0].min()-half_width-feather-200, P[:,0].max()+half_width+feather+200
        y0,y1=P[:,1].min()-half_width-feather-200, P[:,1].max()+half_width+feather+200
        win=(Y[:,0]>=y0)&(Y[:,0]<=y1); wcol=(X[0]>=x0)&(X[0]<=x1)
        js=np.where(win)[0]; is_=np.where(wcol)[0]
        sub=(slice(js[0],js[-1]+1),slice(is_[0],is_[-1]+1))
        px,py=X[sub],Y[sub]
        best=np.full(px.shape,1e18); fz=np.zeros(px.shape)
        for k in range(len(P)-1):
            ax,ay=P[k,0],P[k,1]; bx,by=P[k+1,0],P[k+1,1]
            ex,ey=bx-ax,by-ay; L2=max(ex*ex+ey*ey,1e-9)
            t=np.clip(((px-ax)*ex+(py-ay)*ey)/L2,0,1)
            d=np.hypot(px-(ax+t*ex),py-(ay+t*ey))
            closer=d<best
            best=np.where(closer,d,best); fz=np.where(closer,zf[k]+(zf[k+1]-zf[k])*t,fz)
        s=smooth((best-half_width)/feather)
        target=fz+(Z[sub]-fz)*s
        # The corridor floor itself is set (craters and humps inside it go); beyond it the ground is only lowered.
        Z[sub]=np.where(best<half_width, fz, np.minimum(Z[sub],target))
    return Z
