# Vehicle Async Physics Tick: 3 Realistic but Controlled Force-Execution Strategies

## 1) Current Setup Analysis (from your code)

Your current tracked movement path is split correctly into:

- **Command generation** (path following + PID): throttle/steering intent is already stable.
- **Actuation** (`UTrackPhysicsMovement::AsyncTick`): currently does hard state assignment.

From the implementation:

- Async tick sets yaw with `ATP_SetAngularVelocityInDegrees`.
- Async tick sets linear velocity with `ATP_SetLinearVelocity` after projecting desired velocity onto the traced ground plane.
- Ground orientation comes from two traces and averaged normal (`PerformGroundTrace`).

That architecture explains your observed behavior:

1. **Slope realism issue**: direct velocity overwrite cancels natural gravity acceleration/deceleration every async frame.
2. **Uneven-terrain sticking**: authority is concentrated into a single projected velocity target, while local contact quality can change rapidly.
3. **“Ice skating” during AddForce experiments**: forces likely lacked a traction budget + lateral slip damping + saturation/rate limiting.

---

## Strategy A — Acceleration Servo Motor (Force/Torque Only, No Direct Velocity Set)

### Concept
Keep your existing PID output semantics untouched, but reinterpret them as **target acceleration / yaw acceleration intent**.

### Async physics execution
1. Read body state (forward speed, yaw rate, mass/inertia, contact normal).
2. Convert throttle to `TargetSpeed` (same as today).
3. Compute speed error -> desired longitudinal accel.
4. Compute drive force `F = Mass * Accel`, then **clamp by traction limit**.
5. Compute yaw-rate error from steering -> desired yaw torque, also clamped.
6. Add lateral slip damping force on the body-right axis.
7. Apply `AddForce` + `AddTorque` in async tick.

### Why this works
- Gravity remains fully active, so hills feel natural.
- PID remains useful because command meaning is preserved.
- Skating is controlled through lateral damping + force saturation.

### Minimal designer variables (keep simple)
Use only **3 knobs**:
- `MaxDriveAccel`
- `MaxYawAccel`
- `LateralDamping`

Everything else can be internally derived from mass, gravity, and existing movement multipliers.

---

## Strategy B — Hybrid Soft-Velocity Servo (Recommended First Rollout)

### Concept
Transitional architecture: retain a controlled amount of velocity error correction, but execute correction as **bounded acceleration force**, not hard velocity assignment.

### Async physics execution
1. Build desired velocity on ground tangent plane (same geometric logic you already trust).
2. Compute velocity error.
3. Convert error to correction acceleration with low gain.
4. Clamp correction acceleration magnitude.
5. Apply as force (`Mass * AccelCorrection`).
6. Add yaw torque servo (bounded) and lateral anti-slip damping.
7. If ground-trace confidence is low, scale authority down instead of fighting terrain.

### Why this works
- You keep near-current path adherence.
- You stop hard-cancelling gravity.
- Much lower migration risk than jumping straight to full per-track modeling.

### Minimal designer variables
Use **3 knobs**:
- `VelocityCorrectionGain`
- `MaxCorrectionAccel`
- `LateralDamping`

This is usually enough because your path follower already does most high-level control.

---

## Strategy C — Per-Track Contact Force Model + 3-State Ground Confidence

### Concept
Apply left/right track forces at track contact points so yaw and slope behavior emerge from force distribution, not body-level overrides.

### Async physics execution
1. Do per-track contact traces (left and right).
2. Convert throttle + steering into left/right drive requests.
3. For each track, compute longitudinal slip and generate force from a simple slip curve.
4. Apply per-track lateral scrub damping.
5. Run a tiny grounded-state execution policy:
   - `FullyGrounded`: normal authority.
   - `PartiallyGrounded`: reduced drive authority + stronger damping.
   - `Ungrounded`: no drive force; stabilization only.

### Why this works
- Best realism on uneven terrain.
- Better natural turning feel for tracked vehicles.
- Gives explicit anti-stuck behavior by state-dependent force limits.

### Minimal designer variables
Keep it to **4 knobs**:
- `TrackDriveForceMax`
- `TrackSlipAtMaxForce`
- `TrackLateralDamping`
- `PartialGroundedAuthorityScale`

(Everything else derived internally.)

---

## Recommended Adoption Order

1. **Start with Strategy B** (best control/risk ratio).
2. If skating persists, add Strategy A-style stricter traction clamping.
3. If tracked realism is still insufficient, move to Strategy C.

---

## Practical Implementation Rules (important for all 3)

1. Never mix unlimited velocity overwrite with unlimited additive forces in the same frame.
2. Clamp **every** actuator output (longitudinal force, lateral force, yaw torque).
3. Rate-limit command changes to avoid force spikes on trace jitter.
4. Decrease control authority when trace confidence is low.
5. Keep gravity fully owned by Chaos; your controller should add intent, not replace physics.

---

## A lightweight anti-stuck policy (no extra designer burden)

Use internal logic (no new tunables required initially):

- If throttle command is significant but forward speed stays near zero for a short window,
- and slope/contact indicates partial grounding,
- then apply a short traction pulse with reduced steering authority,
- followed by cooldown.

This solves many “stuck on uneven seam” cases without exposing more settings.
