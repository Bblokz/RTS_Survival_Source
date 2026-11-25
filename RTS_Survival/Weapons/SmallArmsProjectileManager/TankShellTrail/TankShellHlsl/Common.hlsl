// Custom HLSL of the tank shell trail emitters (NS_TankShellTrail), drawn by UTankShellTrailBatcher and
// joined per emitter by RTS.TankShellTrail.BuildNiagaraSystem (see SmallArmsMuzzleNiagaraAuthoring.cpp):
// Spawn node:  SpawnBegin + Style (Style.hlsl) + <Layer> Shape + SpawnEnd + ReadShot + <Layer> Place
// Update node: UpdateBegin + Style + ReadShot + <Layer> Place
// Inputs: ShotPositionAndScale (xyz = a point the shell flew through, w = shell kind * 10 + calibre scale) and
// ShotDirection (xyz = flight direction, w = seconds since the shell passed the point), TypeOffset / TypeCount,
// ExecCount, and MuzzleOriginIn / MuzzleAxisIn / MuzzleShapeIn (the particle attributes as read before the node).
// A Shape section sets LifeFull (seconds); Place sets position, size, rotation and colour from AgeSeconds and T.
// The CPU VM has no smoothstep or step. Kind: 0 AP, 1 HE, 2 HEAT, 3 APCR, 4 fire. S: 1 = a 75 mm shell.

// @section SpawnBegin
float R0 = rand(1.0);
float R1 = rand(1.0);
float R2 = rand(1.0);
float R3 = rand(1.0);
int ShotIndex = TypeOffset + (ExecIndex() * TypeCount) / max(ExecCount, 1);
// Left without a start value on purpose: the VM compiler refuses a constant as an out argument.
float4 ShotPS;
float4 ShotDir;
ShotPositionAndScale.Get(ShotIndex, ShotPS);
ShotDirection.Get(ShotIndex, ShotDir);
float SpawnKind = floor(ShotPS.w / 10.0);
float AgeOffset = max(ShotDir.w, 0.0);
float LifeFull = 0.0;

// @section SpawnEnd
float3 OriginValue = ShotPS.xyz;
float4 AxisValue = float4(normalize(ShotDir.xyz + float3(0.0001, 0.0, 0.0)), ShotPS.w);
// x, y: random numbers of the particle; z: how old it was when spawned; w: its full life.
float4 ShapeValue = float4(R1, R2, AgeOffset, max(LifeFull, 0.0001));
Particles.MuzzleOrigin = OriginValue;
Particles.MuzzleAxis = AxisValue;
Particles.MuzzleShape = ShapeValue;
// The particle is born a part of a frame old; no sample for this group: it dies at once.
Particles.Lifetime = TypeCount > 0 ? max(LifeFull - AgeOffset, 0.0) : 0.0;
float AgeSeconds = AgeOffset;

// @section UpdateBegin
float3 OriginValue = MuzzleOriginIn;
float4 AxisValue = MuzzleAxisIn;
float4 ShapeValue = MuzzleShapeIn;
float AgeSeconds = Particles.Age + ShapeValue.z;

// @section ReadShot
float T = saturate(AgeSeconds / ShapeValue.w);
float3 Fwd = AxisValue.xyz;
float Kind = floor(AxisValue.w / 10.0);
float S = AxisValue.w - Kind * 10.0;
float3 RefUp = abs(Fwd.z) < 0.98 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
float3 Right = normalize(cross(RefUp, Fwd));
float3 Up = cross(Fwd, Right);
float Rx = ShapeValue.x;
float Ry = ShapeValue.y;
// A third random number from the two stored ones.
float Rz = frac(Rx * 7.31 + Ry * 3.77);
float IsHe = (Kind > 0.5 && Kind < 1.5) ? 1.0 : 0.0;
