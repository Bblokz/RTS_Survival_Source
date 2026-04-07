# Vehicle Async Physics Tick: Force Execution Strategies

## Context Snapshot from Current Implementation

From the current tracked tank async movement setup:

- `UTrackPhysicsMovement::AsyncTick` directly overwrites angular velocity via `ATP_SetAngularVelocityInDegrees` and linear velocity via `ATP_SetLinearVelocity`. This gives excellent path adherence but continuously replaces physically simulated responses. 
- Ground alignment is produced from two line traces in `PerformGroundTrace` and then movement is projected onto the averaged ground normal.
- Path following already computes robust steering/throttle through PID (`UTrackPathFollowingComponent::UpdateDriving`) and sends those values to the movement component.

This means your control layer is strong; the weak point is the actuator model (velocity-setter), especially on slopes and terrain discontinuities.

---

## Strategy 1 — **Acceleration Servo (Force/Torque Motor with Slip-Limited Grip)**

### Core idea
Keep your PID controller output exactly as-is (`Throttle`, `Steering`) but change the actuator from “set velocity now” to “drive toward target acceleration and yaw acceleration,” then apply **bounded force/torque** each async step.

### Why it addresses your symptoms
- Uphill/downhill realism improves because gravity remains part of the dynamics.
- Uneven terrain no longer has its vertical/horizontal velocity obliterated each frame.
- Ice-skating is mitigated by explicit lateral slip damping and traction limits rather than unconstrained additive force.

### Async tick execution model
1. Read rigid body state (velocity, angular velocity, orientation, mass, inertia).
2. Compute desired forward speed from throttle (same current logic).
3. Compute speed error and convert to target longitudinal acceleration (`a_target`).
4. Compute required force `F_long = Mass * a_target` and clamp by traction budget:
   - `|F_long| <= MuLong * NormalLoadEstimated`.
5. Compute lateral slip velocity (body-right component of velocity) and apply damping force:
   - `F_lat = -C_lat * V_lat` (clamped).
6. Compute yaw rate error (`YawRateTarget - CurrentYawRate`) and apply torque motor:
   - `TauYaw = Izz * aYawTarget`, clamped.
7. Apply `AddForce`/`AddTorqueInRadians` at COM in async physics.

### Terrain contact notes
- Keep your two traces, but also compute a confidence value: both hits valid + similar normals + valid hit distances.
- If confidence is low, reduce force authority (blend toward conservative mode) to avoid jitter and sudden body snaps.

### PID compatibility
Treat path-following PID output as **command intent**, not direct state assignment:
- `Throttle` maps to desired speed / drive force request.
- `Steering` maps to desired yaw rate / differential track torque request.

This usually keeps your tuned path behavior while making body response physically plausible.

---

## Strategy 2 — **Hybrid Constraint Controller (Soft Velocity Target + Physical Residual Forces)**

### Core idea
Do not jump directly from hard velocity override to pure force drive in one step. Use a hybrid:

- A **soft velocity servo** (low-gain, limited correction) handles precise controllability.
- A **physical residual layer** (gravity-preserving force + anti-slip force + yaw torque) handles realism and terrain reaction.

### Why it addresses your symptoms
- You preserve the tight control quality of your existing setup.
- You reduce unrealistic “gravity cancellation” by limiting direct velocity correction bandwidth.
- You avoid skating by pairing limited correction with contact-aware friction damping.

### Async tick execution model
1. Compute desired linear velocity in the ground tangent plane (as you do today).
2. Compute velocity error `V_err = V_desired - V_current`.
3. Convert only part of this error to correction acceleration:
   - `a_corr = Clamp(KpVel * V_err + KdVel * dV_err, MaxAccelCorrection)`.
4. Apply correction as force (`Mass * a_corr`) instead of direct set velocity.
5. Add residual forces:
   - Lateral anti-slip damping.
   - Hill-hold assistance at low commanded speed (small force along uphill direction only when commanded throttle is near zero).
6. For rotation, same concept:
   - soft yaw-rate correction torque + steering feed-forward torque.

### Key tuning levers
- `MaxAccelCorrection`: start small; this prevents “teleport-like” velocity behavior.
- `LateralDamping`: increase until skating disappears, then back off ~15%.
- `YawTorqueLimit`: cap to prevent flip/jitter on rough terrain.

### PID compatibility
Your existing PID can remain almost untouched. It still outputs throttle/steering; this layer simply changes how that command reaches Chaos.

---

## Strategy 3 — **Contact-Point Track Model (Per-Track Force Application + Grounded State Machine)**

### Core idea
Move from single-body force application to a simplified per-track contact model:

- Solve left/right track forces independently.
- Apply forces at left/right contact points (or approximated sockets).
- Introduce a small movement state machine for grounded confidence and stuck recovery modes.

### Why it addresses your symptoms
- Per-track forces naturally produce yaw from force differential and improve slope interaction.
- Force-at-point produces believable pitch/roll effects over uneven terrain.
- Stuck behavior becomes more controllable because you can explicitly switch modes (normal traction, low-confidence traction, unstuck pulse).

### Async tick execution model
1. Trace contact under left and right tracks separately (not just front/back centerline).
2. Derive per-track normal load estimate.
3. Convert `Throttle` and `Steering` into left/right drive demands:
   - `DriveLeft = Throttle - Steering * TurnMix`
   - `DriveRight = Throttle + Steering * TurnMix`
4. For each track:
   - Compute desired track-ground relative speed.
   - Compute slip ratio.
   - Convert to longitudinal force through a slip curve (piecewise linear is fine).
   - Apply force at track contact point.
5. Apply lateral scrub resistance per track to reduce sideways drift.
6. Blend modes via grounded state machine:
   - `FullyGrounded`: full authority.
   - `PartiallyGrounded`: reduced force/torque and higher damping.
   - `Ungrounded`: no drive force; only stabilization torques.

### Stuck mitigation
When detected stuck but commanded throttle exists:
- Trigger controlled “traction pulse” windows (short boosted force with cooldown).
- Temporarily widen trace footprint / increase suspension probe length.
- Reduce steering authority during pulse to avoid digging in place.

### PID compatibility
This best preserves your current steering/throttle semantics because they map directly to differential track intent. It is the highest-effort option but usually gives the best “heavy tracked vehicle” feel.

---

## Recommended Rollout Plan

1. **Implement Strategy 2 first** (fastest risk-controlled migration).
2. Add telemetry for:
   - slope angle,
   - grounded confidence,
   - longitudinal slip,
   - lateral slip,
   - applied force/torque saturation.
3. If realism gap remains, migrate to **Strategy 3** for tracked-vehicle fidelity.
4. Keep Strategy 1 as fallback architecture for non-tracked vehicles and as a simpler shared motor model.

---

## Practical Guardrails (Important)

- Never combine unrestricted direct velocity override with unrestricted additive force in the same frame.
- Any actuator should be saturating and rate-limited (force, torque, accel).
- In low-contact-confidence frames, reduce authority rather than “fighting terrain.”
- Keep gravity fully owned by Chaos; movement should add intent, not replace world physics.
