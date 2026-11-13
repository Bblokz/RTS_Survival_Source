// Custom HLSL of the small arms muzzle emitters (NS_SmallArmsMuzzle), split into sections that
// RTS.MuzzleVfx.PatchNiagaraSystem joins per emitter; the code is written into the emitters' HLSL nodes.
// Spawn node:  SpawnBegin + <Layer> Shape + SpawnEnd + ReadShot + <Layer> Place
// Update node: UpdateBegin + ReadShot + <Layer> Place
// Inputs: ShotPositionAndScale / ShotDirection (float4 arrays, the shots of this frame of all groups behind each
// other), TypeOffset / TypeCount (where this emitter's group starts and how many shots it has), ExecCount
// (particles spawned this frame), and MuzzleOriginIn / MuzzleAxisIn / MuzzleShapeIn (the particle's attributes as
// read before the node; the update must read them this way, see the C++).
// The CPU VM has no smoothstep or step: layers write them out as saturate(...) and comparisons.
// Space: X is the fire direction, Y right, Z up; S is the shot's scale (1 = a bolt-action rifle).

// @section SpawnBegin
// Random numbers R0..R3 (0 to 1) per particle; the CPU VM has no unsigned integer maths for a hash.
float R0 = rand(1.0);
float R1 = rand(1.0);
float R2 = rand(1.0);
float R3 = rand(1.0);
// Every shot of this emitter's group gets the same share of the particles spawned this frame.
int ShotIndex = TypeOffset + (ExecIndex() * TypeCount) / max(ExecCount, 1);
// Left without a start value on purpose: the VM compiler refuses a constant as an out argument.
float4 ShotPS;
float4 ShotDir;
ShotPositionAndScale.Get(ShotIndex, ShotPS);
ShotDirection.Get(ShotIndex, ShotDir);
Particles.MuzzleOrigin = ShotPS.xyz;
Particles.MuzzleAxis = float4(normalize(ShotDir.xyz + float3(0.0001, 0.0, 0.0)), max(ShotPS.w, 0.05));
float LifetimeMlt = max(ShotDir.w, 0.1);

// @section SpawnEnd
// No shot for this group (should not happen, the spawn count follows the shots): the particle dies at once.
Particles.Lifetime = TypeCount > 0 ? Particles.Lifetime : 0.0;
float3 OriginValue = Particles.MuzzleOrigin;
float4 AxisValue = Particles.MuzzleAxis;
float4 ShapeValue = Particles.MuzzleShape;
float AgeSeconds = 0.0;
float T = 0.0;

// @section UpdateBegin
float3 OriginValue = MuzzleOriginIn;
float4 AxisValue = MuzzleAxisIn;
float4 ShapeValue = MuzzleShapeIn;
float AgeSeconds = Particles.Age;
float T = saturate(Particles.NormalizedAge);

// @section ReadShot
float3 Fwd = AxisValue.xyz;
float S = AxisValue.w;
float3 RefUp = abs(Fwd.z) < 0.98 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
float3 Right = normalize(cross(RefUp, Fwd));
float3 Up = cross(Fwd, Right);
float4 Shape = ShapeValue;
// The shot's rotation as a quaternion (yaw and pitch, no roll), the same as FRotator::Quaternion.
float HalfYaw = atan2(Fwd.y, Fwd.x) * 0.5;
float HalfPitch = asin(clamp(Fwd.z, -1.0, 1.0)) * 0.5;
float4 ShotQuat = float4(sin(HalfPitch) * sin(HalfYaw), -sin(HalfPitch) * cos(HalfYaw),
                         cos(HalfPitch) * sin(HalfYaw), cos(HalfPitch) * cos(HalfYaw));
